#include "client.hxx"
#include "audio.hxx"

#include "config.hxx"
#include "opus_types.h"
#include "shutdown.hxx"
#include "uvgrtp/frame.hh"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <stdexcept>
#include <thread>

void Client::recv_callback(void *userdata, uvgrtp::frame::rtp_frame *frame) {
  auto *self = static_cast<Client *>(userdata);
  if (self == nullptr || frame == nullptr)
    return;

  if (self->should_stop.load() || !self->healthy.load()) {
    auto err_code = uvgrtp::frame::dealloc_frame(frame);
    if (err_code != RTP_OK) {
      self->logger->warn("Frame deallocation failed with code {}",
                         static_cast<int8_t>(err_code));
    }

    return;
  }

  if (frame->payload != nullptr && frame->payload_len > 0) {
    auto lock = std::scoped_lock(self->playback_queue_mutex);
    constexpr size_t MAX_COMPRESSED_QUEUE_FRAMES = 256;

    if (self->compressed_queue.size() >= MAX_COMPRESSED_QUEUE_FRAMES) {
      self->compressed_queue.pop_front();
    }

    auto *begin = static_cast<uint8_t *>(frame->payload);
    self->compressed_queue.emplace_back(begin, begin + frame->payload_len);
    self->frames_received.fetch_add(1, std::memory_order_relaxed);
  }

  self->logger->debug("Queued RTP frame of size {}", frame->dgram_size);
  auto err_code = uvgrtp::frame::dealloc_frame(frame);
  if (err_code != RTP_OK) {
    self->logger->warn("Frame deallocation failed with code {}",
                       static_cast<int8_t>(err_code));
  }
}

std::pair<Client::recv_hook_t, void *>
Client::make_recv_callback(Client *self) {
  return {&Client::recv_callback, self};
}

Client::Client(std::pair<std::string, uint16_t> local_socket,
               std::pair<std::string, uint16_t> remote_socket,
               asio::io_context &io, ConnectionPolicy conn_pol)
    : conn_pol(conn_pol), logger(spdlog::get("audpipe")) {
  // Setup opus decoding.
  int error = OPUS_OK;
  this->opusdec = opus_decoder_create(48000, 2, &error);
  if (error != OPUS_OK) {
    auto error_msg = std::format("Failed to create opus decoder. Reason: {}",
                                 opus_strerror(error));
    set_error(error_msg);

    throw std::runtime_error(error_msg);
  }

  this->pcm_decbuf = std::vector<opus_int16>();
  this->pcm_decbuf.resize(PCM_DECBUF_SIZE);

  // Setup audio backend and virtual input.
  auto &audio = AudioBackend::instance();
  audio.set_write_callback([&](uint8_t *dst, size_t max_len) mutable {
    if (dst == nullptr || max_len == 0 || this->should_stop.load() ||
        !this->healthy.load()) {
      return static_cast<size_t>(0);
    }

    size_t written = 0;

    while (written < max_len) {
      {
        auto lock = std::scoped_lock(this->playback_queue_mutex);
        while (written < max_len && !this->playback_queue.empty()) {
          dst[written] = this->playback_queue.front();
          this->playback_queue.pop_front();
          ++written;
        }
      }

      if (written == max_len) {
        break;
      }

      std::vector<uint8_t> compressed_frame;
      {
        auto lock = std::scoped_lock(this->playback_queue_mutex);
        if (this->compressed_queue.empty()) {
          break;
        }

        compressed_frame = std::move(this->compressed_queue.front());
        this->compressed_queue.pop_front();
      }

      auto decoded_samples_per_channel =
          opus_decode(this->opusdec, compressed_frame.data(),
                      static_cast<opus_int32>(compressed_frame.size()),
                      this->pcm_decbuf.data(), 960, 0);

      if (decoded_samples_per_channel < 0) {
        this->logger->warn("Failed to decode opus payload: {}",
                           opus_strerror(decoded_samples_per_channel));
        continue;
      }

      const auto *pcm_bytes =
          reinterpret_cast<const uint8_t *>(this->pcm_decbuf.data());
      size_t decoded_bytes = static_cast<size_t>(decoded_samples_per_channel) *
                             2 * sizeof(opus_int16);

      {
        auto lock = std::scoped_lock(this->playback_queue_mutex);
        this->playback_queue.insert(this->playback_queue.end(), pcm_bytes,
                                    pcm_bytes + decoded_bytes);
      }
    }

    return written;
  });

  for (unsigned attempt = 1; attempt <= this->conn_pol.max_retries; ++attempt) {
    try {
      this->rtp.emplace(local_socket, remote_socket, make_recv_callback(this),
                        io);
      break;
    } catch (const std::exception &e) {
      this->rtp.reset();

      if (attempt == this->conn_pol.max_retries) {
        auto error_msg = std::format(
            "Failed to initialise RTP after {} attempts. Reason: {}", attempt,
            e.what());
        set_error(error_msg);
        throw std::runtime_error(error_msg);
      }

      this->logger->warn("RTP init failed on attempt {}/{}: {}", attempt,
                         this->conn_pol.max_retries, e.what());

      auto backoff = std::chrono::milliseconds(this->conn_pol.retry_backoff_ms);
      auto waited = std::chrono::milliseconds(0);
      constexpr auto poll_step = std::chrono::milliseconds(100);
      while (waited < backoff && !is_shutdown_requested()) {
        std::this_thread::sleep_for(poll_step);
        waited += poll_step;
      }

      if (is_shutdown_requested()) {
        set_error("Shutdown requested during RTP connection backoff.");
        throw std::runtime_error("Shutdown requested.");
      }
    }
  }

  this->virtual_input_thread =
      std::thread(Client::call_virtual_input_setup, this);
}

Client::~Client() {
  if (this->opusdec != nullptr) {
    opus_decoder_destroy(this->opusdec);
    this->opusdec = nullptr;
  }

  if (this->virtual_input_thread.joinable()) {
    this->virtual_input_thread.join();
  }
}

void Client::set_error(const std::string &err) {
  if (!this->healthy.exchange(false)) {
    return;
  }

  this->last_error = err;
  this->should_stop = true;
  this->logger->error("{}", err);

  // Some cleanup.
  AudioBackend::instance().stop_recording();
}

// Just required to call non static member function of AudioBackend.
// We can't throw across thread boundaries, but we could check the error status.
void Client::call_virtual_input_setup(void *a) {
  Client *self = static_cast<Client *>(a);
  AudioBackend &audio = AudioBackend::instance();

  try {
    audio.create_virtual_input();
  } catch (const std::exception &e) {
    auto error_msg =
        std::format("Could not create virtual input, reason: {}", e.what());

    self->set_error(error_msg);
  }
}

bool Client::is_healthy() const { return this->healthy.load(); }

void Client::request_shutdown() {
  this->should_stop = true;

  if (this->rtp.has_value()) {
    this->rtp->stop();
  }

  AudioBackend::instance().stop_recording();
}

size_t Client::get_frames_received() const {
  return this->frames_received.load();
}

Rtp *Client::get_rtp() {
  if (!this->rtp.has_value()) {
    return nullptr;
  }

  return &this->rtp.value();
}