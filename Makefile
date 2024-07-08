
# Directory structure
BUILD_DIR := build
MEMORY_DIR := Memory
HOOK_DIR := Hook
RESOLVE_DIR := SymbolResolve/Darwin

# iOS Compiler and flags
IOS_GCC_BIN := $(shell xcrun --sdk iphoneos --find clang)
IOS_SDK := $(shell xcrun --sdk iphoneos --show-sdk-path)
IOS_ARCH_FLAGS := -arch arm64

IOS_LDFLAGS := \
    -F$(IOS_SDK)/System/Library/Frameworks/ \
    -framework CoreFoundation \
    -framework AudioToolbox \
    -mios-version-min=14.0 \
    -I. \
    -I$(MEMORY_DIR) \
    -I$(HOOK_DIR)

IOS_GCCFLAGS := \
    -mios-version-min=14.0 \
    -Os \
    -Wall -Wextra -Werror \
    -Wimplicit \
    -DDEBUG_MODE \
    -isysroot $(IOS_SDK) \
    $(IOS_ARCH_FLAGS)

IOS_GCC_ARM := $(IOS_GCC_BIN) $(IOS_GCCFLAGS)

ANDROID_TOOLCHAIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang
ANDROID_SYSROOT := $(ANDROID_NDK)/toolchains/llvm/prebuilt/darwin-x86_64/sysroot

# Android Compiler and flags
ANDROID_ARCH_FLAGS := -target aarch64-none-linux-android30
ANDROID_LDFLAGS := \
    -pie \
    -llog \
	-I$(MEMORY_DIR)\
	-I$(HOOK_DIR)\
	-I.\
    $(ANDROID_ARCH_FLAGS)

ANDROID_GCCFLAGS := \
    $(ANDROID_ARCH_FLAGS) \
    -Os \
    -Wall -Wextra -Werror \
    -DDEBUG_MODE \
    --sysroot=$(ANDROID_SYSROOT)

ANDROID_GCC_ARM := $(ANDROID_TOOLCHAIN) $(ANDROID_GCCFLAGS)

# Source files and objects
SRCS_MEMORY := $(wildcard $(MEMORY_DIR)/*.c)
OBJS_MEMORY := $(patsubst $(MEMORY_DIR)/%.c,$(BUILD_DIR)/ios/$(MEMORY_DIR)/%.o,$(SRCS_MEMORY))
OBJS_MEMORY_ANDROID := $(patsubst $(MEMORY_DIR)/%.c,$(BUILD_DIR)/android/$(MEMORY_DIR)/%.o,$(SRCS_MEMORY))

SRCS_HOOK := $(wildcard $(HOOK_DIR)/*.c)
OBJS_HOOK := $(patsubst $(HOOK_DIR)/%.c,$(BUILD_DIR)/ios/$(HOOK_DIR)/%.o,$(SRCS_HOOK))
OBJS_HOOK_ANDROID := $(patsubst $(HOOK_DIR)/%.c,$(BUILD_DIR)/android/$(HOOK_DIR)/%.o,$(SRCS_HOOK))

SRCS_RESOLVE := $(wildcard $(RESOLVE_DIR)/*.c)
OBJS_RESOLVE := $(patsubst $(RESOLVE_DIR)/%.c,$(BUILD_DIR)/ios/$(RESOLVE_DIR)/%.o,$(SRCS_RESOLVE))



# Build rules for iOS
$(BUILD_DIR)/ios/$(MEMORY_DIR)/%.o: $(MEMORY_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for iOS: $<..."
	@mkdir -p $(BUILD_DIR)/ios/$(MEMORY_DIR)
	@$(IOS_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/ios/$(RESOLVE_DIR)/%.o: $(RESOLVE_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for iOS: $<..."
	@mkdir -p $(BUILD_DIR)/ios/$(RESOLVE_DIR)
	@$(IOS_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/ios/$(HOOK_DIR)/%.o: $(HOOK_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for iOS: $<..."
	@mkdir -p $(BUILD_DIR)/ios/$(HOOK_DIR)
	@$(IOS_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/Example_ios: Example.c $(OBJS_RESOLVE) $(OBJS_HOOK) $(OBJS_MEMORY) | $(BUILD_DIR)
	@echo "Compiling Example for iOS..."
	@$(IOS_GCC_ARM) $(IOS_LDFLAGS) -o $@ $^
	@codesign -s - --entitlements Entitlements.plist $@



# Build rules for Android
$(BUILD_DIR)/android/$(MEMORY_DIR)/%.o: $(MEMORY_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for Android: $<..."
	@mkdir -p $(BUILD_DIR)/android/$(MEMORY_DIR)
	@$(ANDROID_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/android/$(HOOK_DIR)/%.o: $(HOOK_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for Android: $<..."
	@mkdir -p $(BUILD_DIR)/android/$(HOOK_DIR)
	@$(ANDROID_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/Example_android: Example.c $(OBJS_HOOK_ANDROID) $(OBJS_MEMORY_ANDROID) | $(BUILD_DIR)
	@echo "Compiling Example for Android..."
	@$(ANDROID_TOOLCHAIN) $(ANDROID_LDFLAGS) -o $@ $^

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Phony targets
all: Example_ios Example_android

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f Example_ios Example_android

Example_ios: $(BUILD_DIR)/Example_ios

Example_android: $(BUILD_DIR)/Example_android

.DEFAULT_GOAL := all

.PHONY: all clean Example_ios Example_android
