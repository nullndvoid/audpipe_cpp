#ifndef __AUDPIPE_AUDIO
#define __AUDPIPE_AUDIO

// We want to maybe have some modes to list microphones on the system or provide
// a file to stream audio from etc.
//
// This will become a cross platform abstraction layer. Use pulseaudio on Linux,
// and whatever on earth the Windows API is.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Callback invoked with raw audio data: (data pointer, byte length).
using AudioDataCallback = std::function<void(const uint8_t *, size_t)>;

class AudioDevice {
public:
  std::string name;
  std::string description;
  uint32_t index;
  uint32_t sample_rate;
  uint8_t channels;
  bool is_monitor;
  bool is_default;
  bool is_source;

  AudioDevice(std::string name, std::string description, uint32_t index,
              uint32_t sample_rate, uint8_t channels, bool is_monitor,
              bool is_source)
      : name(std::move(name)), description(std::move(description)),
        index(index), sample_rate(sample_rate), channels(channels),
        is_monitor(is_monitor), is_default(false), is_source(is_source) {}
};

class AudioBackend {
public:
  virtual ~AudioBackend() = default;
  virtual std::vector<AudioDevice> get_inputs() = 0;
  virtual std::vector<AudioDevice> get_outputs() = 0;

  // Begin recording from a source device. Calls the data callback with audio
  // chunks as they arrive. Blocks until stop_recording() is called from
  // another thread or the stream fails.
  virtual void record(AudioDevice dev) = 0;
  virtual void stop_recording() = 0;

  // Set a callback to receive raw audio data during recording.
  void set_data_callback(AudioDataCallback cb) {
    data_callback = std::move(cb);
  }

  // Singleton pattern.
  static AudioBackend &instance();

  // Prevent copies/moves
  AudioBackend(const AudioBackend &) = delete;
  AudioBackend &operator=(const AudioBackend &) = delete;

protected:
  AudioBackend() = default;
  AudioDataCallback data_callback;
};

#endif