CROSS	?= avr-
CC	 = $(CROSS)gcc
LD	 = $(CROSS)ld
OBJCOPY	 = $(CROSS)objcopy
AVRDUDE	 = avrdude

MCU	 = atmega328p
F_CPU	 = 16000000

TARGET	 = radiuno

COMMON_FLAGS  = -Os -std=c99 -g
COMMON_FLAGS += -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)

CFLAGS	 = $(COMMON_FLAGS)
CFLAGS	+= -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS	+= -Wall -Wstrict-prototypes -Wa,-adhlns=$(<:.c=.lst)

LDFLAGS	 = $(COMMON_FLAGS)
LDFLAGS	+= -Wl,-Map=$(TARGET).map,--cref

SRCS	 = $(wildcard src/*.c)
OBJS	 = $(SRCS:.c=.o)
LSTS	 = $(SRCS:.c=.lst)

.PHONY: clean flash

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $^ $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

flash: $(TARGET).hex
	$(AVRDUDE) -F -c arduino -p $(MCU) -P /dev/ttyACM0 -b 115200 -U flash:w:$(TARGET).hex
	picocom -b 115200 /dev/ttyACM0 || true

clean:
	$(RM) $(OBJS) $(LSTS) $(TARGET).hex $(TARGET).elf $(TARGET).map
