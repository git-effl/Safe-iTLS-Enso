#
# Safe-iTLS-Enso Companion App Makefile
# VitaSDK Toolchain Build Specification
#

TITLE_ID = ITLS00001
TARGET   = Safe-iTLS-Companion
SOURCES  = src/main.c \
           src/storage.c \
           src/downloader.c \
           src/hw_crypto.c \
           src/pup_extractor.c

OBJS     = $(SOURCES:.c=.o)

LIBS     = -lSceNet_stub \
           -lSceNetCtl_stub \
           -lSceHttp_stub \
           -lSceSsl_stub \
           -lSceCtrl_stub \
           -lScePower_stub \
           -lSceRtc_stub \
           -lSceSysmodule_stub \
           -ldebugscreen

PREFIX   = arm-vita-eabi
CC       = $(PREFIX)-gcc
CFLAGS   = -Wl,-q -Wall -O3 -Isrc -std=gnu11
ASFLAGS  = $(CFLAGS)

all: $(TARGET).vpk

$(TARGET).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		$@

eboot.bin: $(TARGET).velf
	vita-make-fself -c $< $@

$(TARGET).velf: $(TARGET).elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

param.sfo:
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "Safe-iTLS Companion" param.sfo

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) eboot.bin param.sfo

.PHONY: all clean
