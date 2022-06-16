CROSS	?= avr-
CC	 = $(CROSS)gcc
LD	 = $(CROSS)ld
OBJCOPY	 = $(CROSS)objcopy
AVRDUDE	 = avrdude

MCU	 = atmega328p
F_CPU	 = 16000000

TARGET	 = radiuno

COMMON_FLAGS  = -Os -std=c99 -flto -g
COMMON_FLAGS += -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)

CFLAGS	 = $(COMMON_FLAGS)
CFLAGS	+= -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS	+= -Wall -Wstrict-prototypes

LDFLAGS	 = $(COMMON_FLAGS)
LDFLAGS	+= -Wl,-Map=$(TARGET).map,--cref

# Dynamically generate a file containing the current git commit hash.
VERFILE = src/version.c
VERSION = $(shell git rev-parse --short=6 HEAD)

SRCS  = $(filter-out $(VERFILE),$(wildcard src/*.c src/*/*.c))
SRCS += $(VERFILE)
OBJS  = $(SRCS:.c=.o)
OBJS += src/banner.o

.PHONY: clean flash

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $^ $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/banner.o: src/banner.txt
	$(OBJCOPY) -I binary -O elf32-avr --rename-section .data=.progmem.data $^ $@

$(VERFILE):
	echo "const char version[] = \"$(VERSION)\";" > $@

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

flash: $(TARGET).hex
	$(AVRDUDE) -F -c arduino -p $(MCU) -P /dev/ttyACM0 -b 115200 -U flash:w:$(TARGET).hex
	picocom -b 115200 /dev/ttyACM0 || true

clean:
	$(RM) $(OBJS) $(TARGET).hex $(TARGET).elf $(TARGET).map
