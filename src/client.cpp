#include "client.hxx"
#include "audio.hxx"
#include "rtp.hxx"

#include <cstdint>
#include <cstring>

Client::Client(std::string local_address, uint16_t local_port,
               uint16_t remote_port)
    : local_address(std::move(local_address)) {
  this->logger = spdlog::get("audpipe");

  auto rtp = Rtp(this->local_address, local_port, remote_port, RTP_RECV);

  auto &audio = AudioBackend::instance();

  audio.set_write_callback(
      [&](uint8_t *dst, size_t max_len) mutable { return 0; });

  audio.create_virtual_input();
}