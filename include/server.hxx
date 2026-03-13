#ifndef __AUDPIPE_SERVER
#define __AUDPIPE_SERVER

#include <cstdint>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

class Server {
public:
  Server(std::string local_address, uint16_t local_port, uint16_t remote_port);

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
};

#endif