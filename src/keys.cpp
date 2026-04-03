#include "keys.hxx"

#include "filters.h"
#include "hex.h"

#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>

#include "spdlog/spdlog.h"

using namespace CryptoPP;

KeyManager::KeyManager(const std::filesystem::path &keys_dir,
                       const std::string &keyfile_prefix)
    : keyfile_prefix(keyfile_prefix), keys_dir(keys_dir),
      logger(spdlog::get("audpipe")) {
  this->pubkey_file = {};
  this->privkey_file = {};
}

void KeyManager::ensure_file_open(const std::filesystem::path &file_path,
                                  std::fstream &fs) {
  std::string err_msg;
  auto die = [&] {
    this->logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  try {
    fs.exceptions(std::ios::failbit | std::ios::badbit);

    if (!file_path.parent_path().empty()) {
      // Creates if didn't exist already. Will only shit the bed if
      // file_path/../../ does not exist, or we get another error.
      std::filesystem::create_directory(file_path.parent_path());
    }

    fs.open(file_path, std::ios::app);
    fs.close();
    fs.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
  } catch (const std::ios_base::failure &e) {
    if (e.code().value() == 0) {
      throw;
    }

    this->logger->error("Could not open file at path \'{}\' because: {}",
                        file_path.native(), e.what());

    throw std::system_error{
        e.code().value(), std::generic_category(),
        std::format("Opening file at path \'{}\' failed because: {}",
                    file_path.native(), e.what())};
  } catch (const std::exception &e) {
    // Don't presume this is logged upstream here just in case.
    this->logger->error(e.what());

    throw;
  }

  if (!fs.is_open()) {
    logger->error("Could not open config file at \'{}\' for unknown reasons.",
                  file_path.native());
  }
}

void KeyManager::load_or_create() {
  // Guard in case I accidentally call from elsewhere.
  if (this->loaded)
    return;

  std::string err_msg;
  auto die = [&] {
    this->logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  // Open `keys_dir/`keyfile_prefix`_ed25519` and .pub.
  std::filesystem::path pubkey_path =
      this->keys_dir / std::format("{}_ed25519.pub", this->keyfile_prefix);
  std::filesystem::path privkey_path =
      this->keys_dir / std::format("{}_ed25519", this->keyfile_prefix);

  // These should be open for r/w if they don't throw.
  ensure_file_open(pubkey_path, this->pubkey_file);
  ensure_file_open(privkey_path, this->privkey_file);

  // If empty then we want to create a keypair.
  this->pubkey_file.seekg(0, std::ios::end);
  size_t pubkey_file_size = this->pubkey_file.tellg();
  this->pubkey_file.seekg(0, std::ios::beg);

  this->privkey_file.seekg(0, std::ios::end);
  size_t privkey_file_size = this->privkey_file.tellg();
  this->privkey_file.seekg(0, std::ios::beg);

  if (privkey_file_size == 0 || pubkey_file_size == 0) {
    create();

    return;
  }

  load();
}

void KeyManager::sign() {}

void KeyManager::verify() {}

void KeyManager::load() {}

void KeyManager::ensure_loaded() {
  if (this->loaded && this->signer.has_value() && this->verifier.has_value()) {
    return;
  }

  this->load_or_create();
}

void KeyManager::pubkey(ed25519::Signer &signer) {
  ed25519::Verifier verifier(signer);

  const ed25519PublicKey &pubkey =
      dynamic_cast<const ed25519PublicKey &>(verifier.GetPublicKey());
}

// Creates a new keypair from scratch.
void KeyManager::create() {
  auto privkey_str = std::string();
  HexEncoder encoder(new StringSink(privkey_str));

  AutoSeededRandomPool prng;
  ed25519::Signer signer;

  signer.AccessPrivateKey().GenerateRandom(prng);
  const ed25519PrivateKey &privkey =
      dynamic_cast<const ed25519PrivateKey &>(signer.GetPrivateKey());

  privkey.Save(encoder);
}