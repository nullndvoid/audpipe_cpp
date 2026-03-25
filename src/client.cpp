#include "client.hxx"
#include "audio.hxx"
#include "rtp.hxx"

std::pair<std::function<void(void *, uvgrtp::frame::rtp_frame *)>, void *>
Client::make_recv_callback(Client *self) {
  return {[](void *userdata, uvgrtp::frame::rtp_frame *frame) {
            auto *self = static_cast<Client *>(userdata);
            self->logger->debug("Got RTP frame of size {}", frame->dgram_size);
          },
          self};
}

Client::Client(std::string local_address, uint16_t local_port,
               uint16_t remote_port)
    : local_address(std::move(local_address)), logger(spdlog::get("audpipe")),
      rtp(this->local_address, local_port, remote_port,
          make_recv_callback(this)) {
  auto &audio = AudioBackend::instance();
  audio.set_write_callback(
      [&](uint8_t *dst, size_t max_len) mutable { return 0; });
  audio.create_virtual_input();
}