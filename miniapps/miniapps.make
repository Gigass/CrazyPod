# CrazyPod native mini-app payloads. This is intentionally separate from
# Rockbox's legacy plugin catalog, which remains disabled for iPod 6G.

MINIAPP_ROOT := $(ROOTDIR)/miniapps
MINIAPP_BUILD := $(BUILDDIR)/miniapps
MINIAPP_SDK := $(MINIAPP_ROOT)/sdk/crazypod_miniapp.h
MINIAPP_NAMES := calculator pomodoro
MINIAPP_LINK_LDS := $(MINIAPP_BUILD)/miniapp.link
MINIAPP_PLUGIN_LDS := $(APPSDIR)/plugins/plugin.lds
MINIAPP_CONFIG := $(FIRMDIR)/export/config/$(MODELNAME).h

MINIAPP_CALCULATOR_OBJ := \
	$(MINIAPP_BUILD)/calculator/app.o \
	$(MINIAPP_BUILD)/calculator/engine.o
MINIAPP_POMODORO_OBJ := \
	$(MINIAPP_BUILD)/pomodoro/app.o \
	$(MINIAPP_BUILD)/pomodoro/engine.o
MINIAPP_NATIVE_RUNTIME_OBJ := \
	$(MINIAPP_BUILD)/sdk/crazypod_miniapp_runtime.o

MINIAPP_FLAGS := $(CFLAGS) -I$(MINIAPP_ROOT)/sdk \
	-DCRAZYPOD_MINIAPP_PACKAGE -ffreestanding -fno-builtin \
	-ffunction-sections -fdata-sections

ifdef APP_TYPE
MINIAPP_FLAGS += $(SHARED_CFLAGS)
MINIAPP_PAYLOADS := \
	$(MINIAPP_BUILD)/calculator/app.dylib \
	$(MINIAPP_BUILD)/pomodoro/app.dylib
else
MINIAPP_CALCULATOR_OBJ += $(MINIAPP_NATIVE_RUNTIME_OBJ)
MINIAPP_POMODORO_OBJ += $(MINIAPP_NATIVE_RUNTIME_OBJ)
MINIAPP_PAYLOADS := \
	$(MINIAPP_BUILD)/calculator/app.arm \
	$(MINIAPP_BUILD)/pomodoro/app.arm
endif

OTHER_SRC += \
	$(MINIAPP_ROOT)/calculator/app.c \
	$(MINIAPP_ROOT)/calculator/engine.c \
	$(MINIAPP_ROOT)/pomodoro/app.c \
	$(MINIAPP_ROOT)/pomodoro/engine.c \
	$(MINIAPP_ROOT)/sdk/crazypod_miniapp_runtime.c

ROCKS += $(MINIAPP_PAYLOADS)
CLEANOBJS += $(MINIAPP_BUILD)

.PHONY: miniapps
miniapps: $(MINIAPP_PAYLOADS)

$(MINIAPP_BUILD)/%.o: $(MINIAPP_ROOT)/%.c $(MINIAPP_SDK)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		-I$(dir $<) $(MINIAPP_FLAGS) -c $< -o $@

ifndef APP_TYPE
$(MINIAPP_LINK_LDS): $(MINIAPP_PLUGIN_LDS) $(MINIAPP_CONFIG)
	$(call PRINTS,PP $(@F))
	$(SILENT)mkdir -p $(dir $@)
	$(call preprocess2file,$<,$@,-DLOADADDRESS=$(LOADADDRESS))

define build_native_miniapp
$(MINIAPP_BUILD)/$(1)/app.arm: $$(MINIAPP_$(2)_OBJ) $$(MINIAPP_LINK_LDS)
	$$(call PRINTS,LD miniapps/$(1)/app.arm)$$(CC) $$(MINIAPP_FLAGS) \
		-o $$(MINIAPP_BUILD)/$(1)/app.elf \
		$$(filter %.o,$$^) -lgcc -T$$(MINIAPP_LINK_LDS) \
		-Wl,--gc-sections -Wl,-Map,$$(MINIAPP_BUILD)/$(1)/app.map \
		$$(GLOBAL_LDOPTS)
	$$(SILENT)$$(OC) -O binary \
		$$(MINIAPP_BUILD)/$(1)/app.elf $$@
endef

$(eval $(call build_native_miniapp,calculator,CALCULATOR))
$(eval $(call build_native_miniapp,pomodoro,POMODORO))
else
define build_sim_miniapp
$(MINIAPP_BUILD)/$(1)/app.dylib: $$(MINIAPP_$(2)_OBJ)
	$$(call PRINTS,LD miniapps/$(1)/app.dylib)$$(CC) \
		$$(MINIAPP_FLAGS) -o $$@ $$(filter %.o,$$^) \
		-lgcc $$(SHARED_LDFLAGS) \
		-Wl,$$(LDMAP_OPT),$$(MINIAPP_BUILD)/$(1)/app.map \
		$$(GLOBAL_LDOPTS)
endef

$(eval $(call build_sim_miniapp,calculator,CALCULATOR))
$(eval $(call build_sim_miniapp,pomodoro,POMODORO))
endif
