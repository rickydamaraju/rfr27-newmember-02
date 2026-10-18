AVR_CC ?= avr-gcc
AVR_OBJCOPY ?= avr-objcopy
AVR_SIZE ?= avr-size
CFLAGS = -std=c11 -Os -Wall -Wextra -Werror -mmcu=atmega328p -DF_CPU=16000000UL -ffunction-sections -fdata-sections

all: build/temperature-monitor.hex
build:
	mkdir -p build
build/temperature-monitor.elf: main.c monitor.c monitor.h calibration.h | build
	$(AVR_CC) $(CFLAGS) -Wl,--gc-sections main.c monitor.c -o $@
	$(AVR_SIZE) $@
build/temperature-monitor.hex: build/temperature-monitor.elf
	$(AVR_OBJCOPY) -O ihex -R .eeprom $< $@
host-test: | build
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g monitor.c ../tests/test_monitor.c -o build/test_monitor
	./build/test_monitor
.PHONY: all host-test
