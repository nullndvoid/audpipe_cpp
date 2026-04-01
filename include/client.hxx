#ifndef __AUDPIPE_CLIENT
#define __AUDPIPE_CLIENT

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "config.hxx"
#include "opus.h"
#include "opus_types.h"
#include "rtp.hxx"
#include "spdlog/logger.h"

#define PCM_DECBUF_SIZE (960 * 2)

class Client {
  using recv_hook_t = void (*)(void *, uvgrtp::frame::rtp_frame *);

public:
  Client(std::pair<std::string, uint16_t> local_socket,
         std::pair<std::string, uint16_t> remote_socket,
         ConnectionPolicy conn_pol = ConnectionPolicy{});

  ~Client();

  // Prevent copies/moves
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  bool is_healthy() const;
  void request_shutdown();
  size_t get_frames_received() const;

  // Set to `nullptr` if not initialised for whatever reason.
  Rtp *get_rtp();

private:
  std::string local_address;
  std::shared_ptr<spdlog::logger> logger;
  std::optional<Rtp> rtp;

  std::atomic<bool> should_stop = false;
  std::atomic<bool> healthy = true;
  std::atomic<size_t> frames_received = 0;
  std::string last_error;

  OpusDecoder *opusdec = nullptr;
  std::vector<opus_int16> pcm_decbuf;

  // PCM bytes ready for virtual microphone writer.
  std::deque<uint8_t> playback_queue;
  // Compressed Opus payloads queued from RTP callback.
  std::deque<std::vector<uint8_t>> compressed_queue;
  // Deque isn't thread safe.
  std::mutex playback_queue_mutex;

  std::thread virtual_input_thread;

  ConnectionPolicy conn_pol;

  std::pair<recv_hook_t, void *> make_recv_callback(Client *self);
  static void recv_callback(void *userdata, uvgrtp::frame::rtp_frame *frame);

  void set_error(const std::string &err);

  static void call_virtual_input_setup(void *audio);
};

#endif