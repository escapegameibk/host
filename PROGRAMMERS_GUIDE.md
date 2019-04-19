% Programmer's guide

# About

This file contains the programmers guide for the escape game system's host. It
was created to allow anyone else than the original programmer tyrolyean/waschtl
to add features, implement modules and do other stuff with the escape game 
system. I am dearly sorry. This file is not yet complete. Please don't kill me.

# General Principles

## Program structure

The Escape Game system is kept modular. Many parts of it may be removed, and all
other parts should handle that. The escape game system consists of three
parts for most of it:

- The execution level, i.e. the microcontroller code for the ecproto.

	This layer should communicate directly with the hardware, though it may
	use any other protocol to do so. Emulation of hardware components may
	be possible.

- The controlling level, i.e. the escape game system's host, the part where this
	readme is from.

	This layer should tell the execution level waht to do, and when to do
	what. It performs hardware abstraction and handles automated executions
	of events and stuff.

- The presentation level, i.e. the frontends stored inside the frontends repo

	This layer presnts a ui (user interface). For example a website wich is
	able to sned commands to the conbtrolling level.

Each level should only talk to it's neighbours, therefore the execution level
should never have to directly interact with the presentation level. This allows
for any hardware which is capable to talk to the controlling level to be able
talk to the presentation level.

## What language does this thing use?

This program is written in the c programming language, compiled using the gnu
c compiler, which is part of the gnu compiler collection (gcc). This program
should be compatiable with the c99 standart, however it is compiled using the
gnu99 standart, because some features of it were used unintentionally. If those
few things were to be rewritten for the c99 standart, it should compile and run.
To find out what errors exist with the c99 standart, please add a -pedantic to
the CFLAGS line inside of the Makefile.

### Why was this completely insane language chosen for something so perfect for
###  object oriented programming.

I've heared that question a lot, and the answer is simple, I hate object 
oriented programming. In every school you learn how good and how perfect object
oriented programming is, that you can handle all erros so perfectly, and so on,
and so on. Then you go out into the real world, and what do you find? A whole
bunch of the most hacky solutions this world has ever seen, just because the
target didn't fit into the object oriented scheme that was used. Errors come 
from all sides, because everything may throw an exception at any time. Maybe
someone forgot to add a catch clause for this onw specific exception type,
it stacks to the main, and the program crashes with a stack trace. I don't want
to say, that c is flawless, helps you at correcting errors, or finding them.
I just don't want to lie to you. But in my opinion c programs tend to be more
confusing at the beginning, but make more sense the longer you look at them. If
you know the c programming language well enough, you can almost always tell
what a program does, whereas you have layers of abstraction inside a object
oriented programming language.

Another point was that c is fast as hell. it is a compiled language, therefore
it is really, really fast. This program was originally intended to run on a
raspberry pi 1st generation, which it should still do. On a raspberry pi when
running in idle, the program needs a few megabytes of ram, and 2 to 4 percent
of cpu load.

There you have it, that's why this program was written in c. Also because i
wanted to learn a bit of c and practice for programming my microcontrollers.

## Where does this load of code even start

Where every c program starts, at the main. In this case the main is store inside
the src/main.c file. The main should parse the program parameters, pass them 
on to the desired functions, and then wait for the program to terminate.
If the program should terminate, the shutting_down integer should be set to a 
value that evaluates to true. The main function should take at most 6 seconds 
to return after the variable was set. This time may vary though and was added
to allow a clean shutdown, even though in most cases this is not needed.

## How were the functions inside this thing written. Is there a pattern?

Yes, there is a pattern how i write functions, which is the one used in most c
programs:

Every function which doesn't return data is an integer, which should return < 0
on error, and >= 0 on success. I used this behaviour sometimes to put a bit
more information into the return value, by using multiple values below 0 to
indicate where the program failes, or values >0 to indicae anything that could
possibly be needed in the calling function.

If the function returns data, it should return NULL on error. It is now the 
calling functions job to handle this.

## And wher should I start to read the code?

Well you can basically start wherever you want, as it may be a bit confusing in
the beginning to read the code, and it may be a good idea to follow the code
execution from the beginning and start at the main. The code should then
continue to perform initialisation and startup tasks, send off a few threads to
handle stuff and finally come to a rest in the main with an almost infinite
loop, which may be interrupted from any place in the program by setting the
global shutdown variable to be true. I'm missing my point, you may start at
the main, or anywhere else. Whatever suits you best.

## Who are you?

I am a programmer i guess. However I don't really know how that question is
supposed to help you with programming this thing, so I'll just put my email in
here and leave:

tyrolyean@tyrolyean.net		SUBJECT: Escape Game System

# Modules

## History (An old man talking)

Now that that part is finished i want to write a few things about modules.

The Escape Game System is(as you may notice when you read my code) designed to
be modular. The original purpose for this program was to control an escape room
controlled by one protocol. That was it. That task was done quite fast, and it
occured to me, that the used protocol wasn't really good at it's thing at all.
Then I perceeded to think about the possibility of using the same program
for multiple protocols, and how I would allow it to do that. It turned out
that it wouldn't be that difficult, and I started re-writing some pieces of code
to allow it to be modular. This also worked out pretty well and the escape game
system was soon able to handle a variety of modules at the same time. A few days
ago I thought i should make the module loading a bit easier to look at, and
created the module_t struct and the corresponging modules array. It's main
task is to allow controlled loading of modules, showing a modules
dependencies, and (probably most important) make it easy to implement new
modules. Before that it has just been a few function calls scattered over a few
other functions. Now it's a bit easier to read and to implement.

## Current implementation

The current implementation follows a straight forward idea: provided what you
can. Any module can provide a dependency check, a trigger executor, etc. But if
it doesn't, it doesn't matter. This is archived via the module_t struct, which
consists of flags indicating the modules current status and function pointers to
the module's functions. Here comes the trick, if a module can't or simply
doesn't need to handle any of the nescessary available functions, it can just
define the function pointer to be NULL. Isn't that nice?

## What can a module do?

As of the time of writing, a module can handle the follwing functions:

### init dependency:

Dependencies are passed to all modules BEFORE initialisation of the module 
itself. At this point only ground functionality is provided by the host. The 
initial use for this was to allocate resources for the dependencies and
gain information for the init procedure.

This function is called as often as the module's name is specified within a
dependency. The dependency is then passed in the function's parameter. This
step may take as long as it needs.

### init module:

This function is called after all dependencies have been initialised. A bit
more functionality should now be available to the module. The module shoule now
do stuff like open devices, initialize internal values, prepare devices for
use, etc. No second threads should be started yet though.
This step may take as long as it needs.

### start module:

This function is called after all modules habe been initialised. Almost all
functions should be available by now. The module should now start secondary
threads needed for runtime updates etc. This step may take as long as it needs.

### check dependency:

This is used to check wether dependency from this module is forefilled or not.
In case it is > 0 may be returned, i case it isn't 0 may be returned and on
error < 0 may be returned.

This function is called at any time in any interval. This can range from 
a call every few microseconds to a call every few hours. A call to this function
can occure at any time after the initialisation step.

This function should be as fast as possible, because it (can) determine the game
watch speed (the speed at which the game performs a depenency check on all
events). If you think this function needs longer to execute in you module,
please build a cache!

The passed dependency is the dependecy to be checked. The float is used for
percentage-wise representation of the returned integer. In case percentage-wise
representation of the returned value is not supported, the value may be set to
the exact same value as the return value.

### execute trigger:

This is used to execute a trigger. It's return value is >=0 for success and
<0 for failure.

This function is called at any time in any interval. This can range from 
a call every few microseconds to a call every few hours. A call to this function
can occure at any time after the initialisation step.

This function may take as long as it needs. Any further execution is blocked 
until the function has returned. 

The boolean parameter indicates wether the function should actually modify
something, or whether it should just "pretend" to do something and give the
according return value. This is used to check, wether the trigger was correctly
defined.

### reset module:

This function is called on event reset. It is not meant to trigger a module
to change dependencies or execute triggers by itself, rather than to give the
module a good time to clean up internal stuff, synchronize buffers, etc.
A return value of >= 0 indicates success and a return value of <0 indicates
falilure.

This function may take as long as it needs. Any further execution is blocked 
until the function has returned. 

# Output

Output is performed via the log.c file. It provides the println function, which
takes 2+n arguments:

- A formating string:
	The formatting string is directly handed over to the printf function
	and uses the same syntax. An additional newline character is inserted at
	the string's end.

- An integer indicating the message type. An ENUM has been globally defined
	which currently has the values DEBUG, INFO, WARNING and ERROR. ANSI
	escape sequences may be used to generate colorful output. The log is
	by default also written to a log-file at the DEFAULT_LOGFILE location.
	STDOUT_FILENO and stdout are redirected to that file and the initial
	stdout.

- n additional parameters directly passed to printf.

