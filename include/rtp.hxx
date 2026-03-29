#ifndef __AUDPIPE_RTP
#define __AUDPIPE_RTP

#include "uvgrtp/lib.hh" // IWYU pragma: keep
#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

class Rtp {
public:
  // Used by sender.
  Rtp(std::pair<std::string, uint16_t> local_socket,
      std::pair<std::string, uint16_t> remote_socket);

  // NOTE: The hook should not be used for extensive media processing. It is
  // meant to be used as an interface between application and library where
  // uvgRTP hands off the RTP frames to an application thread.
  using recv_hook_t = void (*)(void *, uvgrtp::frame::rtp_frame *);

  // Used by reciever. `cb` is a pair with first argument being a callback with
  // void* to userdata, and the second argument being the pointer passed to the
  // callback. This may be set to nullptr if not in use.
  //
  // NOTE: The hook should not be used for extensive media processing. It is
  // meant to be used as an interface between application and library where
  // uvgRTP hands off the RTP frames to an application thread.
  //
  Rtp(std::pair<std::string, uint16_t> local_socket,
      std::pair<std::string, uint16_t> remote_socket,
      std::pair<recv_hook_t, void *> cb);

  std::pair<recv_hook_t, void *> recv_callback = {nullptr, nullptr};

  ~Rtp();

  // TODO: Handle RTCP information for reads and writes internally.
  void write_frames(uint8_t *data, size_t data_len);

private:
  static uvgrtp::context ctx;
  uvgrtp::session *session;
  uvgrtp::media_stream *stream;

  std::shared_ptr<spdlog::logger> logger;

  static void init_rtp(Rtp *rtp, bool sending, std::string &local_addr,
                       uint16_t local_port, uint16_t remote_port);

  // TODO: Handle RTCP information for reads and writes.
  void read_frames();
};

#endif