#
# Freescale Kinetis makefile
#
# Jared G
#

#
# Set up parameters, note we need
# std=gnu99 for both C99 AND using stuff like asm ("blah")
# debugging optimizations
#
CC = arm-none-eabi-gcc
CFLAGS = -std=gnu99 -g -Og -fmessage-length=0 -mcpu=cortex-m0 -mabi=aapcs -mthumb -msoft-float -IPDD

# Note: use gcc instead of ld for linking! otherwise mcpu and mthumb don't work
LD = arm-none-eabi-gcc
LDFLAGS = -nostartfiles -Xlinker -Map=firmware.map -mcpu=cortex-m0 -mabi=aapcs -mthumb -T ProcessorExpert.ld 
LIBS = 

SIZE = arm-none-eabi-size
OBJCOPY = arm-none-eabi-objcopy

#
# Local sources
#
OBJECTS = main.o systime.o spi_apa102.o flash.o uart_bus.o anim_engine.o

#
# Core make target
#
.PHONY: all
all: firmware.bin

#
# Linking here
#
firmware.elf: Cpu.o Events.o PE_LDD.o Vectors.o startup.o $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(SIZE) $@

#
# Converting here
#
firmware.bin: firmware.elf
	$(OBJCOPY) -O binary $< $@
	ls -al $@

#
# Clean target
#
.PHONY: clean
clean:
	rm -rf *.o *~ *.bin *.elf

#
# Load target and run GDB
#
.PHONY: gdbload
gdbload: firmware.elf
	arm-none-eabi-gdb firmware.elf -ex "target remote localhost:2331" -ex "load" -ex "monitor reset"

#
# Load target and run GDB to flash then exit
#
.PHONY: flash
flash: firmware.elf
	arm-none-eabi-gdb firmware.elf -ex "target remote localhost:2331" -ex "load" -ex "monitor reset" -ex "disconnect" -ex "quit"

