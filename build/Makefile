PROJECT_ROOT := $(abspath ..)

TARGET      ?= f29h85x_makefile_example
PROFILE     ?= debug
CONFIG      ?= RAM
BOARD       ?= LAUNCHXL
C29SDK_ROOT ?= $(HOME)/ti/f29h85x-sdk

# Project source roots (relative to project root). Recursive scan is enabled.
SRC_DIRS ?= src

# Additional sources outside project root required for this example.
EXTRA_SRCS ?= \
	$(C29SDK_ROOT)/examples/device_support/source/device.c \
	$(C29SDK_ROOT)/source/bitfields/bitfield_structs.c

include $(C29SDK_ROOT)/imports.mak

OBJDIR := $(PROJECT_ROOT)/build/obj/$(BOARD)/$(CONFIG)
BINDIR := $(PROJECT_ROOT)/build/out/$(BOARD)/$(CONFIG)

CMD_SRCS := $(CMD_SRCS_$(CONFIG))
CFLAGS   := $(CFLAGS_common) $(CFLAGS_$(PROFILE))
INCLUDES := $(INCLUDES_common) -I $(PROJECT_ROOT)
LFLAGS   := $(LFLAGS_common)

comma := ,

rwildcard = \
	$(wildcard $(1)/$(2)) \
	$(foreach d,$(wildcard $(1)/*),$(call rwildcard,$(d),$(2)))

PROJECT_SRCS := $(sort $(foreach d,$(SRC_DIRS),$(call rwildcard,$(PROJECT_ROOT)/$(d),*.c)))
REL_PROJECT_SRCS := $(patsubst $(PROJECT_ROOT)/%,%,$(PROJECT_SRCS))
PROJECT_OBJS := $(addprefix $(OBJDIR)/,$(REL_PROJECT_SRCS:.c=.o))
EXTRA_OBJS := $(addprefix $(OBJDIR)/external/,$(notdir $(EXTRA_SRCS:.c=.o)))
OBJS := $(PROJECT_OBJS) $(EXTRA_OBJS)
DEPS := $(OBJS:.o=.d)

vpath %.lib $(LIBS_PATH)
vpath %.a $(LIBS_PATH)

.PHONY: all clean flash rom ram print-sources

all: $(BINDIR)/$(TARGET).out $(BINDIR)/$(TARGET).bin

print-sources:
	@echo "SRC_DIRS=$(SRC_DIRS)"
	@printf '%s\n' $(PROJECT_SRCS)
	@printf '%s\n' $(EXTRA_SRCS)

$(OBJDIR)/%.o: $(PROJECT_ROOT)/%.c
	@$(MKDIR) $(dir $@)
	@echo Compiling: $<
	$(COMPILER) -c $(CFLAGS) $(INCLUDES) -MMD -MP -o $@ $<

define EXTRA_RULE
$(OBJDIR)/external/$(notdir $(1:.c=.o)): $(1)
	@$(MKDIR) $$(dir $$@)
	@echo Compiling: $$<
	$(COMPILER) -c $(CFLAGS) $(INCLUDES) -MMD -MP -o $$@ $$<
endef

$(foreach s,$(EXTRA_SRCS),$(eval $(call EXTRA_RULE,$(s))))

$(BINDIR)/$(TARGET).out: $(OBJS) $(CMD_SRCS)
	@$(MKDIR) $(dir $@)
	@echo Linking: $@
	$(LINKER) $(INCLUDES) $(LFLAGS) $(addprefix -Wl$(comma)-i, $(LIBS_PATH)) \
		-Wl,-m=$(BINDIR)/$(TARGET).map \
		-o $@ \
		$(OBJS) \
		$(ASM_SRCS) \
		$(addprefix -l,$(LIBS)) \
		$(CMD_SRCS)

$(BINDIR)/$(TARGET).bin: $(BINDIR)/$(TARGET).out
	$(CG_TOOL_OBJCOPY) -O binary $< $@
ifeq ($(CONFIG),FLASH)
	$(PYTHON) $(C29SDK_ROOT)/tools/boot/signing/mcu_rom_image_gen.py \
		--image-bin $@ \
		--core C29 \
		--swrv 1 \
		--loadaddr 0x10001000 \
		--sign-key $(C29SDK_ROOT)/tools/boot/signing/mcu_gpkey.pem \
		--out-image $(BINDIR)/$(TARGET)_cert.bin \
		--device f29h85x \
		--boot FLASH \
		--img_integ no
endif

flash:
	$(MAKE) CONFIG=FLASH all

rom: flash

ram:
	$(MAKE) CONFIG=RAM all

clean:
	-$(RMDIR) $(PROJECT_ROOT)/build/obj $(PROJECT_ROOT)/build/out
	-$(RM) $(PROJECT_ROOT)/C29-cert-pad.bin $(PROJECT_ROOT)/build/C29-cert-pad.bin

-include $(DEPS)
