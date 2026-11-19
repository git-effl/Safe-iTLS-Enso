#
# Safe-iTLS-Enso Kernel Plugin Makefile
# Produces itls.skprx (taiHEN Kernel Module)
#

TARGET   = itls
OBJS     = src/main.o src/redirect.o

LIBS     = -ltaihenForKernel_stub \
           -lSceIofilemgrForDriver_stub \
           -lSceSysclibForDriver_stub \
           -lSceDebugForDriver_stub

PREFIX   = arm-vita-eabi
CC       = $(PREFIX)-gcc
CFLAGS   = -Wall -O3 -Isrc -std=gnu99
LDFLAGS  = -Wl,-q -nostdlib

all: $(TARGET).skprx

$(TARGET).skprx: $(TARGET).velf
	vita-make-fself -c $< $@

$(TARGET).velf: $(TARGET).elf
	vita-elf-create -e exports.yml $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET).skprx $(TARGET).velf $(TARGET).elf $(OBJS)

.PHONY: all clean
