# audpipe

A tool to forward microphones to other devices over end to end encrypted RTP. Maybe I might also make it into a tiny CLI VOIP tool to chat with your friends.

Currently only records input on Linux via Pulseaudio but I plan to extend support to Windows and Mac OS soon.

## TODOs

- Fix broken client, server implementations.
- Make a TUI/GUI for controlling the application.
- Fix timeout issues on initial handshake (partially done)
- Add Windows support in terms of audio (very important).
- Add pipewire backend for fun and learning.
- VOIP chat for fun? Or write like a library and have a different app call helpers.

## Dependencies

Currently we need the following dependencies at build/runtime

- pulseaudio (or pipewire-pulse) on Linux
