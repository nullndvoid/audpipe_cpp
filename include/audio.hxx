#ifndef __AUDPIPE_AUDIO
#define __AUDPIPE_AUDIO

// We want to maybe have some modes to list microphones on the system or provide
// a file to stream audio from etc.
//
// This will become a cross platform abstraction layer. Use pulseaudio on Linux,
// and whatever on earth the Windows API is.

#include <cstdint>
#include <string>
#include <vector>

class AudioDevice {
public:
  std::string name;
  std::string description;
  uint32_t index;
  uint32_t sample_rate;
  uint8_t channels;
  bool is_monitor;
  bool is_default;

  AudioDevice(std::string name, std::string description, uint32_t index,
              uint32_t sample_rate, uint8_t channels, bool is_monitor)
      : name(std::move(name)), description(std::move(description)),
        index(index), sample_rate(sample_rate), channels(channels),
        is_monitor(is_monitor), is_default(false) {}
};

class AudioBackend {
public:
  virtual ~AudioBackend() = default;
  virtual std::vector<AudioDevice> get_inputs() = 0;
  virtual std::vector<AudioDevice> get_outputs() = 0;

  // virtual void open(const AudioDevice &device) = 0;
  // virtual void close() = 0;

  // Singleton pattern.
  static AudioBackend &instance();

  // Prevent copies/moves
  AudioBackend(const AudioBackend &) = delete;
  AudioBackend &operator=(const AudioBackend &) = delete;

protected:
  AudioBackend() = default;
};

#endif