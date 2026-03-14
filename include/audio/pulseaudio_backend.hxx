#ifndef __AUDPIPE_PULSE_BACKEND
#define __AUDPIPE_PULSE_BACKEND

#include "audio.hxx"

#include <memory>

#include <spdlog/spdlog.h>

#include <pulse/pulseaudio.h>

class PulseaudioBackend : public AudioBackend {
public:
  PulseaudioBackend();
  ~PulseaudioBackend();
  std::vector<AudioDevice> get_inputs() override;
  std::vector<AudioDevice> get_outputs() override;
  void record(AudioDevice dev) override;
  void stop_recording() override;
  void create_virtual_input() override;

private:
  pa_context *ctx;
  pa_mainloop *mainloop;
  pa_mainloop_api *api;
  pa_stream *stream = nullptr;
  std::shared_ptr<spdlog::logger> logger;
  std::atomic<bool> recording{false};

  void wait_for_context_ready();

  void wait_for_operation(pa_operation *op);

  // Block the mainloop until the stream transitions to READY or FAILED.
  void wait_for_stream_ready(pa_stream *s);

  // Disconnect and unreference the active stream, if any.
  void destroy_stream();

  // Called on stream state changes.
  static void stream_state_cb(pa_stream *s, void *userdata);

  // Called when data is available to read from the stream.
  static void stream_read_cb(pa_stream *s, size_t nbytes, void *userdata);
};

#endif