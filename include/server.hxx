#ifndef __AUDPIPE_SERVER
#define __AUDPIPE_SERVER

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <opus.h>

#include <spdlog/spdlog.h>

#include "audio.hxx"
#include "rtp.hxx"

class Server {
public:
  Server(std::pair<std::string, uint16_t> local_socket,
         std::pair<std::string, uint16_t> remote_socket, AudioDevice dev);
  ~Server();

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void run();
  void stop();

  // Interactively asks the user to choose a device to use. This should likely
  // be replaced with configuration file/CLI arguments.
  static AudioDevice choose_device_interactive(std::vector<AudioDevice> inputs);

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
  OpusEncoder *opusenc = nullptr;

  std::vector<uint8_t> opus_enc_outbuf;
  size_t opus_enc_outbuf_size;
  AudioDevice device;
  std::atomic<bool> running{false};

  Rtp rtp;

  void bytes_to_opus(const uint8_t *data, size_t len);
};

#endif