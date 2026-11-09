#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Project configuration
#---------------------------------------------------------------------------------
TARGET        := fangtune
BUILD         := build
SOURCES       := source
INCLUDES      := include
GRAPHICS      := assets
GFXBUILD      := $(BUILD)

APP_TITLE       := FangTune
APP_DESCRIPTION := MP3 player for Nintendo 3DS
APP_AUTHOR      := FangTune

APP_ICON        := $(TOPDIR)/assets/icon.png

#---------------------------------------------------------------------------------
# Options for code generation
#---------------------------------------------------------------------------------
ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS   := -g -Wall -O2 -mword-relocations \
            -ffunction-sections -fdata-sections \
            $(ARCH)

CFLAGS   += $(INCLUDE) -D__3DS__

CXXFLAGS := $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions

LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS     := -lcitro2d -lcitro3d -lctru -lm

LIBDIRS  := $(CTRULIB)

#---------------------------------------------------------------------------------
# Lists of directories that may contain sources
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES        := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES        := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# Use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE    := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                     $(foreach dir,$(DATA),-I$(CURDIR)/$(dir)) \
                     -I$(CURDIR)/$(BUILD)

export LIBPATHS   := -L$(CURDIR) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: all clean cia

#---------------------------------------------------------------------------------
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

cia: all
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(TARGET).cia

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(TARGET).cia $(TARGET).map

else
#---------------------------------------------------------------------------------
# Makefile inside build directory
#---------------------------------------------------------------------------------
DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)

#---------------------------------------------------------------------------------
# Installable CIA (requires makerom and bannertool on PATH)
#---------------------------------------------------------------------------------
$(OUTPUT).cia: $(OUTPUT).elf $(OUTPUT).smdh $(TOPDIR)/$(TARGET).rsf $(TOPDIR)/assets/banner-cia.png
	@echo banner ...
	@bannertool makebanner -i $(TOPDIR)/assets/banner-cia.png -o $(CURDIR)/$(TARGET).bnr
	@echo building ... $(notdir $@)
	@makerom -f cia -o $@ -target t -exefslogo -elf $(OUTPUT).elf \
		-icon $(OUTPUT).smdh -rsf $(TOPDIR)/$(TARGET).rsf -banner $(CURDIR)/$(TARGET).bnr

$(OFILES_SRC): $(HFILES_BIN)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
