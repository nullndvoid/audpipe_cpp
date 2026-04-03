#include "keys.hxx"

#include "base64.h"
#include "cryptlib.h"
#include "files.h"

#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>

#include "hex.h"
#include "nlohmann/json.hpp"
#include "queue.h"
#include "spdlog/spdlog.h"
#include "xed25519.h"

using namespace CryptoPP;

KeyManager::KeyManager(const std::filesystem::path &keys_dir,
                       const std::string &keyfile_prefix)
    : keyfile_prefix(keyfile_prefix), keys_dir(keys_dir),
      logger(spdlog::get("audpipe")) {
  this->pubkey_file = {};
  this->privkey_file = {};
}

void KeyManager::ensure_file_open(const std::filesystem::path &file_path,
                                  std::fstream &fs,
                                  std::ios_base::openmode mode) {
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
    fs.open(file_path, mode);
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
    logger->error("Could not open file at \'{}\' for unknown reasons.",
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
  this->pubkey_path =
      this->keys_dir / std::format("{}_ed25519.pub", this->keyfile_prefix);
  this->privkey_path =
      this->keys_dir / std::format("{}_ed25519", this->keyfile_prefix);

  this->trusted_peers_path = this->keys_dir / "trusted_peers.json";

  // These should be open for r/w if they don't throw.
  ensure_file_open(pubkey_path, this->pubkey_file);
  ensure_file_open(privkey_path, this->privkey_file);
  ensure_file_open(trusted_peers_path, this->trusted_peers_file);

  // If empty then we want to create a keypair.
  this->pubkey_file.seekg(0, std::ios::end);
  size_t pubkey_file_size = this->pubkey_file.tellg();
  this->pubkey_file.seekg(0, std::ios::beg);

  this->privkey_file.seekg(0, std::ios::end);
  size_t privkey_file_size = this->privkey_file.tellg();
  this->privkey_file.seekg(0, std::ios::beg);

  if (privkey_file_size == 0 || pubkey_file_size == 0) {
    create();

    logger->info("Created ed25519 keypair in {}.", this->keys_dir.native());

    return;
  }

  load();

  logger->info("Loaded ed25519 keypair from {}.", this->keys_dir.native());
}

void KeyManager::sign() {}

void KeyManager::verify() {}

void load_from_file(const std::string &filename, BufferedTransformation &bt) {
  FileSource file(filename.c_str(), true);

  file.TransferTo(bt);
  bt.MessageEnd();
}

void KeyManager::load() {
  this->privkey_file.close();
  this->pubkey_file.close();

  Base64Decoder dec(new ByteQueue());

  ::load_from_file(this->pubkey_path, dec);
  this->pubkey.Load(dec);

  auto queue = dynamic_cast<ByteQueue *>(dec.AttachedTransformation());
  queue->Clear();

  ::load_from_file(this->privkey_path, dec);
  this->privkey.Load(dec);
}

// You can safely call .value() on signer, verifier after this, provided it does
// not throw an exception.
void KeyManager::ensure_loaded() {
  if (this->loaded && this->signer.has_value() && this->verifier.has_value()) {
    return;
  }

  this->load_or_create();
}

// We may cache the result but this is called on `create()`.
void KeyManager::create_pubkey() {
  std::string err_msg;
  auto die = [&] {
    this->logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  if (!signer.has_value()) {
    err_msg = "KeyManager->signer not set but called `KeyManager::pubkey`! "
              "This is an internal invariant. Probably file an issue if you "
              "see this.";

    die();
  }

  this->verifier = ed25519::Verifier(signer.value());

  auto file_sink = new FileSink(this->pubkey_file);
  Base64Encoder encoder(file_sink);

  const ed25519PublicKey &pubkey =
      dynamic_cast<const ed25519PublicKey &>(this->verifier->GetPublicKey());

  pubkey.Save(encoder);
  encoder.MessageEnd();

  this->pubkey = pubkey;
}

KeyManager::~KeyManager() {
  if (this->privkey_file.is_open()) {
    this->privkey_file.close();
  }

  if (this->pubkey_file.is_open()) {
    this->pubkey_file.close();
  }
}

// Creates a new keypair from scratch.
void KeyManager::create() {
  Base64Encoder encoder(new FileSink(this->privkey_file));

  AutoSeededRandomPool prng;
  this->signer = ed25519::Signer();
  this->signer->AccessPrivateKey().GenerateRandom(prng);
  const ed25519PrivateKey &privkey =
      dynamic_cast<const ed25519PrivateKey &>(this->signer->GetPrivateKey());

  privkey.Save(encoder);
  encoder.MessageEnd();

  this->create_pubkey();
  this->privkey = privkey;
}

std::vector<TrustedPeer> KeyManager::get_trusted_peers() {
  using json = nlohmann::json;

  json data;

  std::string err_msg;
  auto die = [&] {
    this->logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  this->trusted_peers_file >> data;

  if (!data.is_array()) {
    err_msg = std::format(
        "{} appears to be corrupted. Expected an array of TrustedPeers.",
        this->trusted_peers_path.filename().native());

    die();
  }

  for (auto &elem : data) {
    if (!elem.is_object()) {
      err_msg = std::format(
          "{} appears to be corrupted. Expected an array of TrustedPeers.",
          this->trusted_peers_path.filename().native());
    }
  }

  return {}; // change me.
}

TrustedPeer::TrustedPeer(json::TrustedPeer json_model) {
  std::string err_msg;
  auto logger = spdlog::get("audpipe");
  auto die = [&] {
    logger->error(err_msg);

    throw std::runtime_error(err_msg);
  };

  if (json_model.signer_id.empty()) {
    err_msg =
        std::format("Empty `signer_id` field when reading from JSON file!");
    die();
  }

  if (json_model.pubkey_base64.empty()) {
    err_msg =
        std::format("Empty `pubkey_base64` field when reading from JSON file!");
    die();
  }

  // Expect 64 characters for `signer_id` as it is hex encoded.
  if (json_model.signer_id.length() != 64) {
    err_msg =
        std::format("Incorrect `signer_id` length as this is hex encoded!");
    die();
  }

  // Decode fields and create TrustedPeer.
  this->signer_id = decode_hex_id(json_model.signer_id);
  this->pubkey = decode_pubkey_base64(json_model.pubkey_base64);
  this->label = std::move(json_model.label);
  this->revoked = json_model.revoked;
}

std::array<uint8_t, 32> TrustedPeer::decode_hex_id(const std::string &hex) {
  std::array<uint8_t, 32> out{};
  auto ss =
      CryptoPP::StringSource(hex, true,
                             new CryptoPP::HexDecoder(new CryptoPP::ArraySink(
                                 out.data(), out.size())));
  return out;
}

CryptoPP::ed25519PublicKey
TrustedPeer::decode_pubkey_base64(const std::string &b64) {
  CryptoPP::ByteQueue q;
  auto ss = CryptoPP::StringSource(
      b64, true, new CryptoPP::Base64Decoder(new CryptoPP::Redirector(q)));

  CryptoPP::ed25519PublicKey pubkey;
  pubkey.Load(q);
  return pubkey;
}

std::string
TrustedPeer::encode_hex_id(const std::array<uint8_t, 32> &id) const {
  std::string out;
  CryptoPP::StringSource ss(
      id.data(), id.size(), true,
      new CryptoPP::HexEncoder(new CryptoPP::StringSink(out), false));
  return out;
}

std::string TrustedPeer::encode_pubkey_base64(
    const CryptoPP::ed25519PublicKey &pubkey) const {
  std::string out;
  CryptoPP::Base64Encoder encoder(new CryptoPP::StringSink(out), false);
  pubkey.Save(encoder);
  encoder.MessageEnd();
  return out;
}

json::TrustedPeer TrustedPeer::to_json() const {
  return json::TrustedPeer{
      .signer_id = encode_hex_id(this->signer_id),
      .pubkey_base64 = encode_pubkey_base64(this->pubkey),
      .label = this->label,
      .revoked = this->revoked,
  };
}