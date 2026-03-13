#include "server.hxx"

#include <algorithm>

#include <cstdint>
#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

#include "audio.hxx"
#include "opus.h"
#include "opus_defines.h"
#include "opus_types.h"
#include "rtp.hxx"

Server::Server(std::string local_address, uint16_t local_port,
               uint16_t remote_port)
    : local_address(std::move(local_address)) {
  this->logger = spdlog::get("audpipe");

  auto rtp = Rtp(this->local_address, local_port, remote_port, RTP_SEND);

  int error = OPUS_OK;
  this->opusenc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
  if (error != OPUS_OK) {
    auto error_msg = std::format("Failed to create opus encoder. Reason: {}",
                                 opus_strerror(error));
    this->logger->critical(error_msg);
    throw std::runtime_error(error_msg);
  }

  auto &audio = AudioBackend::instance();
  auto inputs = audio.get_inputs();

  audio.set_data_callback([&](const uint8_t *data, size_t len) {
    // TODO: Encode to Opus and send via RTP.
    // this->logger->debug("Received {} bytes of audio data.", len);
    bytes_to_opus(data, len);
  });

  // Find first monitor input. TODO: Config file and setup wizard maybe?
  auto monitor = std::ranges::find_if(
      inputs, [&](AudioDevice &dev) { return dev.is_monitor; });

  this->logger->info("First monitor device found is \'{}\'.",
                     monitor->description);

  audio.record(*monitor.base());
}

void Server::bytes_to_opus(const uint8_t *data, size_t len) {
  //   960 frame size = 20ms at 48kHz. Taken from Opus documentation.
  std::vector<uint8_t> out;
  const size_t max_bytes = len * 2; // May want to be 3840.

  out.reserve(max_bytes);

  auto res =
      opus_encode(this->opusenc, reinterpret_cast<const opus_int16 *>(data),
                  960, out.data(), len);

  // TODO: Recover.
  if (res != OPUS_OK) {
    auto error_msg = std::format("Failed to encode opus frame. Reason: {}",
                                 opus_strerror(res));
    this->logger->critical(error_msg);
    throw std::runtime_error(error_msg);
  }

  this->logger->debug("Got opus frame.");
}
