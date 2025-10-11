# Picochan - a channel subsystem for Raspberry Pi Pico

Picochan is library software that runs on Raspberry Pi Pico microcontrollers
or, more generally, RP-series family chips such as RP2040 and RP2350 and
implements an I/O architecture inspired by that of IBM mainframes.

In this architecture, the application-facing API triggers the
_Channel Subsystem_ (CSS) to run asynchronous channel programs of
_Channel Command Words_ (CCWs) communicated to a remote _Control Unit_ (CU)
that performs the low-level device I/O.

Whereas for a real mainframe CSS, CPU hardware instructions are the
API that drives the CSS, Picochan implements the CSS as low-level
libraries to drive available Pico peripherals (e.g. DMA, UART, PIO)
that allow separating the CSS from the CU on separate cores of one
Pico or separate Picos.

Picochan has been written for interest in order to find out whether
this I/O model which has proved useful for mainframes and their
programmers when introduced 60+ years ago is a useful model now that
Pico  microcontroller I/O capabilities have caught up with those of
mainframes 30-40 years ago.

It is **NOT** compatible in any way with actual mainframe I/O software,
hardware, channels, CUs or I/O or device hardware.

It is **NOT** anything that does or would make sense to port to or
compile on actual mainframe hardware, whether old or new.

Picochan is written in C using the
[Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk).

# Documentation

See the [main document page](src/picochan/docs/mainpage.md).

# Example code

There are some [examples](examples) you can build.

# License

Picochan is open source, licensed under the
[MIT License](https://spdx.org/licenses/MIT.html).

# About the author

Picochan has been written by me, Malcolm Beattie. Although I am
employed by IBM, I do not speak for IBM. Picochan is a personal
project unrelated to any IBM business.
