#ifndef __AUDPIPE_HANDSHAKE
#define __AUDPIPE_HANDSHAKE

#include <cstdint>

namespace handshake {
enum class Role : uint8_t {
  CLIENT = 0,
  SERVER = 1,
};

enum class MessageType : uint8_t {
  CLIENT_HELLO = 0,
  SERVER_HELLO = 1,
  FINISHED = 2,
};

constexpr uint8_t MAGIC[4] = {'A', 'P', 'H', 'M'};

struct HandshakePacket {
  // Encoded in network order. Must be equal to `MAGIC`.
  uint32_t magic;
  // Valid values: 1.
  uint8_t version;
  handshake::MessageType msg_type;
  handshake::Role role;
  // Reserved for future use.
  uint8_t flags;
  // Random per session.
  uint64_t session_id;
  // Random nonce to prevent replay attacks.
  uint8_t nonce[16];
  // Raw public key bytes.
  uint8_t x25519_pub[32];
  // Fingerprint of long-term Ed25519 key.
  uint8_t signer_id[32];
  // The length of the signature thereafter, where signature is the ed25519
  // signature for the packet.
  uint16_t sig_len;
  // uint8_t signature[sig_len];
};
} // namespace handshake

#endif