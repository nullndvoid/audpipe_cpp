// Defines we might want to set application-wide. Currently most of these are
// just consumed in `parse_cli`.
#ifndef __AUDPIPE_CONFIG
#define __AUDPIPE_CONFIG

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#define DEFAULT_REMOTE_PORT 8890
#define DEFAULT_LOCAL_PORT 8891
#define DEFAULT_LOCAL_IP "localhost"

// TODO: Allow configuration in [net] table.
#define DEFAULT_CONNECT_TIMEOUT_MS 5000
#define DEFAULT_HANDSHAKE_TIMEOUT_MS 3000
#define DEFAULT_MAX_RETRIES 3
#define DEFAULT_RETRY_BACKOFF_MS 2000
#define DEFAULT_MAX_BACKOFF_MS 30000
#define DEFAULT_KEEPALIVE_INTERVAL_MS 20000
#define DEFAULT_UNREACHABLE_AFTER_FAILURES 3

enum class Mode { CLIENT, SERVER };

struct ConnectionPolicy {
  uint32_t connect_timeout_ms = DEFAULT_CONNECT_TIMEOUT_MS;
  uint32_t handshake_timeout_ms = DEFAULT_HANDSHAKE_TIMEOUT_MS;
  uint16_t max_retries = DEFAULT_MAX_RETRIES;
  uint32_t retry_backoff_ms = DEFAULT_RETRY_BACKOFF_MS;
  uint32_t max_backoff_ms = DEFAULT_MAX_BACKOFF_MS;
  uint32_t keepalive_interval_ms = DEFAULT_KEEPALIVE_INTERVAL_MS;
  uint16_t unreachable_after_failures = DEFAULT_UNREACHABLE_AFTER_FAILURES;
};

class Config {
public:
  // Default behaviour should be to pass cwd/audpipe.toml. If this is empty we
  // can find ~/.config/audpipe/audpipe.toml.
  //
  // Should make some public static helper methods to get the paths as needed.
  Config(const std::string &cfg_path, Mode mode);

  static std::optional<std::string> get_user_config_path();

  // Converts the `Mode` to a string, lowercase.
  static std::string mode_to_str(Mode m);

  std::string local_ip;
  std::string remote_ip;
  uint16_t remote_port;
  uint16_t local_port;

  // Connection policy for initial handshake and runtime health.
  struct ConnectionPolicy connection;

private:
  static std::string file_to_str(const std::filesystem::path &path);
  // Resolve hostnames as required using system DNS.
  const std::string validate_ip(const std::string &s);
  uint16_t validate_port(int64_t port);
};

#endif