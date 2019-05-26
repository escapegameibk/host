# Host

## About

This is the host for games at the escape game innsbruck. It's main purpose is,
to control an escape room. Afterwars, it became apparent to me, that it would
also be kinda suitable as a home-automation system.

## License

This program is licensed under the GPLv3 or any later version of the license,
which permits you to use it commercially, but requires you to give users the
source code and the right to re-sell the program. in any case, we don't sell the
software itself any way, so it doesn't really matter, or at least that's the 
idea.

## Building

The program only requires the linux operating system... Well kinda. It also
needs it's dependencies. I won't tell you how to install linux, but I will
describe the program's dependencies:

### Dependencies

As of now, the program has the following dependencies:

- json-c >= 0.13
- libvlc >= 0.12
- gcc >= 5.0
- make

### Include directories

The vlc includes are required to be in <vlc/> and the json-c includes are 
required to be in <json-c/> of the global include path, as these are hard-coded
into the program. This is meant ot change in the future, or as soon as i find 
time to correctly configure autotools.

#### LIBVLC

The simplest way is to install the 'vlc', 'libvlc-core' and 'libvlc-dev
packages on a debian based system, or if you are running an ArchLinux
based system, the package vlc does the same thing. The dependencies should
allow the rest to work fine. You may need to install vlc's dependencies if you
would like to use some of it's features.

If the sound module is disabled via the NOSND option, this dependency may be 
ignored.

#### JSON-C

The json-c version of the \*bian operating system is *heavily* outdated, as it
is version 0.12. Therefore the regular package doesn't work. You have to build
it from scratch. The latest version can be downloaded at:

https://s3.amazonaws.com/json-c_releases/releases/index.html

Extract the downloaded tarball, go into the directory, it may be nescessary to 
execute the command
'autogen', then run './configure --prefix=/usr --enable-threading', 'make',
'make check' and 'sudo make install'. This should install the nescessary
librarys and headers in the rght directories, and should allow the linker to
find them.

Building json-c requires autotools, the gcc and libtool to be installed.

### Building an executable

The program's build system is at this point very basic. It's simply a Makefile
which is written in GNU Make. The Makefile compiles all files in the src/
directory and links them together to form an executable.

The executable may be found at 'bin/host' afterwards, and should have been
compiled for the hosts's architecture..

The executable itself is by default dynamically linked, so taking it from one
computer to another should be possible, if the nescessary packages are installed
and the loader can find the libraries.

## Links

LibVLC: https://wiki.videolan.org/LibVLC/

JSON-C: https://json-c.github.io/json-c/

## cross-compilation

As of now i have no clue how to make cross-compilation a thing for this. This is
a TODO.

# Configuration

The host is configured via one json object stored in a file. The file's location
is determined in the following order:

 - the comand line paramter -c has been specified and points to a file
 - the working directory contains a file called "config.json"

If none of the above exists or is a valid json, the host will return with exit
code EXIT_FAILURE.

A few example configurations may be found in examples/configurations/.

## General configuration

The following fields may be specified on the upper most level of the
json file:

 - "name": The name of the game. It can be ommited and will be replaced with
   "UNSPECIFIED" or NULL. String value.
 - "duration": the normal duration of a game in seconds. it can be ommited and
   will default to 3600s or 1h. Integer value. **omitting this field is discouraged**
 - "boot_sound": A URL pointing to an audio file which is beeing played on
   startup. Requires snd module. String value.
 - "events": an array containing all events to be executed. more information
   should follow this list. Array of event objects.
 - "hints": an array containing hints in a special order. please consult the
   chapter on hint configuration for further information. Array of arrays of
   hint objects.
 - "hint_dependencies" : Dependencies used for auto hinting. Please consult the
   the chapter on autohinting for more information. Array of dependency arrays.
 - "mtsp_device" : The mtsp device to which to connect. Can be ommitted and
   will be replaced with "/dev/ttyUSB0". String value.
 - "mtsp_baud" : The mtsp baud rate with which to connect onto the bus. Can be
   ommited and will be replaced with 460800. Integer value
 - "langs" : An array of strings representing the language. These values are
   never used inside the host and only for frontends. Array of strings.
 - "default_lang" : An integer defining which language is used at startup.
 - "mtsp_device" : Defines the device of the mtsp connection. For further 
   details please see the mtsp section further down. String value.
 - "mtsp_baud" : Defines the baud rate of the mtsp connection. For further 
   details please see the mtsp section further down. Integer value.
 - "ecp_device" : Defines the device of the mecp connection. For further 
   details please see the ecp section further down. String value
 - "ecp_baud" : Defines the baud rate of the ecp connection. For further 
   details please see the ecp section further down. Integer value.
 - "log_level": Defines the level to which output is surpressed. Possible
   Values are (in order of output): DEBUG_ALL, DEBUG_MOST, DEBUG_MORE, DEBUG,
   INFO, WARNING, ERROR; String value.
- "modules": Defines the modules that the ecp configuration wants. Is an array
   of module identifier strings. If ommitted ALL avaliable modules will be 
   loaded. Array of strings.

This is where it get's a bit more complicated. I will now try to explain the
construction of the "events" array to you. The events array contains an
array of "events" or actions which have triggers and dependencies. If all
dependencies of such an event are fullfilled, the action will be triggered and 
all of it's triggers will be executed. So each element of the events array has 
to contain at least the following fields:

 - "name": The event's name. Follows the regular handling for multilanguage
   name objects.

 - "dependencies": an array containing all of the dependencies which have to be
   fullfilled in order to trigger the event

 - "triggers": an array containing all triggers to be executed in case the
   event is triggered.

Optionally, the following fields may be specified:

 - "autoreset" : Defines wether an event is able to reset itself. The reset of
    the event is performed, as soon as one of it's dependencies is changeing 
    state to false. This is done in order to avoid instantly triggering the
    event again. The default value for this field is false.

 - "hidden" : Defines wether an event is shown in the interface. In case this is
   set to true, the interface will not show any traces of the event, and it will
   not be controllable. The default value for this field is false. In case the
   interface module is not loaded, this field has no effect.

 - "hintable" : Defines wether an event is taken into account for the 
   auto-hinting system. If this is set to false, the auto-hinting calculation
   should not use this event, and not attempt to exectute any of it's 
   dependencies. The default value for this field is true. In case the hint 
   module is not loaded, this field has no effect.

An event is a thing that should occure over the curse of a game. The idea
behind it is, that an event has triggers and dependencies. In case all
dependencies are met, the event's triggers should be executed and the event
should be marked that way. An event (at this point) cannot reset itself,
therefore any reset activity should only unset the previously set marker.
Implementation of reset triggers has been considered, but is thus far a TODO.

### Dependencies

Dependencies are checked at a regular interval and passwed over to the modules
regarding it's correct handling.
If a dependency is met, the next dependency is checked. If all dependencies are
met, the triggeres are executed, and the event is beeing marked as triggered.
A dependency only needs to specify it's module name with the "module" string.
Optionally a name may be given via the "name" field. This is useful for debug
output. It cannot be assuemd, that dependency checks are run in a certain
interval. It may be possible for a check to only occure from every few milli-
seconds to every hour! If your backend module relies on checks in a certain
interval, please build a cache.

#### Internally used fields

This may be important to you if you want to build your own modules or something
like that. Inside of dependencies the event_id AND id fields are reserved and
used internally. The event_id field is used to associate a given dependency with
it's parent event and the id field is used internally and contains a unique id
to differentiate dependencies from eachother. As saied before, PLEASE don't
override these fields or specify them manually.

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
modules get initialized, and then started. During module initialisation, the
module should get all its needed information, initialize datastructures and
prepare for regular operation. This step may take any time. During module
start it should start caching threads, perform sanity checks and is now
able to assume that other modules have had their data structures initialized
as well.

### Interface

The interface to anything wanting to communicate with the host is provided
at /var/run/escape/socket. It takes queries in form of a json object and
answers in form of a json object. The request has the following parts:

- An element called ´action´ which states what type of action the
  command is

- Any other value(s) as defined by the parser for that specific action

Documentation on how the commands are parsed and executed may be found in 
src/interface.c, where they are parsed inside a switch statement.
Further documentation is a TODO.

It is currently not possible to configure the interface during runtime via
triggers, or depend on internal state via dependencies.

### Core

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

	Additionally on updates triggers may be executed. In case the trigger
	are always wanted to be executed, the "update_trigger" field may be used
	to specify an array of triggers. In case the trigger is only wanted to
	be executed when the added dependency was a correct one for the 
	sequence, the "trigger_right" field may be used. In case the opposite
	is wanted, and you want a bunch of triggers to execute if the added
	dependency is wrong, the "trigger_wrong" field may be used. The very
	first specified field 

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

5. never:
	The never dependency is, as you may have guessed, never forefilled. It
	requires no special fields.

6. length:
	A length dependency is, by default, forefilled, if it's sub dependency
	(specified with the "dependency" field) has been forefilled for >= the 
	amount of seconds sepcified with the "length" field. It has been
	implemented to support the Escape Game Munich orphanage behaviour.
	Optionally, the below and/or above fields may be specified, to specify,
	wether the sub-dependency has to be continuously forefilled for
	> length, <length respectively. If the specifed length is equal to the
	expired length, the dependency is ALWAYS forefilled.

	Additionally a fail_triggers, a success_end_triggers or a start_triggers
	field may be specified, containing an array of triggers, which will be
	run at the end of a continuous forefillment, if it was forefilled at the
	end of a continuous forefillment, if it was not forefilled, or if the
	forefillment has started respectively.

7. and:
	This dependency represents a logical and. It requires a "dependencies"
	field, of which it will check all dependencys, until it encounters one,
	which is NOT forefilled, in which case it will abort and return 0. In
	case all dependencies are forefilled, the dependency is forefilled.

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

7. alarm_release:
	Releases the alarm. If the alarm isn't running, it will stay off.
	No additional fields are required.

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

The module called MTSP which is an acronym for **M**inimum **T**ransport 
**S**ecurity
**P**rotocol provides an interface for hardware from the esd team, a russian group
of people which thought using a 480600 baud connection for more than 100m would
be a good idea. It heavily uses caching and error checking as it is highly
probable, that messages get lost from this protocol. It can be trigger, as well
as dependency, and is specified as mtsp in the module field. The MTSP takes 
two global fields: mtsp_baud and mtsp_device, which specify the baud rate and
the device to connect to respectively. On a linux system, the baud rate may be
for example 57600, and the device may be for example "/dev/ttyUSB0". By default,
the baud rate is 460800 and the device is "/dev/ttyUSB0". These values may 
change over time, and should be specified inside the configuration. Not 
specifying them is discouraged. For further documentation on 
the MTSP please see the documentation repository.

#### Dependencies

Dependencies are cached, and therefore have very low latencies to access. A
dependency is required to contain at least a device field, a register field,
and a target field. The target specifies a 32 bit unsigned integer which is
compared to the return value of the mtsp device and port. Any value in any
length is set to be 32 bit wide in order to simplify the protocol.

#### Triggers

Triggeres are real-time, and if something fails to trigger, the ENTIRE event
is reset and may only be re-triggered from the very beginning. Triggers are
checked during initialisation. A trigger requires a device, register and target
to be specified with positive integer values. The target is written to the 
device at the specified register.

### ECPROTO

The ecproto or shortened "ecp" module is responsible for connection handling
with ecproto capable devices. The ecproto protocol definition may be looked at 
at ../microcontroller/ECPROTO.md. The ECPROTO module is caching all requests to 
the hardware and never allows direct hardware access for reading, therefore all
read requests are only accessing a cache and are never triggering a real
hardware request. The ecproto allows for almost passive updates, and allows a
client to only send what is needed to the master. The protocol also
supports acively polling the desired values, though this module does not support
this feature.

Global configuration values are:

- ecp_device: Specifies the ecp device to connect to, for example "/dev/ttyS1".
  By default the value is "/dev/ttyACM0". Configuration of this value is
  recommended.

- ecp_baud: Specifies the ecp device baud rate, for example 115200. By default
  the value 38400 is assumed. Configuration of this value is recommended.

#### Dependencies

The ecproto module currently has these types of dependencies, each one declared
with the type field within the dependency:

- port: A port dependency is a requesting the state of a gpio pin. It requires
  the device, register and bit fields to be populated with a integer, a 
  character and an integer respectively. It has the following optional values:
  pulled (boolean value which specifies wether pullup-resistors are to be 
  activated if possible, assumed to be true by default), isinput (boolean value 
  which specifies wether the pin is used as an input pin or not, assumed by 
  default to be true);

- analog: DEPRECATED. Kept for compliance with old ecp systems. !If you still
  maintain things using this, please update them to the adc type!

- mfrc522: Specifies an MFRC522 dependency. MFRC522 are SPI, I²C and UART
  capable ICs from Mifare Semiconductor used for RFID communication. You know
  those little blue access keys right? These cna be controlled via a MFRC522
  IC. An MFRC522 dependency requires the device, tag and tag_id fields to be
  populated with integers respectively, where the tag specifies which MFRC522
  connection on the device is to be used, and the tag_id specifies the desired
  values.

- adc: Specifies an analog signal which has been converted to a digital value.
  The threshold, value and channel fields ned to populated with a string, an
  integer and an integer respectively. The threshold field specifies, in what
  way the value field should be interpreted and can have the value "above",
  "below" or "is" respectively. The value field specifies the point at which the
  dependency is considered fullfulled. The channel field specifies the device's
  ADC channel to be used.

if not specified, the port dependency is automatically selected.

#### Triggers

The ecproto module currently supports the following types of triggers, each one
specified with the type field within the trigger:

- port: A port trigger specifies, that a GPIO pin's value should be changed 
	to the desired value. The port dependency needs the device, register and
	bit fields to be populated with an integer, a character and an integer
	respectively. A target should also be specified, what the desired value
	of the GPIO pin is. In case this is not specified, HIGH is assumed.

- secondary_trans: Secondary transmission triggers are built into a device
	for the sole purpose of relaying information. It specifies, that the 
	string specified within the string field to whatever the device has
	built-in as secondary means of communication. The null-termination is 
	not transmitted. The device field also needs to be poulated.

- pwm: PWM triggers set the current PWM values of a device's counter. The
	pwm dependency needs the device, counter, output and value fields to be
	specified with integers. The counter specifies what counter is meant,
	the output specifies what mux output is meant and the value what value
	to set the PWM to. The counter and output fields are device specific,
	the value field may no exceed 255.

## Hints

Hints are, well obviously, meant to help the players. The global field "hints"
is a 2-dimensional array. The first array contains hints correlating to the
events array. So each array in the second dimension, is directly linked to the
corresponding event, in the event array. The second dimension contains hint
objects. Inside a hint object the name may be specified, as well as the 
contents field, which is a multilanguage object defining what the hint "says".
This was meant for audio hints to write the sentences spoken in the hints inside
there. The hint object also contains a triggers field, which contains an array
of trigger objects triggered in case the hint is executed.

A hidden event does NOT change the game's behaviour of correlating hints to
events as the hidden flag is only of interest to the interface.

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

## Controls

This was added as a way to add controls to the interface and add linear
actuators for light, without the need for an event for all that. Controls
are things that execute triggers when they are updated. The type field
would specify what kind of trigger that would be, but thus far, only a linear
type has been implemented, which should represent a slider in the interface. The
linear field requires the min, max, step, initial, value and trigger fields
 to be populated wih an integer, an integer an integer, 

## Language

The "langs" field, which is an array of strings, declares, in which languages
the game is available. The languages are internally represented by the
corresponding index in the array. The "default_lang" field in the root 
object defines, which index is used by default.

# Cutting stuff out

Some things may be completely omitted via a preprocessor definition. You can,
for example, add a -D NOMTSP to the gcc comandline inside the Makefile, to cut
out the MTSP module. This might be done to minimize the size of the binary, or
if a module isn't nescessary and you would like to avoid it's dependencies.
The following is a list of these definitions:

- NOMTSP : Completely disables and removes the MTSP module.
- COLOR : Enables Color in the debug output.
- HEADLESS : Completely disables and removes the interface module.
- NOHINTS : Completely disables ad removes the Hinting system.
- NOALARM : Completely disables and removes the alarm system.
- NOEC : Completely disables and removes the ECPROTO module.
- NOLOL : Completely disables and removes the LOLPROTO
- NOVIDEO : Completely disables and removes the Escape game video system.
- NOSOUND : Completely disables and removes sound capabilities.
- NORECOVER : Don't attempt message recovery. Flag for the ECPROTO
- AGGRESSIVE_RECOVER: Aggressively attempts to recover any device errors.
- NOMEMLOG :  Disable logging to log file.

# Maintainers

This is a list of people involved in this project, what they did and how to
contact them:

- tyrolyean@tyrolyean.net:	Everything to date.

# ERRATA

## Raspberry Pi

If the host is a raspberry pi and the native USART connection is used, it is
known that it won't work, unless you change the following things:

- Enable uart in the config.txt with the enable_uart=1 option
- Disable bluetooth with the dtoverlay=pi3-disable-bt option
- Remove the uart parameter from the kernel command line

If you forget to disable bluetooth you will be able to send stuff via uart, but
you are unable to control the clock speed and if bluetooth changes clock speed,
so does your connection. This is probably not what you want.

## Special device capabilities regression

After I discovered that I created an regression where I basically had to 
assume that a device is GPIO capable, I made first steps to remove this
regression from future versions, but first all installations need to be
updated. This is still under way...

# TODO

## Postgresql

It was requested, that all data may be dumped into a postgresql database for
later analysys.

## Camera

Maybe a camera control interface would be a good idea to be able to control the
entire room by one thing. Though most of that would fall to the the interface.
