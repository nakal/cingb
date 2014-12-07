
# cingb

cingb is an ATTEMPT to write a Gameboy emulator for all kinds of platforms.
It plays Gameboy and Gameboy Color ROMs, includes a debugger for step-by-step
analysis of Gameboy programs and might be also interesting for educational
purposes.

It is a quite old piece of software written when I was still at school
in the mid-90s. It is perhaps not usable for your satisfaction, but I
have published it to share with people.

The only gameboy emulator that predates cingb was vgb which had some bugs
at that time. Some important information about the gameboy system has
been retrieved from this project.

## Supported systems

Quite a few. It is generally portable and I've heard of people who
ported it to a Texas Instruments calculator without many problems.

## Makefile & compilation (Unix/Solaris)

This is quite an early release of cingb.  You may need to change the 'Makefile'
(after the execution of ./configure) to compile the sources successfully.

So first start the configuration script with:
```
# ./configure
```

optionally you could configure your joypad
```
# ./cingb_conf
```

then run:
```
# make
```

If you want to install the binary for
regular use type:
```
# make install
```

### What to do if make fails

Eventually, you need to adjust the
variable-definitions.

#### INCPATH

* locate stdlib.h
* locate Xlib.h (without /X11)

#### LIBPATH

* locate libX11.a

Now, it should be possible to compile the sources.  If you get errors, while
running the compiled cingb, try to comment out the OPTMZ variable.  And don't
forget to read README.FAQ.

### Optional "make" syntax

#### make, make standard

Standard version supports:
- X
- framebuffer device

#### make glide

Glide version (requires root access).

#### make debug

Like standard, but implements debugger.

Do not execute the 'debug' version in
framebuffer mode, it's not possible.
You will need the terminal screen, so it's only
possible in X.

### Makefile & compilation (MS-DOS)

Requires:
* Cygwin package from http://www.cygwin.com/

Just type 'make -f Makefile.dos'.
CINGB.EXE should be created.

## Joypad configuration

cingb supports the sidewinder-joypad
and standard keyboards

### Default configuration

```
gameboy       	keyboard      	sidewinder

up		crsr up		up
down		crsr down       down
left		crsr left	left
right		crsr right	right
A		c		B
B		x		A
SELECT          d		M
START           s		start
```

#### NOTE

You should select the keys like on
a real gameboy, so that's why
gameboy A = sidewinder B and
gameboy B = sidewinder A !!!

### Other joypads

To configure other joypads, type:

```
# ./cingb_conf
```

## A warning about sound

1. It still sounds crap (ALPHA-release) !
    The gameboy sound chip has some more
    features which aren't implemented, yet.
2. On Sun Solaris the sound generation
    is very slow. Better switch it off
    with -n.

## Syntax

There are 3 types of executables so far:

- cingb
- cingb_deb
- cingb_glide

cingb can be started in X or on the
framebuffer console. cingb_deb can only be started in X.

The syntax of both versions is:
```
cingb [options] romfile

Options:    -d           execute in double-size mode
            -x           don't use XVideo extension
            -n           sound off
            -f freq      sound frequency (8000-44100)
            -o           force old standard gameboy mode (B/W)
            -c server    connect to server (dialog link)
            -h           start as server for dialog link
            -a code      use action-replay code (see below)
```

cingb_glide doesn't support "-d", it
will be ignored.  The glide version eventually needs root
access rights.

After starting the X-version can be stopped by pressing 'q'.

The framebuffer and Glide versions can be stopped by pressing:

* ctrl-C
* escape key
* q

Don't "kill -9" the emulator, or else your last status won't be saved.

## Action Replay Interface

*I hope it works properly (probably not).*

You can pass AR-codes by specifying "-a CODE -a CODE -a CODE ..." (up to eight
times). At start, the codes are NOT toggled on.  To enable/disable them press
RETURN while playing.

The codes look like this: **VVVVVVVV**

- eight characters
- every 'V' is a hex digit (0..9,A..F)

## Compatibility

These are games that are known not to work properly:

Double Dragon 3
- joypad doesn't work

Alladin, Warioland 2 GB/GBC (playable)
- problems with window alignment

## Other known problems

* There is a small bug in the
  graphical unit causing bugs
  when displaying "gameboy windows".

* You can have joypad emulation
  problems while executing some games.

* The sources won't compile, if you
  don't have enough virtual memory.

## Savegames (battery status)

The savegames will be stored in the same directory as the roms.
They have the extension '.GBS', but should be compatible to the '.SAV'-files
which are created by other emulators, so just copy or rename them.
(I think, these are raw SRAM-images without the last zeros.)

## That's it!

Have lots of fun studying it.
