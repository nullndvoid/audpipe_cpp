#ifndef __AUDPIPE_PULSE_BACKEND
#define __AUDPIPE_PULSE_BACKEND

#include "audio.hxx"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

enum class PulseaudioBackendState { SETUP, READY, ERROR, STOPPED };

class PulseaudioBackend : public AudioBackend {
public:
  PulseaudioBackend();
  ~PulseaudioBackend();
  std::vector<AudioDevice> get_inputs() override;
  std::vector<AudioDevice> get_outputs() override;
  void record(AudioDevice dev) override;
  void stop_recording() override;

  void setup_virtual_input() override;
  void run_virtual_input() override;
  bool is_virtual_input_ready() const override;
  std::string get_virtual_input_error() const override;
  void create_virtual_input() override;
  // Explicitly unload the module-pipe-source virtual input.
  // Safe to call when already torn down.
  // Throws std::runtime_error on unload failure so callers can fail fast.
  // The destructor also calls this, but explicit calls are preferred when
  // errors must be observed by the caller.
  void destroy_virtual_input() override;

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
  pa_stream *stream = nullptr;
  std::shared_ptr<spdlog::logger> logger;
  std::atomic<bool> recording{false};
  std::atomic<bool> playback{false};

  // Used to determine whether `destroy_virtual_input` should be called.
  std::atomic<bool> virtual_source_loaded{false};
  // Set on errors. The error string should be set for debugging etc.
  std::atomic<bool> virtual_input_failed{false};

  std::string virtual_input_error;

  mutable std::mutex virtual_input_state_mutex;
  PulseaudioBackendState virtual_input_state = PulseaudioBackendState::SETUP;

  // Used for unloading `module-pipe-source` when done with the virtual input.
  uint32_t virtual_source_mod_idx{PA_INVALID_INDEX};

  // FIFO path used by module-pipe-source.
  std::string virtual_source_fifo_path{"/tmp/audpipe_input.pcm"};

  // Writer fd for the FIFO.
  int virtual_source_fd{-1};

  void wait_for_context_ready();

  // Block the mainloop until the stream transitions to READY or FAILED.
  void wait_for_stream_ready(pa_stream *s);

  // Disconnect and unreference the active stream, if any.
  void destroy_stream();

  // Called on stream state changes.
  static void stream_state_cb(pa_stream *s, void *userdata);

  // Called when data is available to read from the stream.
  static void stream_read_cb(pa_stream *s, size_t nbytes, void *userdata);

  // Called when data is available to write to the stream.
  static void stream_write_cb(pa_stream *s, size_t nbytes, void *userdata);

  void set_virtual_input_error(const std::string &msg);
  void clear_virtual_input_state();
};

#endif