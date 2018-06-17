# Painter

## About
This is the backend for the painter at the escape game innsbruck. it basically
opens a unix socket at /tmp/escape and reads commands from there. Every command
has to be a valid json an has to contain the following things:

- An element called ´action´ which states what type of action the
command is

- Any other value as defined by the parser.

Documentation on how the commands are parsed and executed may be found in 
src/interface.c.

A serial connection to a µc is established at application start. Documentation
on how the protocol works can be found in ../arduino/README.md.

Audio is played via the libvlc.

## License
This program is licensed under the GPLv3 or any later version of the license,
which permits you to use it commercially, but requires you to give users the
code if they demand it. in any case, we don't sell the software itself any way,
so it doesn't really matter.

## Dependencies
As of now, the program has the following dependencies:

- json-c
- libvlc

## Links

LibVLC: https://wiki.videolan.org/LibVLC/

JSON-C: https://json-c.github.io/json-c/
