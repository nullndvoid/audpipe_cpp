#include "client.hxx"
#include "audio.hxx"
#include "rtp.hxx"

Client::Client(std::string local_address, uint16_t local_port,
               uint16_t remote_port)
    : local_address(std::move(local_address)) {
  this->logger = spdlog::get("audpipe");

  auto rtp = Rtp(this->local_address, local_port, remote_port, RTP_RECV);

  auto &audio = AudioBackend::instance();
  auto inputs = audio.get_inputs();

  audio.set_data_callback([&](const uint8_t *data, size_t len) {
    // TODO: Encode to Opus and send via RTP.
    this->logger->debug("Received {} bytes of audio data.", len);
  });

}