// Long term key management code for ed25519.
// TODO: Setup wizard stuff.

#ifndef __AUDPIPE_KEYS
#define __AUDPIPE_KEYS

#include "spdlog/logger.h"

#include <cstdint>
#include <osrng.h>
#include <xed25519.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

// Manages ed25519 keys and message signing, verification using these.
//
// TODO: Allow trust-on-first-use in config file.
//       this makes life easier for users who can't
//       or won't copy paste their public keys over
//       to remote.
class KeyManager {
public:
  KeyManager(const std::filesystem::path &keys_dir,
             const std::string &keyfile_prefix);

  ~KeyManager();

  // Loads or creates a ed25519 keypair.
  void load_or_create();

  // Signs a message using the ed25519 keypair.
  void sign();

  // Verifies a message using the ed25519 keypair.
  void verify();

  void get_pubkey();

  // TODO: Could just serialise a list of these to a file. Prefixed with the
  // number of entries.
  struct TrustedPeer {
    std::array<uint8_t, 32> signer_id;
    CryptoPP::ed25519PublicKey pubkey;
    std::string label;
    bool revoked = false;
  };

  std::array<uint8_t, 32> get_local_signer_id() const;

  std::optional<TrustedPeer>
  lookup_peer(std::span<const uint8_t, 32> signer_id) const;

  bool verify_with_signer_id(std::span<const uint8_t> message,
                             std::span<const uint8_t> signature,
                             std::span<const uint8_t, 32> signer_id) const;

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

  std::fstream trusted_peers_file;

  std::filesystem::path trusted_peers_path;

  std::filesystem::path pubkey_path;
  std::filesystem::path privkey_path;

  CryptoPP::ed25519PrivateKey privkey;
  CryptoPP::ed25519PublicKey pubkey;

  // Guard to check signer and verifier are loaded.
  void ensure_loaded();

  // Helper function to load the keypair from disk.
  void load();

  // Helper function to create a new keypair.
  void create();

  // Helper function used to derive a public key from a Signer.
  // Called in `create()` but we may cache this for later usage.
  void create_pubkey();

  // Ensures a file is open before we try to read to or write from the given
  // `fstream`.
  void ensure_file_open(const std::filesystem::path &file_path,
                        std::fstream &fs,
                        std::ios_base::openmode mode = std::ios::app |
                                                       std::ios::in |
                                                       std::ios::out);

  // If the file does not exist, don't crash, just return nothing.
  std::vector<TrustedPeer> get_trusted_peers();
};

#endif