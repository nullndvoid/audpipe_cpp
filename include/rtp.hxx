#ifndef __AUDPIPE_RTP
#define __AUDPIPE_RTP

#include "uvgrtp/lib.hh" // IWYU pragma: keep
#include <functional>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

class Rtp {
public:
  // Used by sender.
  Rtp(std::string &local_addr, uint16_t local_port, uint16_t remote_port);

  // Used by reciever. `cb` is a pair with first argument being a callback with
  // void* to userdata, and the second argument being the pointer passed to the
  // callback. This may be set to nullptr if not in use.
  Rtp(std::string &local_addr, uint16_t local_port, uint16_t remote_port,
      // First argument is callback taking `userdata` and recieved frame.
      // Second argument is void* to your `userdata`.
      std::pair<std::function<void(void *, uvgrtp::frame::rtp_frame *)>, void *>
          cb);

  ~Rtp();

private:
  static uvgrtp::context ctx;
  uvgrtp::session *session;
  uvgrtp::media_stream *stream;

  std::pair<std::function<void(void *, uvgrtp::frame::rtp_frame *)>, void *>
      recv_callback = std::pair(nullptr, nullptr);

  std::shared_ptr<spdlog::logger> logger;

  static void init_rtp(Rtp *rtp, bool sending, std::string &local_addr,
                       uint16_t local_port, uint16_t remote_port);

  void write_frames();
  void read_frames();
};
#endif