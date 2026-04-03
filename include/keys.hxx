// Long term key management code for ed25519.
// TODO: Setup wizard stuff.

#ifndef __AUDPIPE_KEYS
#define __AUDPIPE_KEYS

class KeyManager {
public:
  KeyManager();

  // Loads or creates a ed25519 keypair.
  void load_or_create();

  // Signs a message using the ed25519 keypair.
  void sign();

  // Verifies a message using the ed25519 keypair.
  void verify();

private:
};

#endif