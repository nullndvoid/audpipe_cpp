#include "server.hxx"

#include <algorithm>

#include <cstdint>
#include <spdlog/spdlog.h>

#include <utility>
#include <uvgrtp/lib.hh>

#include "audio.hxx"
#include "rtp.hxx"

Server::Server(std::string local_address, uint16_t local_port,
               uint16_t remote_port)
    : local_address(std::move(local_address)) {
  this->logger = spdlog::get("audpipe");

  auto rtp = Rtp(local_address, local_port, remote_port, RTP_SEND);

  auto &audio = AudioBackend::instance();
  auto inputs = audio.get_inputs();

  audio.set_data_callback([&](const uint8_t *data, size_t len) {
    // TODO: Encode to Opus and send via RTP.
    this->logger->debug("Received {} bytes of audio data.", len);
  });

  // Find first monitor input. TODO: Config file and setup wizard maybe?
  auto monitor = std::ranges::find_if(
      inputs, [&](AudioDevice &dev) { return dev.is_monitor; });

  this->logger->info("First monitor device found is \'{}\'.",
                     monitor->description);

  audio.record(*monitor.base());
}