#!/bin/bash
avr-gcc -Wall -gdwarf-2 -O3 -std=gnu99 -mmcu=atmega128 \
    -DF_CPU=16000000 -ffunction-sections -fdata-sections \
    -Wl,--relax,--gc-sections \
    -Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 \
    -I../simavr/sim/avr \
    atmega128_mini_celeste.c \
    -o atmega128_mini_celeste.axf
avr-size atmega128_mini_celeste.axf|sed '1d'