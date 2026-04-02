#include <cstdint>
#include <cstring>
#include <iostream>

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "audio.hxx"
#include "opus.h"

#include "rtp.hxx"
#include "server.hxx"

#define OPUS_FRAME_SIZE 3840

std::string getline();

Server::Server(std::pair<std::string, uint16_t> local_socket,
               std::pair<std::string, uint16_t> remote_socket, AudioDevice dev,
               asio::io_context &io)
    : local_address(local_socket.first), device(std::move(dev)),
      rtp(local_socket, remote_socket, io) {
  this->logger = spdlog::get("audpipe");

  int error = OPUS_OK;
  this->opusenc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
  if (error != OPUS_OK) {
    auto error_msg = std::format("Failed to create opus encoder. Reason: {}",
                                 opus_strerror(error));
    this->logger->critical(error_msg);
    throw std::runtime_error(error_msg);
  }

  this->opus_enc_outbuf = std::vector<uint8_t>();
  this->opus_enc_outbuf.resize(OPUS_FRAME_SIZE);
  this->opus_enc_outbuf_size = 0;

  auto &audio = AudioBackend::instance();

  audio.set_data_callback([&](const uint8_t *data, size_t len) {
    this->bytes_to_opus(data, len);

    uint8_t *opus_data = static_cast<uint8_t *>(this->opus_enc_outbuf.data());
    size_t opus_data_len = this->opus_enc_outbuf_size;

    // TODO: Make this check for errors/fail etc. For now just hand it off.
    this->rtp.write_frames(opus_data, opus_data_len);
  });
}

Server::~Server() {
  stop();

  if (this->opusenc != nullptr) {
    opus_encoder_destroy(this->opusenc);
    this->opusenc = nullptr;
  }
}

void Server::run() {
  this->running = true;

  try {
    AudioBackend::instance().record(this->device);
  } catch (...) {
    // We trust upstream code to log before throwing so just ignore the
    // exception.
    this->running = false;
    throw;
  }

  this->running = false;
}

void Server::stop() {
  this->running = false;
  this->rtp.stop();
  AudioBackend::instance().stop_recording();
}

AudioDevice Server::choose_device_interactive(std::vector<AudioDevice> inputs) {
  for (int i = 0; i < inputs.size(); i++) {
    std::cout << std::format("{})\t{}", i + 1, inputs.at(i).description)
              << std::endl;
  }

  std::cout << "Choose a device to record from (1): " << std::flush;
  int idx = 1;

  std::istringstream iss(getline());
  if (!(iss >> idx) || (iss >> std::ws, !iss.eof()) || idx <= 0 ||
      idx > inputs.size()) {
    auto logger = spdlog::get("audpipe");
    logger->debug("Invalid input, defaulting to 1.");

    idx = 1;
  }

  AudioDevice dev = inputs.at(idx - 1);

  return dev;
}

// Source - https://stackoverflow.com/a/546470
// Posted by Johannes Schaub - litb, modified by community. See post 'Timeline'
// for change history Retrieved 2026-03-13, License - CC BY-SA 3.0
std::string getline() {
  std::string str;
  std::getline(std::cin, str);
  return str;
}

// Populates opus_enc_outbuf.
void Server::bytes_to_opus(const uint8_t *data, size_t len) {
  //   960 frame size = 20ms at 48kHz. Taken from Opus documentation.
  const size_t max_bytes = OPUS_FRAME_SIZE; // May want to be 3840?

  // Clear the outbuf.
  memset(this->opus_enc_outbuf.data(), 0, this->opus_enc_outbuf.size());

  auto res =
      opus_encode(this->opusenc, reinterpret_cast<const opus_int16 *>(data),
                  960, this->opus_enc_outbuf.data(), max_bytes);

  // TODO: Recover from this.
  if (res < 0) {
    auto error_msg = std::format("Failed to encode opus frame. Reason: {}",
                                 opus_strerror(res));
    this->logger->critical(error_msg);
    throw std::runtime_error(error_msg);
  }

  // Else we have the size of the output frame. I would rather not reallocate
  // the block of memory, seems easier to just store pointer and length.
  this->opus_enc_outbuf_size = res;
  this->logger->debug("Got opus frame with size {}", res);
}
