#ifndef __AUDPIPE_CLIENT
#define __AUDPIPE_CLIENT

#include <memory>
#include <string>

#include "rtp.hxx"
#include "spdlog/logger.h"

class Client {
public:
  Client(std::string local_address, uint16_t local_port, uint16_t remote_port);

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
  Rtp rtp;

  std::pair<std::function<void(void *, uvgrtp::frame::rtp_frame *)>, void *>
  make_recv_callback(Client *self);
};

#endif