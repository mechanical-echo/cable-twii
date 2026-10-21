.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
DEVKITPRO	?=	/opt/devkitpro
export DEVKITPRO
export DEVKITPPC	:=	$(DEVKITPRO)/devkitPPC
export PATH	:=	$(DEVKITPRO)/tools/bin:$(DEVKITPPC)/bin:$(PATH)
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	boot
BUILD		:=	build
SOURCES		:=	source ../src ../vendor/tomlc99
DATA		:=
INCLUDES	:=	../include ../src ../vendor

DEFINES		:=
ifneq ($(strip $(AUTOTEST)),)
DEFINES		+=	-DAUTOTEST
endif

CFLAGS	= -g -O2 -std=gnu11 -Wall -Wextra $(MACHDEP) $(INCLUDE) $(DEFINES)
CXXFLAGS	=	$(CFLAGS)

LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS	:=	-lfat -lwiiuse -lbte -logc -lm

LIBDIRS	:=

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD	:=	$(CC)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

export LIBPATHS	:= -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

DEPENDS	:=	$(OFILES:.o=.d)

toml.o: CFLAGS += -Wno-char-subscripts

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

-include $(DEPENDS)

endif
