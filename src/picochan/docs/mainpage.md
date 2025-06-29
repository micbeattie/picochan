# Picochan - a channel subsystem for Raspberry Pi Pico

## Introduction

Picochan is
* library software that runs on Raspberry Pi Pico microcontrollers
  or, more generally, RP-series family chips such as RP2040 and RP2350.
* inspired by the I/O architecture of IBM mainframes which provides
  application-facing I/O machine instructions that trigger the
  _Channel Subsystem_ (CSS) to run asynchronous channel programs of
  _Channel Command Words_ (CCWs) communicated to a remote
  _Control Unit_ (CU) that performs the low-level device I/O
* implements a CSS and CU(s) as low-level libraries to drive available
  Pico peripherals (e.g. DMA, UART, PIO) that allow separating the
  CSS from CU on separate cores of one Pico or separate Picos
* licensed as Open Source
* not designed particularly to be a replacement or "better than" any
  existing way of writing software for Pico that does I/O, whether
  with plain software libraries or addition peripherals (e.g. USB)
  or hardware (e.g. breakout boards)
* written for interest in order to find out whether this I/O model
  which proved useful for mainframes and their programmers when
  introduced 60+ years ago is a useful model now that Pico
  microcontroller I/O capabilities have caught up with those of
  mainframes 30-40 years ago
* **NOT** compatible in any way with actual mainframe I/O software,
  hardware, channels, CUs or I/O or device hardware
* **NOT** anything that does or would make sense to port to or
  compile on actual mainframe hardware, whether old or new. This is
  fundamental since Picochan is written to use the low-level
  microcontroller-style "bit-banging" of DMA and pin-based
  peripherals (UART, PIO) in order to implement its functionality -
  the functionality which actual IBM mainframe architectures
  (from S/360 right up to modern z/Architecture) hide invisibly
  behind their actual hardware/firmware-implemented I/O architecture.

## [Channel Programs](@ref channel_program_page)

The Application API arranges for a series of I/O operations to happen
to/from a device by using the CSS to start a _channel program_ to
its subchannel that runs asynchronously, managed by the CSS.
Each I/O command - known as a _Channel Command Word_ (CCW) - in the
channel program gets the device to perform a "read-like" or
"write-like" command that send/receives (offers/requests) a
segment of data to/from the device. The command has an 8-bit
command code field so a device may simply implement a basic
"read" and/or "write" command or may offer a larger range of
more complex commands for application use.

CCWs can be chained together in sequences and loops (with simplistic
conditional branches) to form a channel program. Each CCW in the
channel program and its completion on success or error can be flagged
to notify the application by means of callbacks or interrupts.
The notifications can be either passive (with the channel program
continuing in parallel) or allowing suspend/resume that the program can
use to inspect and/or update the CCWs of the channel program as it
progresses.

For more information, see the
[Introduction to Channel Programs](@ref channel_program_page).

## Picochan APIs

For writing an application that uses the CSS to perform I/O
to devices on a CU, you use the Picochan application API for
using the CSS along with documentation provided by the
CU-side device driver author on what CCW commands are supported
for that device and what they do.

[Application API for using CSS](@ref css_api_page)

For writing a device driver that uses the CU to talk to a
specific kind of device and offers a useful set of CCWs for
a Picochan application on a CSS to use, you use the Picochan
application API for using the CU along with documentation
for the device you are driving.

[Device driver API for using CU](@ref cu_api_page)

## [Compiling](@ref compiling_page)

Picochan is written in C using the Pico SDK. It uses CMake in the
way recommended by that SDK. Similarly to how the Pico SDK divides
into multiple modules with names of the form `pico_foo` and
`hardware_foo`, Picochan is divided into three CMake modules:
* `picochan_base`
* `picochan_css`
* `picochan_cu`

For which modules to use for which purposes and how to use CMake
to compile your application and/or device driver programs, see
[Compiling](@ref compiling_page).

## [Design](@ref design_page)

There is a [brief summary](@ref design_page) of the design of
Picochan.
