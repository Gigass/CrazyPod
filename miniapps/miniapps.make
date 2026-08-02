# CrazyPod AOT native miniapp payloads.  TypeScript/TSX is generated into C
# before Make runs; both app.arm and app.dylib are linked from that same C.

MINIAPP_ROOT := $(ROOTDIR)/miniapps
MINIAPP_BUILD := $(BUILDDIR)/miniapps
MINIAPP_SDK := $(MINIAPP_ROOT)/sdk/crazypod_miniapp_native.h
MINIAPP_LINK_LDS := $(MINIAPP_BUILD)/miniapp.link
MINIAPP_PLUGIN_LDS := $(APPSDIR)/plugins/plugin.lds
MINIAPP_CONFIG := $(FIRMDIR)/export/config/$(MODELNAME).h

MINIAPP_NATIVE_APPS := apps/native-reference apps/capability-lab \
	apps/game2048 themes/atelier-hifi themes/signal-one
MINIAPP_NATIVE_OBJS := $(foreach app,$(MINIAPP_NATIVE_APPS), \
	$(MINIAPP_BUILD)/$(app)/app.o)
MINIAPP_NATIVE_RUNTIME_OBJ := \
	$(MINIAPP_BUILD)/sdk/crazypod_miniapp_runtime.o

MINIAPP_FLAGS := $(CFLAGS) -I$(MINIAPP_ROOT)/sdk \
	-DCRAZYPOD_MINIAPP_PACKAGE -ffreestanding -fno-builtin \
	-ffunction-sections -fdata-sections

ifdef APP_TYPE
MINIAPP_FLAGS += $(SHARED_CFLAGS)
MINIAPP_PAYLOADS := $(foreach app,$(MINIAPP_NATIVE_APPS), \
	$(MINIAPP_BUILD)/$(app)/app.dylib)
else
MINIAPP_PAYLOADS := $(foreach app,$(MINIAPP_NATIVE_APPS), \
	$(MINIAPP_BUILD)/$(app)/app.arm)
endif

OTHER_SRC += $(foreach app,$(MINIAPP_NATIVE_APPS), \
	$(MINIAPP_ROOT)/$(app)/generated/app.c)
OTHER_SRC += $(MINIAPP_ROOT)/sdk/crazypod_miniapp_runtime.c

ROCKS += $(MINIAPP_PAYLOADS)
CLEANOBJS += $(MINIAPP_BUILD)

.PHONY: miniapps
miniapps: $(MINIAPP_PAYLOADS)

$(MINIAPP_BUILD)/%.o: $(MINIAPP_ROOT)/%.c $(MINIAPP_SDK)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		-I$(dir $<) $(MINIAPP_FLAGS) -c $< -o $@

$(MINIAPP_BUILD)/%/app.o: \
	$(MINIAPP_ROOT)/%/generated/app.c $(MINIAPP_SDK)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC miniapps/$*/generated/app.c)$(CC) \
		-I$(dir $<) $(MINIAPP_FLAGS) -c $< -o $@

ifndef APP_TYPE
$(MINIAPP_LINK_LDS): $(MINIAPP_PLUGIN_LDS) $(MINIAPP_CONFIG)
	$(call PRINTS,PP $(@F))
	$(SILENT)mkdir -p $(dir $@)
	$(call preprocess2file,$<,$@,-DLOADADDRESS=$(LOADADDRESS))

$(MINIAPP_BUILD)/%/app.arm: \
	$(MINIAPP_BUILD)/%/app.o $(MINIAPP_NATIVE_RUNTIME_OBJ) $(MINIAPP_LINK_LDS)
	$(call PRINTS,LD miniapps/$*/app.arm)$(CC) \
		$(MINIAPP_FLAGS) -o $(MINIAPP_BUILD)/$*/app.elf \
		$(filter %.o,$^) -lgcc -T$(MINIAPP_LINK_LDS) \
		-Wl,--gc-sections \
		-Wl,-Map,$(MINIAPP_BUILD)/$*/app.map \
		$(GLOBAL_LDOPTS)
	$(SILENT)$(OC) -O binary \
		$(MINIAPP_BUILD)/$*/app.elf $@
else
$(MINIAPP_BUILD)/%/app.dylib: \
	$(MINIAPP_BUILD)/%/app.o
	$(call PRINTS,LD miniapps/$*/app.dylib)$(CC) \
		$(MINIAPP_FLAGS) -o $@ $(filter %.o,$^) \
		-lgcc $(SHARED_LDFLAGS) \
		-Wl,$(LDMAP_OPT),$(MINIAPP_BUILD)/$*/app.map \
		$(GLOBAL_LDOPTS)
endif
