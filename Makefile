# name for the target project
TARGET = binbro
#FLAVOUR = release
FLAVOR = debug

# relative to the project directory
BUILD = build
FIRMWARE	= $(BUILD)/firmware

# add xtensa toolchain to the path
XTENSA_TOOLS ?= $(CURDIR)/../esp-open-sdk/xtensa-lx106-elf/bin
PATH := $(XTENSA_TOOLS):$(PATH)

# # root directory of the ESP8266 SDK package, absolute
SDK	?= $(CURDIR)/../esp-open-sdk/sdk
LWIP ?= $(CURDIR)/../esp-open-sdk/esp-open-lwip

# # esptool.py path and port
ESPTOOL	?= $(XTENSA_TOOLS)/esptool.py
PORT ?= /dev/ttyUSB0
BAUD ?= 115200
# flash size 8Mbits - EN25Q80A 
# SPI flash mode - dual I/O
	# In dio mode, the host uses the "Dual I/O Fast Read" (BBH) command to read data. 
	# Each read command is sent from the host to the flash chip via normal SPI, but then 
	# the address is sent to the flash chip via both the MOSI & MISO pins with two bits per clock. 
	# After this, the host reads the data bits with two bits per clock in the same way as "Dual Output Fast Read".
# Flash speed 40 Mhz
ESPTOOLOPTS	= -ff 40m -fm dio -fs 1MB

# which modules (subdirectories) of the project to include in compiling
MODULES	= user easygpio stdout ping

# compiler flags using during compilation of source files
# warn on pointer arithmetic GNU
# warn if an undefined identifier is evaluated in an `#if' directive
# treat warnings as errors
# -Wl passes CSV to linker
# -EL Link little endian objects
CFLAGS = -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH -DLWIP_OPEN_SRC -DUSE_OPTIMIZE_PRINTF

# TODO : enable flavors
#ifeq ($(FLAVOR), debug)
#	CFLAGS += -g -O2
#endif

# linker flags used to generate the main object file
LDFLAGS	= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -L.

# linker script used for the above linkier step
#LD_SCRIPT	= eagle.app.v6.ld
LD_SCRIPT1	= -Trom0.ld
LD_SCRIPT2	= -Trom1.ld

# libraries used in this project, mainly provided by the SDK
LIBS = c gcc hal pp phy net80211 lwip_open_napt wpa wpa2 main
LIBS		:= $(addprefix -l,$(LIBS))

# various paths from the SDK used in this project
SDK_LIBDIR	= $(addprefix $(SDK)/,lib)
SDK_INCLUDE	= include include/json
SDK_INCLUDE	:= $(addprefix -I$(SDK)/,$(SDK_INCLUDE))

# we create two different files for uploading into the flash
# these are the names and options to generate them
FW_1_ADDR	= 0x02000
FW_2_ADDR	= 0x82000

# select compiler, assembler and linker
CC := $(XTENSA_TOOLS)/xtensa-lx106-elf-gcc
AR := $(XTENSA_TOOLS)/xtensa-lx106-elf-ar
LD := $(XTENSA_TOOLS)/xtensa-lx106-elf-ld


SRC_DIR	:= $(MODULES)
BUILD_DIR := $(addprefix $(BUILD)/,$(MODULES))

SRC	:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ	:= $(patsubst %.c,$(BUILD)/%.o,$(SRC))

APP_AR := $(addprefix $(BUILD)/,$(TARGET)_app.a)
TARGET_OUT := $(addprefix $(BUILD)/,$(TARGET).out)

#LD_SCRIPT	:= $(addprefix -T$(SDK)/$(SDK_LDDIR)/,$(LD_SCRIPT))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

FW_FILE_1	:= $(addprefix $(FIRMWARE)/,$(FW_1_ADDR).bin)
FW_FILE_2	:= $(addprefix $(FIRMWARE)/,$(FW_2_ADDR).bin)
RBOOT_FILE	:= $(addprefix $(FIRMWARE)/,0x00000.bin)

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCLUDE) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

#all: checkdirs $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)
all: checkdirs $(FW_FILE_1) $(FW_FILE_2) $(RBOOT_FILE) $(FIRMWARE)/sha1sums

#$(FIRMWARE)/%.bin: $(TARGET_OUT) | $(FIRMWARE)
#	$(vecho) "FW" $@
#	$(Q) $(ESPTOOL) elf2image --version=2 $(TARGET_OUT) -o $@

#../esp-open-lwip/liblwip_open.a:
#	cd ../esp-open-lwip ; make -f Makefile.ajk all


$(FW_FILE_1): $(APP_AR)
	$(LD) -L$(CURDIR)/../esp-open-lwip -L$(SDK_LIBDIR) $(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $(TARGET_OUT)
	$(ESPTOOL) elf2image --version=2 $(TARGET_OUT) -o $(FW_FILE_1)


$(FW_FILE_2): $(APP_AR)
	$(LD) -L$(CURDIR)/../esp-open-lwip -L$(SDK_LIBDIR) $(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $(TARGET_OUT)
	$(ESPTOOL) elf2image --version=2 $(TARGET_OUT) -o $(FW_FILE_2)

$(RBOOT_FILE): rboot.bin
	cp rboot.bin $(RBOOT_FILE)


$(FIRMWARE)/sha1sums: $(APP_AR) $(FW_FILE_1) $(FW_FILE_2) $(RBOOT_FILE)
	sha1sum $(FW_FILE_1) $(FW_FILE_2) $(RBOOT_FILE) > $(FIRMWARE)/sha1sums

$(APP_AR): $(OBJ)
	$(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FIRMWARE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FIRMWARE):
	$(Q) mkdir -p $@

flash: $(FIRMWARE)/sha1sums
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) write_flash $(ESPTOOLOPTS) 0x00000 $(RBOOT_FILE) $(FW_1_ADDR) $(FW_FILE_1)

flash1: $(FIRMWARE)/sha1sums
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) write_flash $(ESPTOOLOPTS) 0x00000 $(RBOOT_FILE) $(FW_2_ADDR) $(FW_FILE_2)

flashboth: $(FIRMWARE)/sha1sums
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) write_flash $(ESPTOOLOPTS) 0x00000 $(RBOOT_FILE) $(FW_1_ADDR) $(FW_FILE_1) $(FW_2_ADDR) $(FW_FILE_2)

clean:
	rm -rf $(FIRMWARE) $(BUILD)
	find . -name "*~" -print0 | xargs -0 rm -rf

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
