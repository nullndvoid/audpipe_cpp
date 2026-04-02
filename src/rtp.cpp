#include "rtp.hxx"
#include "shutdown.hxx"

#include <asio/error_code.hpp>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

uvgrtp::context Rtp::ctx;

constexpr int DEFAULT_SEND_FLAGS =
    RCE_RTCP | RCE_SYSTEM_CALL_CLUSTERING | RCE_SEND_ONLY;

constexpr int DEFAULT_RECV_FLAGS = RCE_RTCP;

// Confusingly the server connects to the client, this is my poor naming.
Rtp::Rtp(std::pair<std::string, uint16_t> local_socket,
         std::pair<std::string, uint16_t> remote_socket) {
  this->logger = spdlog::get("audpipe");

  this->session =
      ctx.create_session(std::pair(local_socket.first, remote_socket.first));

  handshake_server();

  init_rtp(this, true, local_socket.first, local_socket.second,
           remote_socket.second);
}

Rtp::Rtp(std::pair<std::string, uint16_t> local_socket,
         std::pair<std::string, uint16_t> remote_socket,
         std::pair<recv_hook_t, void *> cb) {
  this->logger = spdlog::get("audpipe");

  this->recv_callback = cb;

  this->session =
      ctx.create_session(std::pair(local_socket.first, remote_socket.first));

  handshake_client();

  init_rtp(this, false, local_socket.first, local_socket.second,
           remote_socket.second);
}

void asio_signal_handler(const asio::error_code &error, int signum) {
  if (!error) {
    request_shutdown();
  }
}

void Rtp::write_frames(uint8_t *data, size_t data_len) {
  auto lock = std::scoped_lock(this->teardown_mutex);

  if (this->stopped.load() || this->stream == nullptr) {
    return;
  }

  rtp_error_t err = this->stream->push_frame(data, data_len, RCC_NO_FLAGS);

  if (err != RTP_OK) {
    this->logger->error("Failed to write RTP frame with error code: {}",
                        static_cast<int8_t>(err));
    // TODO: Check if recoverable at all.
  }

  this->logger->debug("Wrote frame of length {}", data_len);
}

void Rtp::init_rtp(Rtp *rtp, bool sending, std::string &local_addr,
                   uint16_t local_port, uint16_t remote_port) {
  assert(ctx.crypto_enabled());
  if (!sending) {
    // Invariant: We want the callback to definitely exist before a
    // media_stream is created.
    assert(rtp->recv_callback.first != nullptr);
  }

  // Set asio to respect shutdowns as everywhere else.
  asio::signal_set signals(rtp->io, SIGINT, SIGTERM);

  signals.async_wait(asio_signal_handler);

  if (rtp->session == nullptr) {
    auto error_str = "Failed to create uvgRTP session. Must be OOM.";
    rtp->logger->critical(error_str);
    throw std::runtime_error(error_str);
  }

  int flags;

  if (sending) {
    flags = DEFAULT_SEND_FLAGS;
  } else {
    flags = DEFAULT_RECV_FLAGS;
  };

  rtp->stream = rtp->session->create_stream(local_port, remote_port,
                                            RTP_FORMAT_OPUS, flags);

  if (rtp->stream == nullptr) {
    auto error_str =
        std::format("Failed to create opus RTP stream with error code {}.",
                    static_cast<uint8_t>(rtp_errno));
    rtp->logger->critical(error_str);
    throw std::runtime_error(error_str);
  }

  if (!sending) {
    rtp->stream->install_receive_hook(rtp->recv_callback.second,
                                      rtp->recv_callback.first);
  }
}

Rtp::~Rtp() { stop(); }

void Rtp::stop() {
  auto lock = std::scoped_lock(this->teardown_mutex);
  if (this->stopped.exchange(true)) {
    return;
  }

  if (this->session != nullptr && this->stream != nullptr) {
    this->session->destroy_stream(this->stream);
    this->stream = nullptr;
  }

  if (this->session != nullptr) {
    ctx.destroy_session(this->session);
    this->session = nullptr;
  }
}

bool Rtp::is_initialised() const {
  return !this->stopped.load() && this->session != nullptr &&
         this->stream != nullptr;
}

bool Rtp::is_stopped() const { return this->stopped.load(); }

void Rtp::handshake_client() {
  if (is_shutdown_requested()) {
    throw std::runtime_error("Shutdown requested before client handshake.");
  }

  this->logger->info("Sent client handshake!");
}

void Rtp::handshake_server() {
  if (is_shutdown_requested()) {
    throw std::runtime_error("Shutdown requested before server handshake.");
  }

  this->logger->info("Sent server handshake!");
}
