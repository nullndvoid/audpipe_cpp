#ifndef __AUDPIPE_RTP
#define __AUDPIPE_RTP

#include "uvgrtp/lib.hh" // IWYU pragma: keep
#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

typedef enum rtp_mode {
  RTP_SEND = 0,
  RTP_RECV = 1,
} rtp_mode_t;

class Rtp {
public:
  Rtp(std::string &local_addr, uint16_t local_port, uint16_t remote_port,
      rtp_mode_t mode);

private:
  static uvgrtp::context ctx;
  uvgrtp::session *session;
  uvgrtp::media_stream *stream;

  rtp_mode_t mode;

  std::shared_ptr<spdlog::logger> logger;
};
#endif