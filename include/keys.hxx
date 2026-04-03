// Long term key management code for ed25519.
// TODO: Setup wizard stuff.

#ifndef __AUDPIPE_KEYS
#define __AUDPIPE_KEYS

#include "spdlog/logger.h"

#include <osrng.h>
#include <xed25519.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

class KeyManager {
public:
  KeyManager(const std::filesystem::path &keys_dir,
             const std::string &keyfile_prefix);

  // Loads or creates a ed25519 keypair.
  void load_or_create();

  // Signs a message using the ed25519 keypair.
  void sign();

  // Verifies a message using the ed25519 keypair.
  void verify();

  void get_pubkey();

private:
  std::shared_ptr<spdlog::logger> logger;

  // The directory in which the keypair ${HOSTNAME}_ed25519, .pub are stored.
  std::filesystem::path keys_dir;

  std::string keyfile_prefix;

  // If we have loaded or created the necessary keypair for operations.
  bool loaded = false;

  std::optional<CryptoPP::ed25519::Signer> signer;
  std::optional<CryptoPP::ed25519::Verifier> verifier;

  std::fstream pubkey_file;
  std::fstream privkey_file;

  // Guard to check signer and verifier are loaded.
  void ensure_loaded();

  // Helper function to load the keypair from disk.
  void load();

  // Helper function to create a new keypair.
  void create();

  void pubkey(CryptoPP::ed25519::Signer &signer);

  // Ensures a file is open before we try to read to or write from the given
  // `fstream`.
  void ensure_file_open(const std::filesystem::path &file_path,
                        std::fstream &fs);
};

#endif