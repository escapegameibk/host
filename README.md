% Documentation for the Escape Game Innsbruck's Host

# Host

## About
This is the host for games at the escape game innsbruck. It's main purpose is,
to control an escape room. Afterwars, it became apparent to me, that it would
also be kinda suitable as a home-automation system.

## Interface

The interface to anything wanting to communicate with the host is provided
at /var/run/escape/socket. It takes queries in form of a json object and
answers in form of a json object.

- An element called ´action´ which states what type of action the
command is

- Any other value as defined by the parser.

Documentation on how the commands are parsed and executed may be found in 
src/interface.c. Further documentation is a TODO.

## Serial interface 

A serial connection to a µc is established at application start. Documentation
on how the protocol works can be found in ../arduino/README.md.

Audio is played via the libvlc.

## License
This program is licensed under the GPLv3 or any later version of the license,
which permits you to use it commercially, but requires you to give users the
source code. in any case, we don't sell the software itself any way,
so it doesn't really matter.

## Building

The program only requires the linux operating system. 

### Dependencies

As of now, the program has the following dependencies:

- json-c >= 0.13
- libvlc >= 0.12
- gcc >= 5.0
- make

The vlc includes are required to be in <vlc/> and the json-c includes are 
required to be in <json-c/> of the global include path, as these are hard-coded
into the program. This is meant ot change in the near future, or as soon as i
find time to correctly configure the autotools.

The json-c version of the \*bian operating system is heavily outdated, as it
is version 0.12. Therefore the regular package doesn't work. You have to build
it from scratch. The latest version can be downloaded at:

https://s3.amazonaws.com/json-c_releases/releases/index.html

Extract the downloaded tarball, go into the directory, execute the command
'autogen', then run './configure --prefix=/usr --enable-threading', 'make',
'make check' and 'sudo make install'. This should install the nescessary
librarys and headers in the rght directories, and should allow the linker to
find them.

### Building an executable

Building an executable is as easy as typing 'make' into the commandline of any
linux terminal.

The executable may be found at 'bin/host' afterwards, and should have been
compiled for the target architecture. In case this will ever be needed (i
do hopefor gods sake that it will not) support for non 8 bit per byte platorms
has been implemented. 

The executable itself is by default dynamicallylinked, so taking it from one
computer to another should be possible, if the nescessary packages are installed
and the libldlinux can find the libraries.

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
 - "hints": an array containing hints in a special order. please consult the
   chapter on hint configuration for further information.
 - "hint_dependencies" : Dependencies used for auto hinting. Please consult the
   the chapter on autohinting for more information.
 - "mtsp_device" : The mtsp device to which to connect. Can be ommitted and
   will be replaced with "/dev/ttyUSB0".
 - "mtsp_baud" : The mtsp baud rate with which to connect onto the bus. Can be
   ommited and will be replaced with B460800
 - "langs" : An array of strings representing the language. These values are
   never touched inside of the program.
 - "default_lang" : An integer defining which language is used at startup.

This is where it get's a bit more complicated. I will try to explain the
construction of the "events" array now to you. The events array contains an
array of "events" or actions which have triggers and dependencies. If all
dependencies of an action are fullfilled, the action will be triggered and all
of it's triggers will be executed. So each element of the events array has to
contain at least the following fields:

 - "name": The event name
 - "dependencies": an array containing all of the dependencies which have to be
   fullfilled in order to trigger the event
 - "triggers": an array containing all triggers to be executed in case the
   event is triggered. 

### Dependencies

Dependencies are checked at a regular interval and passwd over to the modules.
If a dependency is met, the next dependcy is checked. If all dependencies are
met, the triggeres are executed, and the event is beeing marked as triggered.
A dependency only needs to specify it's module name with the "module" string.
Optionally a name may be given via the "name" field. This is useful for debug
output. It cannot be assuemd, that dependency checks are run in a certain
interval. It may be possible for a check to only occure from every few milli-
seconds to every hour! If your backend module relies on checks in a certain
interval, please build a cache.

### Triggers

Triggers are executed after all dependencys of an event are fullfilled, or if
an event has been triggered manually. If one trigger fails to execute, the
complete event gets reset, which doesn't mean, that i attempt to do magic
with the triggers, but that i reset the trigger state of the event and continue
as usual. A trigger must contain any module value and may contain any other 
values.

## Modules

All modules get their respective dependencies before initialisation to prepare
them for further actions. Nested dependencies are possible, but the module has
to initialize it's dependencies in order for them to work. Afterwards the
modules get initialized. 

### CORE

The core module contains all game relevant triggers and dependencys. It
communicate directly with the host's internal states and attempts to give a
clear interface to the user.

#### Dependencies

The core module is specified via the module field inside of a dependency. It
REQUIRES a type field to be specified. This type field contains information on
what type of dependency it is. The following types have been defined as of now:

1. event:
	Event dependencies are fullfilled if the event-id integer specified 
	inside the event field. Optionally a target field may be specified 
	containing a boolean which will be compared agaist the event's trigger-
	state.
2. sequence:
	Sequence dependencies are fullfilled, if a specific sequence of sub-
	dependencies is matched to a target. Imagine you want a series of
	inputs from a user in the right order. That's waht i would call a 
	sequence. Sequences require at the very least a squence array field and
	a dependencies array field to be specified. The dependencies array field
	may contain any amount of dependencies which is larger than 1, wheter
	this makes sense or not is totally up to you. The dependencies specified
	inside of this field are all treated as if they were regular
	dependencies. They may contain ANY other dependency, if you want to even
	another sequence may be specified in here. The sequence field consists
	of an array containing integer values. It specifies in which order the
	dependencies have to be triggered. If you for example specify 2,1,0,
	the first dependency to trigger would be the first one, the second the
	second one, and the third the third one. Yes arrays start at 0. This
	means the first element is the last one in the sequence array.
3. flank:
	A flank dependency is updated when it's read. This dependency has 1
	subdependency declared in a "dependency" field. The flank dependency
	is forefilled, in case the sub-dependency changes state. By default, any
	state-change triggers the flank dependency. The boolean fields "high"
	and "low" may specify, which change forefilles the dependency, with
	"low" meaning a change from 1 to 0 and "high" meaning a change from 0
	to 1. Both values are assumed to be true, and therefore forefilling the
	dependency, by default.
4. or:
	This dependency represents a logical or. It requires a "dependencies"
	field, of which it will check all dependencies, until one of them is
	forefilled, in which case it will return true. If it reaches the end
	of the array, without ever encountering a forefilled dependency,
	the dependency is considered not forefilled. 

#### Triggers 
The core module is specified via the module field inside of a trigger.
It REQUIRES an action field to be specified. It may contain any of the
following actions:

1. timer_start:
	This action starts the timer and sets it's start time to the current
	unix time. No additional fields are required.

2. timer_stop:
	This action causes the startend to be set to the actual unix time,
	therefore causing the timer to stop. No additional fields required.
3. timer_reset:
	This action causes the timer values timer_start and timer_stop to be
	cleared.
4. reset:
	Reset all game critical things and modules to their initial state. This
	DOESN't cause the game to automatically reset all values, but rather the
	modules to perform a cleanup. It also resets all events to be not
	triggered.  No additional fields are required.

5. delay:
	A delay is performed. As all triggeres are executed in serial, this
	can be used to wait a bit before triggereing the next thing. It
	requires a delay field to be specified containing the time to be slept
	in milliseconds aka 1s/1000.
6. alarm: 
	This triggers the alarm. It is part of the alarm system and, in any
	case, activates the alarm. if it is already running, it will keep
	running. No additional fields are required.
7.alarm_release:
	Releases the alarm. If the alarm isn't running, it will stay in that 
	state. No additional fields are required.
### SND

The sound module aka snd is specified inside the module section of a trigger
with the value snd. Specifying the sound module inside a dependency is not 
valid as of the time of writing. 

#### Triggers 

The sound module requires an action field to be specified. This action field
specifies what the sound module should do. The following actions have been
specified as of now:

1. play:
	This action tells the sound module to play whatever URL is specified
	inside the resource field. This may be a http link, a file link, or
	anything else that can be passed to the libvlc without error.

2. reset:
	This clears ALL currently playing sounds. No additional fields are 
	required.
3. effect:
	This action tells the sound module to play whatever URL is specified
	inside the resource field. This differes from the play action, that
	it is only possible to play one effect at a time. if a new effect is
	played, the old effect gets cleared.

#### Multilanguage sounds

In order to be able to play multilanguage sounds, it was nescessary to
implement it in a way, that avoids regression. The implementation, therefore,
is basically the same as for the name object. Any amount of languages may be
specified by putting an array of urls in the position of the "resource" field
of the play and effect actions. The rest is basically the same as for the name
object, and if the "resource" field contains a string, the string is used for
any language. 

### MTSP

The module called MTSP which is an acronym for *Minimum Transport Security
Protocol* provides an interface for hardware from the esd team, a russian group
of people which thought using a 480600 baud connection for more than 2m would
be a good idea. It heavily uses caching and error checking as it is highly
propable, that messages get lost from this protocol. It can be trigger, as well
as dependency, and is specified as mtsp in the module field.

#### Dependencies

Dependencies are cached, and therefore have very low latencies to access. A
dependency is required to contain at least a device field, a register field,
and a target field. If one of the above is not present, the program will either
crash or error-out! The target specifies a 32 bit unsigned integer which is
compared to the return value of the mtsp device and port.

#### Triggers

Triggeres are real time, and if something fails to trigger, the ENTIRE event
is reset and may only be re-triggered from the very beginning. Triggeres are not
checked during initialisation. A trigger requires a device, register and target
to be specified. The target is written to the device at the specified register.

## Hints

Hins are, well obviously, meant to help the players. The global field "hints"
is a 3-dimensional array. The first array contains hints correlating to the
events array. So each array in the second dimension, is directly linked to the
corresponding event, in the event array. The second dimension contains arrays of
triggers, with each array representing a hint, so an event may have any amount
of hints, iwith each any amount of triggers.

### Auto hinting

The host is capable of automatically executing hints. As it's aim was to be as
capable as possible, the auto-hinting system uses the global hint_depedencies
field as reference for triggering. The hint_dependencies field contains a 
2-dimensional array. The first array, defines the depth of hints, so the amount
of hints for each event to be triggerable. The second dimension contains
dependencies. If all dependencies are evaluated to be true, the highest
triggered event get's looked up, it is decided, which event is set to be
triggered the next time, and the correlating hint is executed. So for example if
the latest event which has been executed is 10, and all dependencies are clear
to execute in the first array, the hint 11,0 get's executed.

## Language

The "langs" field, which is an array of strings, declares, in which languages
the game is available. The languages are internally represented by the
corresponding index in the array. The "default_lang" inside of the root object
defines, which index is used by default. 

# Cutting stuff out

Some things may be completely omitted via a preprocessor definition. You can,
for example, add a -D NOMTSP to the gcc comandline inside the Makefile. The
following is a list of these definitions:

- NOMTSP : Completely disables and removes the MTSP module.
- COLOR : Enables Color in the debug output.
- HEADLESS : Completely disables and removes the interface.
- NOHINTS : Completely disables ad removes the Hinting system.
- NOALARM : Completely disables and removes the alarm system.

# ERRATA
## Raspberry Pi

If the host is a raspberry pi and the native USART connection is used, it is
known that it won't work, unless you change the following things:

- Enable uart in the config.txt with the enable_uart=1 option
- Disable bluetooth with the dtoverlay=pi3-disable-bt option
- Remove the uart parameter from the kernel command line

If you forget to disable bluetooth you will be able to send stuff via uart, but
you are unable to controll the clock speed and if bluetooth changes clock speed,
so does your connection. This is probably not what you want.
