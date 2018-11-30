% Programmer's guide

# About

This file contains the programmers guide for the escape game innsbrucks host. It
was created to allow anyone else than the original programmer tyrolyean/waschtl
to add features, implement modules and do other stuff with the escape game 
system.

# General Principles

## Program structure

The Escape Game system is kept modular. Many parts of it may be removed, and all
other parts should handle that. The escape game system consists of three
parts fo most of it:

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

Another point was that c is fast as hell. it is a compiled languagem therefore
it is really really fast. This program was originally intended to run on a
raspberry pi 1st generation, which it should still do. On a raspberry pi when
running in idle. the program needs a few megabytes of ram, and 2 to 4 percent
of cpu load.

There you have it, that's why this program was written in c. 

## Where does this load of code even start

Where every c program starts, at the main. In this case the main is store inside
the src/main.c file. The main should parse the program parameters, pass them 
on to the desired functions, and then wait for the program to terminate.
If the program should terminate, the shutting down integer should be set to a 
value that evaluates to true. The main function should take at most 6 seconds 
to return after the variable was set.

## How were the functions inside this thing written. Is there a pattern

Yes, there is a pattern how i write functions, which is the one used in most c
programs:

Every function which doesn't return data is an integer, which should return < 0
on error, and >= 0 on success. I used this behaviour sometimes to put a bit
more information into the return value, by using multiple values below 0 to
indicate where the program failes, or values >0 to indicae anything that could
possibly be needed in the calling function.

If the function which returns data should return NULL on error. It is now the 
calling functions job to handle this.

## And wher should I start to read the code?

Well you can basically start wherever you want, as it may be a bit confusing in
the beginning to read tis code. it may be a good idea to follow the code
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

tyrolyean@tyrolyean.net		SUBJECT: ESCPE


