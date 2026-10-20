#
# Safe-iTLS-Enso Kernel Plugin Makefile
# Produces itls.skprx (taiHEN Kernel Module)
#

TARGET   = itls
OBJS     = src/main.o src/redirect.o

LIBS     = -ltaihenForKernel_stub \
           -lSceModulemgrForKernel_stub \
           -lSceIofilemgrForKernel_stub \
           -lSceSysmemForKernel_stub \
           -lSceClibForKernel_stub

PREFIX   = arm-vita-eabi
CC       = $(PREFIX)-gcc
CFLAGS   = -Wl,-q -Wall -O3 -Isrc -nostdlib
ASFLAGS  = $(CFLAGS)

all: $(TARGET).skprx

$(TARGET).skprx: $(TARGET).velf
	vita-make-fself -k -c $< $@

$(TARGET).velf: $(TARGET).elf
	$(PREFIX)-strip -g $<
	vita-elf-create -e exports.yml $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET).skprx $(TARGET).velf $(TARGET).elf $(OBJS)

.PHONY: all clean
