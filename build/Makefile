PROJECT_ROOT := $(abspath ..)

TARGET      ?= f29h85x_makefile_example
PROFILE     ?= debug
CONFIG      ?= RAM
BOARD       ?= LAUNCHXL
C29SDK_ROOT ?= $(HOME)/ti/f29h85x-sdk
CCS_ROOT    ?= $(HOME)/ti/ccs2030/ccs

# Project source roots (relative to project root). Recursive scan is enabled.
SRC_DIRS ?= src

# Additional sources outside project root required for this example.
EXTRA_SRCS ?= \
	$(C29SDK_ROOT)/examples/device_support/source/device.c \
	$(C29SDK_ROOT)/source/bitfields/bitfield_structs.c

XDSDFU       ?= $(CCS_ROOT)/ccs_base/common/uscif/xds110/xdsdfu
DSS_SH       ?= $(CCS_ROOT)/ccs_base/scripting/bin/dss.sh
BURN_DSS_JS  ?= $(PROJECT_ROOT)/build/scripts/burn_dss.js
CCXML        ?= $(C29SDK_ROOT)/examples/device_support/targetconfigs/TMS320F29H850TU9.ccxml
CORE         ?= C29xx_CPU1

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

.PHONY: all clean flash rom ram burn burn-check burn-win print-sources

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
	$(CG_TOOL_OBJCOPY) --remove-section=cert -O binary $< $@
ifeq ($(CONFIG),FLASH)
ifeq ($(DUMMY_CERT),1)
	chmod +x $(C29SDK_ROOT)/tools/misc/$(UPDATEDUMMYCERT)
	$(C29SDK_ROOT)/tools/misc/$(UPDATEDUMMYCERT) $(DUMMY_CERT_FLASH) $@
	$(CONCAT) $(DUMMY_CERT_FLASH) $@ > $(BINDIR)/$(TARGET)_cert.bin
	$(CG_TOOL_OBJCOPY) --update-section cert=$(DUMMY_CERT_FLASH) $(BINDIR)/$(TARGET).out $(BINDIR)/$(TARGET).out
else
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
	$(CG_TOOL_OBJCOPY) --update-section cert=C29-cert-pad.bin $(BINDIR)/$(TARGET).out $(BINDIR)/$(TARGET).out
endif
endif

flash:
	$(MAKE) CONFIG=FLASH all

rom: flash

ram:
	$(MAKE) CONFIG=RAM all

# Program FLASH image to target using CCS DSS scripting.
# Override as needed:
#   make burn CCXML=/path/to/your.ccxml CORE=C29xx_CPU1
#   make burn BURN_IMAGE=/path/to/custom.out
BURN_CONFIG ?= FLASH
BURN_IMAGE ?= $(PROJECT_ROOT)/build/out/$(BOARD)/$(BURN_CONFIG)/$(TARGET).out
BURN_CERT_IMAGE ?= $(PROJECT_ROOT)/build/out/$(BOARD)/$(BURN_CONFIG)/$(TARGET)_cert.bin
burn: burn-check
	$(MAKE) CONFIG=$(BURN_CONFIG) all
	@test -x "$(DSS_SH)" || (echo "ERROR: DSS launcher not found: $(DSS_SH)"; exit 1)
	@test -f "$(BURN_DSS_JS)" || (echo "ERROR: DSS burn script not found: $(BURN_DSS_JS)"; exit 1)
	@test -f "$(CCXML)" || (echo "ERROR: CCXML not found: $(CCXML)"; exit 1)
	@test -f "$(BURN_IMAGE)" || (echo "ERROR: Image not found: $(BURN_IMAGE)"; exit 1)
	@echo "Burning $(BURN_IMAGE) via $(CCXML) (core: $(CORE))"
	"$(DSS_SH)" "$(BURN_DSS_JS)" "$(CCXML)" "$(CORE)" "$(BURN_IMAGE)"

burn-check:
	@test -x "$(XDSDFU)" || (echo "ERROR: XDS110 utility not found: $(XDSDFU)"; exit 1)
	@$(XDSDFU) -e 2>/dev/null | grep -q "Found [1-9][0-9]* devices" || \
		(echo "ERROR: No XDS110 probe visible in this environment."; \
		 echo "If using WSL2, attach USB probe with usbipd on Windows first."; \
		 exit 2)

# Burn using Windows-side UniFlash CLI (useful when probe is attached to Windows, not WSL).
# Requires Windows CCS installed. Paths below are defaults and can be overridden.
WIN_UNIFLASH_EXE ?= C:\\ti\\ccs2041\\ccs\\ccs_base\\scripting\\examples\\uniflash\\cmdLine\\uniflash.bat
WIN_CCXML        ?= $(shell wslpath -m "$(CCXML)")
WIN_BURN_IMAGE   ?= $(shell wslpath -m "$(BURN_CERT_IMAGE)")
burn-win:
	$(MAKE) CONFIG=$(BURN_CONFIG) all
	@command -v powershell.exe >/dev/null || (echo "ERROR: powershell.exe not found"; exit 1)
	@echo "Burning (Windows) $(WIN_BURN_IMAGE) via $(WIN_CCXML) (core: $(CORE))"
	@powershell.exe -NoProfile -Command "\$$ErrorActionPreference='Stop'; if (!(Test-Path '$(WIN_UNIFLASH_EXE)')) { throw 'UniFlash not found: $(WIN_UNIFLASH_EXE)' }; \$$stageDir = Join-Path \$$env:TEMP 'f29h85x_makefile_example-uniflash'; New-Item -ItemType Directory -Force -Path \$$stageDir | Out-Null; \$$ccxmlLocal = Join-Path \$$stageDir ([System.IO.Path]::GetFileName('$(WIN_CCXML)')); \$$imageLocal = Join-Path \$$stageDir ([System.IO.Path]::GetFileName('$(WIN_BURN_IMAGE)')); Copy-Item -LiteralPath '$(WIN_CCXML)' -Destination \$$ccxmlLocal -Force; Copy-Item -LiteralPath '$(WIN_BURN_IMAGE)' -Destination \$$imageLocal -Force; \$$ccxmlArg = \$$ccxmlLocal.Replace('\','/'); \$$imageArg = \$$imageLocal.Replace('\','/'); Set-Location \$$stageDir; & '$(WIN_UNIFLASH_EXE)' -ccxml \$$ccxmlArg -core '$(CORE)' -programBin \$$imageArg 0x10000000 -targetOp reset; if (\$$LASTEXITCODE -ne 0) { exit \$$LASTEXITCODE }"

clean:
	-$(RMDIR) $(PROJECT_ROOT)/build/obj $(PROJECT_ROOT)/build/out
	-$(RM) $(PROJECT_ROOT)/C29-cert-pad.bin $(PROJECT_ROOT)/build/C29-cert-pad.bin

-include $(DEPS)
