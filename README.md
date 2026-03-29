# audpipe

A tool to forward microphones to other devices over end to end encrypted RTP.  Maybe I might also make it into a tiny CLI VOIP tool to chat with your friends.

Currently only records input on Linux via Pulseaudio but I plan to extend support to Windows and Mac OS soon.


## TODOs

* Fix broken client, server implementations.
* Implement state machines for Server and Rtp classes.
* Make a TUI/GUI for controlling the application.
* Fix timeout issues on initial handshake (orchestration etc)
* Add Windows support in terms of audio (QUITE IMPORTANT).
* Cleanup the pulseaudio backend. Code is hot garbage.
* VOIP chat for fun? Or write like a library and have a different app call helpers.
