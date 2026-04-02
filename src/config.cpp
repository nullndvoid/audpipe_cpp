#include <arpa/inet.h>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <iostream>
#include <netdb.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <toml++/toml.hpp>

#include "config.hxx"

// Returns the opposing mode to ensure users provide the remote connection
// details.
inline Mode remote(Mode m) {
  return (m == Mode::CLIENT) ? Mode::SERVER : Mode::CLIENT;
}

// Converts the `Mode` to a string, lowercase.
inline std::string Config::mode_to_str(Mode m) {
  return (m == Mode::CLIENT) ? "client" : "server";
}

uint16_t Config::validate_port(int64_t port) {
  if (port < 1024 || port > 65535) {
    auto logger = spdlog::get("audpipe");
    auto err_str = std::format(
        "Got \'{}\' for port but expected port in [1024, 65535].", port);

    logger->error(err_str);

    throw std::runtime_error(err_str);
  }

  return static_cast<uint16_t>(port);
}

// Resolves hosts using system DNS if not passed an IP address.
// Throws exception if getaddrinfo fails.
const std::string Config::validate_ip(const std::string &s) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;                 /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM;              /* Datagram socket */
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* For wildcard IP address */
  hints.ai_protocol = 0;                       /* Any protocol */
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  struct addrinfo *out = {};
  auto ret = getaddrinfo(s.c_str(), nullptr, &hints, &out);
  if (ret != 0) {
    auto logger = spdlog::get("audpipe");

    auto err_msg = std::format("getaddrinfo failed for host \'{}\': {}", s,
                               gai_strerror(ret));
    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }

  void *addr;
  char ip[INET6_ADDRSTRLEN];

  if (out->ai_family == AF_INET6) {
    addr = &((struct sockaddr_in6 *)out->ai_addr)->sin6_addr;
    inet_ntop(AF_INET6, addr, ip, sizeof(ip));
  } else {
    addr = &((struct sockaddr_in *)out->ai_addr)->sin_addr;
    inet_ntop(AF_INET, addr, ip, sizeof(ip));
  }

  return std::string(ip);
}

std::string Config::file_to_str(const std::filesystem::path &path) {
  auto logger = spdlog::get("audpipe");
  auto p = std::filesystem::canonical(path);

  errno = 0;
  std::ifstream config_file = std::ifstream(p);
  try {
    config_file.exceptions(std::ios::failbit | std::ios::badbit);
  } catch (const std::ios_base::failure &e) {
    if (errno == 0) {
      throw;
    }

    logger->error("Could not open file at path \'{}\' because: {}", p.native(),
                  e.what());

    throw std::system_error{
        errno, std::generic_category(),
        std::format("Opening file at path \'{}\' failed because: {}",
                    p.native(), e.what())};
  }

  if (!config_file.is_open()) {
    logger->error("Could not open config file at \'{}\' for unknown reasons.",
                  p.native());
  }

  std::vector<char> config_contents(
      (std::istreambuf_iterator<char>(config_file)),
      std::istreambuf_iterator<char>());

  return std::string(config_contents.data());
}

Config::Config(const std::string &cfg_path, Mode mode) {
  auto logger = spdlog::get("audpipe");
  std::filesystem::path path;

  try {
    path = std::filesystem::path(cfg_path);
  } catch (const std::exception &e) {
    logger->error("Invalid path input \'{}\' threw exception: {}", cfg_path,
                  e.what());
    throw e;
  }

  auto p = std::filesystem::canonical(path);
  auto contents = Config::file_to_str(p);

  // Now parse as TOML.
  toml::table table;
  try {
    table = toml::parse(contents);
  } catch (const toml::parse_error &err) {
    logger->error("Failed to parse TOML config from file \'{}\'", p.native());
    logger->error("Reason: {} (line {}, column {})", err.what(),
                  err.source().begin.line, err.source().begin.column);

    throw;
  }

  std::string err_msg;
  auto die = [&] {
    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  // Parse optional connection policy section with sensible defaults.
  auto connection_table = table["net"];
  if (connection_table && connection_table.is_table()) {
    auto connect_timeout =
        connection_table["connect_timeout_ms"].value_exact<int64_t>();
    if (connect_timeout.has_value()) {
      this->connection.connect_timeout_ms =
          static_cast<uint32_t>(connect_timeout.value());
    }

    auto handshake_timeout =
        connection_table["handshake_timeout_ms"].value_exact<int64_t>();
    if (handshake_timeout.has_value()) {
      this->connection.handshake_timeout_ms =
          static_cast<uint32_t>(handshake_timeout.value());
    }

    auto max_retries = connection_table["max_retries"].value_exact<int64_t>();
    if (max_retries.has_value()) {
      this->connection.max_retries = static_cast<uint16_t>(max_retries.value());
      if (this->connection.max_retries == 0) {
        err_msg = "Key `net.max_retries` should be at least 1.";
        die();
      }
    }

    auto retry_backoff =
        connection_table["retry_backoff_ms"].value_exact<int64_t>();
    if (retry_backoff.has_value()) {
      this->connection.retry_backoff_ms =
          static_cast<uint32_t>(retry_backoff.value());
    }

    auto max_backoff =
        connection_table["max_backoff_ms"].value_exact<int64_t>();
    if (max_backoff.has_value()) {
      this->connection.max_backoff_ms =
          static_cast<uint32_t>(max_backoff.value());
    }

    auto keepalive =
        connection_table["keepalive_interval_ms"].value_exact<int64_t>();
    if (keepalive.has_value()) {
      this->connection.keepalive_interval_ms =
          static_cast<uint32_t>(keepalive.value());
    }

    auto unreachable_failures =
        connection_table["unreachable_after_failures"].value_exact<int64_t>();
    if (unreachable_failures.has_value()) {
      this->connection.unreachable_after_failures =
          static_cast<uint16_t>(unreachable_failures.value());
    }
  }

  // Must be present or we shit the bed.
  auto remote_table_name = mode_to_str(remote(mode));
  auto remotes = table[remote_table_name];

  // May be null, in which case we stick with defaults.
  auto local_table_name = mode_to_str(mode);
  auto locals = table[local_table_name];

  if (!remotes || !remotes.is_table()) {
    err_msg =
        std::format("Expected table `[{}]` in config file.", remote_table_name);

    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  }

  auto ip = remotes["ip"].value<std::string>();
  if (!ip.has_value()) {
    err_msg =
        std::format("You need to set {}.ip in config file.", remote_table_name);

    die();
  }

  this->remote_ip = validate_ip(ip.value());

  auto port = remotes["port"].value_exact<int64_t>();
  if (!port.has_value()) {
    err_msg = std::format("You need to set {}.port in config file. The "
                          "port should also be in the range 1024-65535.",
                          remote_table_name);

    die();
  }
  this->remote_port = validate_port(port.value());

  // Then, check `mode` table for ip and port. If not present, defaults are
  // already set, and we will log these values later.
  if (!locals) {
    logger->info("Read configuration file from \'{}\'.", path.native());

    return;
  }

  auto local_ip = locals["ip"].value_or<std::string>(DEFAULT_LOCAL_IP);
  this->local_ip = validate_ip(local_ip);

  auto local_port = locals["port"].value_exact<int64_t>();
  if (!local_port.has_value()) {
    err_msg = std::format("You need to set {}.port in config file. The "
                          "port should also be in the range 1024-65535.",
                          local_table_name);

    die();
  }
  this->local_port = validate_port(local_port.value());

  logger->info("Read configuration file from \'{}\'.", path.native());
}

std::optional<std::string> Config::get_user_config_path() {
  auto logger = spdlog::get("audpipe");
  auto home_dir = std::getenv("HOME");

  if (home_dir == nullptr) {
    logger->error("Could not get $HOME, config file will not be automatically "
                  "read from "
                  "\'~/.config/audpipe/audpipe.toml\'.");
  }

  return std::nullopt;
}