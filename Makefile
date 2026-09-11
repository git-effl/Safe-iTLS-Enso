TARGET = safe-itls-enso
OBJS   = main.o

LIBS = -ltaihen_stub -lSceLibKernel_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc

# Add -nostartfiles to LDFLAGS to prevent crt0.o from looking for main()
CFLAGS  = -Wall -O2
LDFLAGS = -Wl,-q -nostartfiles

all: $(TARGET).skprx

# Kernel self signing flag (-k)
%.skprx: %.velf
	vita-make-fself -c -k $< $@

# Added -s flag to denote a kernel module/plugin (if required by plugin.yml)
%.velf: %.elf
	vita-elf-create -e plugin.yml $< $@

# Link step now uses LDFLAGS containing -nostartfiles
$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(TARGET).skprx $(TARGET).velf $(TARGET).elf $(OBJS)
