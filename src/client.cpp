#include "client.hxx"
#include "audio.hxx"
#include "rtp.hxx"

void Client::recv_callback(void *userdata, uvgrtp::frame::rtp_frame *frame) {
  auto *self = static_cast<Client *>(userdata);
  self->logger->debug("Got RTP frame of size {}", frame->dgram_size);
}

std::pair<Client::recv_hook_t, void *>
Client::make_recv_callback(Client *self) {
  return {&Client::recv_callback, self};
}

Client::Client(std::pair<std::string, uint16_t> local_socket,
               std::pair<std::string, uint16_t> remote_socket)
    : logger(spdlog::get("audpipe")),
      rtp(local_socket, remote_socket, make_recv_callback(this)) {
  auto &audio = AudioBackend::instance();
  audio.set_write_callback(
      [&](uint8_t *dst, size_t max_len) mutable { return 0; });
  audio.create_virtual_input();
}