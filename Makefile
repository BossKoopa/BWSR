
UNAME       := $(shell uname -s)
BUILD_DIR   := build

include Make/darwin.mk
include Make/linux.mk
include Make/android.mk

DEPLOYMENTS :=  \
    debug       \
    release

define platform_rules
# Source files
SRCS_$(1) := $(foreach dir,                         \
               $(4),                                \
               $(wildcard $(dir)/*.c))

OBJS_$(1) := $$(patsubst %.c,                       \
                $$(BUILD_DIR)/$(1)/$(3)/$(2)/%.o,   \
                $$(SRCS_$(1)))

# Compiled objects
$$(BUILD_DIR)/$(1)/$(3)/$(2)/%.o: %.c | \
$$(addprefix $$(BUILD_DIR)/$(1)/$(3)/$(2)/,$$($(4)))
	@echo "Compiling for $(1) on $(2) in $(3): $$<..."
	@mkdir -p $$(dir $$@)
	@$$(GCC_$(1)) $$(GCCFLAGS_$(1)_$(3)) -arch $(2) -I. -c $$< -o $$@

# libBwsrHook
$$(BUILD_DIR)/$(1)/$(3)/$(2)/libBwsrHook: \
$$(OBJS_$(1)) | \
$$(addprefix $$(BUILD_DIR)/$(1)/$(3)/$(2)/,$$($(4)))
	@echo "Building Archive for $(1) on $(2) in $(3)..."
	@$$(AR_$(1)) rcs $(BUILD_DIR)/$(1)/$(3)/$(2)/libBwsrHook.a $$^

# Example
$$(BUILD_DIR)/$(1)/$(3)/$(2)/Example: Example.c \
$$(OBJS_$(1)) | \
$$(addprefix $$(BUILD_DIR)/$(1)/$(3)/$(2)/,$$($(4)))
	@echo "Compiling Example for $(1) on $(2) in $(3)..."
	@$$(GCC_$(1)) $$(GCCFLAGS_$(1)_$(3)) -arch $(2) $$(EXAMPLE_LDFLAGS_$(1)) -I. -o $$@ $$^
ifeq ($(1),ios)
	@codesign -s - --entitlements Entitlements.plist $$@
endif

$$(addprefix $$(BUILD_DIR)/$(1)/$(3)/$(2)/,$$($(4))):
	@mkdir -p $$@
endef

# Evaluate the platform_rules for each platform
ifeq ($(UNAME),Darwin)
$(foreach platform,$(DARWIN_PLATFORMS), \
	$(foreach arch,$(DARWIN_ARCHS), \
		$(foreach deployment,$(DEPLOYMENTS), \
			$(eval $(call platform_rules,$(platform),$(arch),$(deployment),$(DARWIN_DIRS))) \
		) \
	) \
)
endif

ifeq ($(UNAME),Linux)
$(foreach platform,$(LINUX_PLATFORMS), \
	$(foreach arch,$(LINUX_ARCHS), \
		$(foreach deployment,$(DEPLOYMENTS), \
			$(eval $(call platform_rules,$(platform),$(arch),$(deployment),$(LINUX_DIRS))) \
		) \
	) \
)
endif

ifdef ANDROID_NDK
$(foreach platform,$(ANDROID_PLATFORMS), \
	$(foreach arch,$(ANDROID_ARCHS), \
		$(foreach deployment,$(DEPLOYMENTS), \
			$(eval $(call platform_rules,$(platform),$(arch),$(deployment),$(ANDROID_DIRS))) \
		) \
	) \
)
endif

$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf build

collect_header:
	@cp Hook/InlineHook.h $(BUILD_DIR)

hooklib_debug:                  \
	mac_debug                   \
	ios_debug                   \
	android_debug               \
	linux_debug

hooklib_release:                \
	mac_release                 \
	ios_release                 \
	android_release             \
	linux_release

examples_debug:                 \
	mac_example_debug           \
	ios_example_debug           \
	android_example_debug       \
	linux_example_debug

examples_release:               \
	mac_example_release         \
	ios_example_release         \
	android_example_release     \
	linux_example_release

examples:                       \
	examples_release            \
	examples_debug

hooklibs:                       \
	hooklib_release             \
	hooklib_debug

debug:                          \
	hooklib_debug               \
	examples_debug

release:                        \
	hooklib_release             \
	examples_release

all: hooklibs examples collect_header

.DEFAULT_GOAL := all

.PHONY: all clean release debug
