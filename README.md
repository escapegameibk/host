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
source code. in any case, we don't sell the software itself any way,
so it doesn't really matter.

## Dependencies
As of now, the program has the following dependencies:

- json-c >= 0.13
- libvlc >= 0.12

## Links

LibVLC: https://wiki.videolan.org/LibVLC/

JSON-C: https://json-c.github.io/json-c/

# Configuration

The host is configured via a giant json object which will be located in the
following order:

 - the comand line paramter -c has been specified and points to a file
 - the working directory contains a file called "config.json"

if none of the above exists or is a valid json, the host will return with exit
code 1. 
## General configuration
The following fields may be specified on the upper most level of the
json file:

 - "name": The name of the game. It can be ommited and will be replaced with
   "UNSPECIFIED" or NULL.
 - "duration": the normal duration of a game in seconds. it can be ommited and
   will default to 3600s or 1h.
 - "boot_sound": A URL pointing to a audio file which is beeing played on
   startup. 
 - "events": an array containing all events to be executed. more inforamtion
   follows.

This 

# ERRATA

# Raspberry Pi

If the host is a raspberry pi and the native USART connection is used, it is
known that it won't work, unless you change the following things:

- Enable uart in the config.txt with the enable_uart=1 option
- Disable bluetooth with the dtoverlay=pi3-disable-bt option
- Remove the uart parameter from the kernel command line

If you forget to disable bluetooth you will be able to send stuff via uart, but
you are unable to controll the clock speed and if bluetooth changes clock speed,
so does your connection. This is probably not what you want.
