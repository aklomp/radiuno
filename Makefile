CROSS	= /opt/cross/avr/bin/avr-
CC	= $(CROSS)gcc
LD	= $(CROSS)ld
OBJCOPY	= $(CROSS)objcopy
OBJDUMP	= $(CROSS)objdump
AVRDUDE	= avrdude

MCU	= atmega328p
F_CPU	= 16000000

TARGET	= main

COMMON_FLAGS = -Os -std=c99 -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)

CFLAGS	 = $(COMMON_FLAGS)
CFLAGS	+= -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS	+= -Wall -Wstrict-prototypes -Wa,-adhlns=$(<:.c=.lst)

LDFLAGS	 = $(COMMON_FLAGS)
LDFLAGS	+= -Wl,-Map=$(TARGET).map,--cref

SRCS	= $(wildcard *.c)
OBJS	= $(SRCS:.c=.o)

.PHONY: clean flash

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $^ $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

flash: $(TARGET).hex
	$(AVRDUDE) -F -c arduino -p $(MCU) -P /dev/ttyACM0 -b 115200 -U flash:w:$(TARGET).hex
	minicom -D /dev/ttyACM0 -b 115200

clean:
	$(RM) $(OBJS) $(TARGET).hex $(TARGET).elf *.lst *.map *.d
