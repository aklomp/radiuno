```
______          _ _
| ___ \        | (_)
| |_/ /__ _  __| |_ _   _ _ __   ___
|    // _` |/ _` | | | | | '_ \ / _ \
| |\ \ (_| | (_| | | |_| | | | | (_) |
\_| \_\__,_|\__,_|_|\__,_|_| |_|\___/
```

# Radiuno

[![Build](https://github.com/aklomp/radiuno/actions/workflows/build.yml/badge.svg)](https://github.com/aklomp/radiuno/actions/workflows/build.yml)

A bare-metal commandline server for the Arduino Uno + [SparkFun si4735
shield](https://www.sparkfun.com/products/retired/10342), written virtually
from scratch in C99. It was developed and tested on a genuine Arduino Uno, but
the Uno is used as just another Atmel AVR board without all the Arduino
sugarcoating. Flash this code to a Uno, open a serial port terminal (like
`minicom` or `putty`) at 115200 baud 8N1, and get a commandline interface to
the si4735.

## Install

Requires `avr-gcc` and `avr-libc`. Being a bare-metal project, it doesn't
require any Arduino stuff. You'll need to edit the `Makefile` to point to your
avr cross-compilation environment. You might also need to change some other
specifics, such as the default USB device, `/dev/ttyACM0`. Then:

```sh
# Build image and flash to device
make flash
```

## Acknowledgements

The si4735 code was written with one eye on the datasheets and another on the
original [Trunet library](https://github.com/trunet/Si4735). Having a working
reference implementation proved indispensable for maintaining my sanity among
such questions as "why am I not getting data" (answer: typos) and "why is the
data that I'm getting corrupted" (answer: despite what the datasheet says, the
chip can't handle SPI transfer speeds of over 500 KHz).
