#include "client.hxx"
#include "audio.hxx"

#include "opus_types.h"
#include "uvgrtp/frame.hh"
#include <cstdint>
#include <exception>
#include <format>
#include <stdexcept>
#include <thread>

void Client::recv_callback(void *userdata, uvgrtp::frame::rtp_frame *frame) {
  auto *self = static_cast<Client *>(userdata);
  if (self == nullptr || frame == nullptr)
    return;

  if (self->should_stop.load() || self->state.load() == ClientState::ERROR) {
    auto err_code = uvgrtp::frame::dealloc_frame(frame);
    if (err_code != RTP_OK) {
      self->logger->critical("Frame deallocation failed with code {}",
                             static_cast<int8_t>(err_code));
    }

    return;
  }

  self->logger->debug("Got RTP frame of size {}", frame->dgram_size);
  auto err_code = uvgrtp::frame::dealloc_frame(frame);
  if (err_code != RTP_OK) {
    self->logger->critical("Frame deallocation failed with code {}",
                           static_cast<int8_t>(err_code));
  }
}

std::pair<Client::recv_hook_t, void *>
Client::make_recv_callback(Client *self) {
  return {&Client::recv_callback, self};
}

// State machine goes roughly in the order as in the markdown file and header.
// IDLE -> AUDIO_SETUP -> RX_READY
Client::Client(std::pair<std::string, uint16_t> local_socket,
               std::pair<std::string, uint16_t> remote_socket)
    : logger(spdlog::get("audpipe")) {
  this->state = ClientState::AUDIO_SETUP;

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
  audio.set_write_callback(
      [&](uint8_t *dst, size_t max_len) mutable { return 0; });

  this->virtual_input_thread =
      std::thread(Client::call_virtual_input_setup, this);

  // TODO: Handle server not being started or reachable.
  try {
    this->rtp.emplace(local_socket, remote_socket, make_recv_callback(this));
  } catch (const std::exception &e) {
    auto error_msg =
        std::format("Failed to initialise RTP. Reason: {}", e.what());
    set_error(error_msg);

    throw std::runtime_error(error_msg);
  }

  this->state = ClientState::RX_READY;
}

Client::~Client() {
  if (this->virtual_input_thread.joinable()) {
    this->virtual_input_thread.join();
  }
}

void Client::set_error(const std::string &err) {
  ClientState expected = this->state.load();
  if (expected == ClientState::ERROR || expected == ClientState::STOP) {
    return;
  }

  this->state = ClientState::ERROR;
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