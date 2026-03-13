#ifndef __AUDPIPE_SERVER
#define __AUDPIPE_SERVER

#include <cstdint>
#include <memory>
#include <string>

#include <opus.h>

#include <spdlog/spdlog.h>

class Server {
public:
  Server(std::string local_address, uint16_t local_port, uint16_t remote_port);

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
  OpusEncoder *opusenc;

  std::vector<uint8_t> opus_enc_outbuf;
  size_t opus_enc_outbuf_size;

  void bytes_to_opus(const uint8_t *data, size_t len);
};

#endif