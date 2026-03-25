#ifndef __AUDPIPE_CLIENT
#define __AUDPIPE_CLIENT

#include <cstdint>
#include <memory>
#include <string>

#include "rtp.hxx"
#include "spdlog/logger.h"

class Client {
public:
  Client(std::pair<std::string, uint16_t> local_socket,
         std::pair<std::string, uint16_t> remote_socket);

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
  Rtp rtp;

  std::pair<std::function<void(void *, uvgrtp::frame::rtp_frame *)>, void *>
  make_recv_callback(Client *self);
};

#endif