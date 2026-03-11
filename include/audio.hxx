#ifndef __AUDPIPE_AUDIO
#define __AUDPIPE_AUDIO

// We want to maybe have some modes to list microphones on the system or provide
// a file to stream audio from etc.
//
// This will become a cross platform abstraction layer. Use pulseaudio on Linux,
// and whatever on earth the Windows API is.

#include <vector>

class AudioDevice {};

class AudioBackend {
public:
  virtual ~AudioBackend() = default;
  virtual std::vector<AudioDevice> get_inputs() = 0;
  virtual void open(const AudioDevice &device) = 0;
  virtual void close() = 0;
};

#endif