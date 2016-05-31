BINDIR	= /opt/cross/avr/bin
CC	= $(BINDIR)/avr-gcc
LD	= $(BINDIR)/avr-ld
OBJCOPY	= $(BINDIR)/avr-objcopy
OBJDUMP	= $(BINDIR)/avr-objdump
AVRDUDE	= avrdude

MCU	= atmega328p
F_CPU	= 16000000

TARGET	= main

CFLAGS	= -Os -DF_CPU=$(F_CPU)UL -mmcu=$(MCU) -Wall -std=gnu99 \
	  -Wall -Wstrict-prototypes -Wa,-adhlns=$(<:.c=.lst) \
	  -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums

LDFLAGS	= -Wl,-Map=$(TARGET).map,--cref

SRCS	= $(wildcard *.c)
OBJS	= $(SRCS:.c=.o)

.PHONY: clean flash

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $^ $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

flash: $(TARGET).hex
	$(AVRDUDE) -F -c arduino -p atmega328p -P /dev/ttyACM0 -b 115200 -U flash:w:$(TARGET).hex
	minicom -D /dev/ttyACM0 -b 115200

clean:
	$(RM) $(OBJS) $(TARGET).hex $(TARGET).elf *.lst *.map *.d
