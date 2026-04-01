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
#define DEFAULT_LOCAL_IP "127.0.0.1"

enum class Mode { CLIENT, SERVER };

class Config {
public:
  // Default behaviour should be to pass cwd/audpipe.toml. If this is empty we
  // can find ~/.config/audpipe/audpipe.toml.
  //
  // Should make some public static helper methods to get the paths as needed.
  Config(const std::string &cfg_path, Mode mode);

  static std::optional<std::string> get_user_config_path();

  std::string local_ip;
  std::string remote_ip;
  uint16_t remote_port;
  uint16_t local_port;

private:
  static std::string file_to_str(const std::filesystem::path &path);
  bool validate_ip(const std::string &s);
  uint16_t validate_port(const std::string &s);
};

#endif