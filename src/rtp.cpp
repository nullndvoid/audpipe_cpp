#include "rtp.hxx"

#include <cstdint>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include <uvgrtp/lib.hh>

uvgrtp::context Rtp::ctx;

Rtp::Rtp(std::string &local_addr, uint16_t local_port, uint16_t remote_port,
         rtp_mode_t mode)
    : mode(mode) {
  this->logger = spdlog::get("audpipe");
  this->session = ctx.create_session(local_addr);

  if (this->session == nullptr) {
    auto error_str = "Failed to create uvgRTP session. Must be OOM.";
    this->logger->critical(error_str);
    throw std::runtime_error(error_str);
  }

  int flags;
  if (this->mode == RTP_SEND) {
    flags = RCE_SEND_ONLY;
  } else {
    flags = RCE_RECEIVE_ONLY;
  }

  this->stream = this->session->create_stream(local_port, remote_port,
                                              RTP_FORMAT_OPUS, flags);

  if (this->stream == nullptr) {
    auto error_str = "Failed to create opus RTP stream.";
    this->logger->critical(error_str);
    throw std::runtime_error(error_str);
  }

  this->logger->info("RTP crypto enabled? {}", ctx.crypto_enabled());
}

void Rtp::write_frames() {}
