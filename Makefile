
# Directory structure
BUILD_DIR := build
MEMORY_DIR := Memory
HOOK_DIR := Hook
RESOLVE_DIR := SymbolResolve/Darwin
ARCHIVE_NAME := libBwsrHook.a

# iOS Compiler and flags
IOS_GCC_BIN := $(shell xcrun --sdk iphoneos --find clang)
IOS_SDK := $(shell xcrun --sdk iphoneos --show-sdk-path)
IOS_AR := $(shell xcrun --sdk iphoneos --find ar)
IOS_ARCH_FLAGS := -arch arm64

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
	PLATFORM := darwin
endif
ifeq ($(UNAME),Linux)
	PLATFORM := linux
endif

ANDROID_TOOLCHAIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/clang
ANDROID_SYSROOT := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/sysroot
ANDROID_AR := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/llvm-ar

IOS_LDFLAGS := \
    -F$(IOS_SDK)/System/Library/Frameworks/ \
    -framework CoreFoundation \
    -framework AudioToolbox \
    -mios-version-min=14.0 \
    -I.

IOS_GCCFLAGS := \
    -mios-version-min=14.0 \
    -Os \
    -Wall -Wextra -Werror \
    -Wimplicit \
    -isysroot $(IOS_SDK) \
    $(IOS_ARCH_FLAGS)

# Android Compiler and flags
ANDROID_ARCH_FLAGS := -target aarch64-none-linux-android30
ANDROID_LDFLAGS := \
    -pie \
    -llog \
	-I.\
    $(ANDROID_ARCH_FLAGS)

ANDROID_GCCFLAGS := \
    $(ANDROID_ARCH_FLAGS) \
    -Os \
    -Wall -Wextra -Werror \
    --sysroot=$(ANDROID_SYSROOT)

IOS_GCC_ARM := $(IOS_GCC_BIN) $(IOS_GCCFLAGS)
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

ISDEBUG := 0

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

$(BUILD_DIR)/ios/Example: Example.c $(OBJS_RESOLVE) $(OBJS_HOOK) $(OBJS_MEMORY) | $(BUILD_DIR)
	@echo "Compiling Example for iOS..."
	@$(IOS_GCC_ARM) $(IOS_LDFLAGS) -o $@ $^
	@codesign -s - --entitlements Entitlements.plist $@

$(BUILD_DIR)/ios/libBwsrHook: $(OBJS_RESOLVE) $(OBJS_HOOK) $(OBJS_MEMORY) | $(BUILD_DIR)
	@echo "Building Archive for iOS..."
	@$(IOS_AR) rcs $(BUILD_DIR)/ios/$(ARCHIVE_NAME) $^


# Build rules for Android
$(BUILD_DIR)/android/$(MEMORY_DIR)/%.o: $(MEMORY_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for Android: $<..."
	@mkdir -p $(BUILD_DIR)/android/$(MEMORY_DIR)
	@$(ANDROID_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/android/$(HOOK_DIR)/%.o: $(HOOK_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling for Android: $<..."
	@mkdir -p $(BUILD_DIR)/android/$(HOOK_DIR)
	@$(ANDROID_GCC_ARM) -I. -c $< -o $@

$(BUILD_DIR)/android/Example: Example.c $(OBJS_HOOK_ANDROID) $(OBJS_MEMORY_ANDROID) | $(BUILD_DIR)
	@echo "Compiling Example for Android..."
	@$(ANDROID_TOOLCHAIN) $(ANDROID_LDFLAGS) -o $@ $^

$(BUILD_DIR)/android/libBwsrHook: $(OBJS_HOOK_ANDROID) $(OBJS_MEMORY_ANDROID) | $(BUILD_DIR)
	@echo "Building Archive for Android..."
	@$(ANDROID_AR) rcs $(BUILD_DIR)/android/$(ARCHIVE_NAME) $^

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Phony targets
all: clean Archives

clean:
	@rm -rf $(BUILD_DIR)

iOSExample: $(BUILD_DIR)/ios/Example

AndroidExample: check-android-env $(BUILD_DIR)/android/Example

SetDebugMode:
	$(eval ANDROID_LDFLAGS := $(ANDROID_LDFLAGS) -DDEBUG_MODE)
	$(eval ANDROID_GCC_ARM := $(ANDROID_GCC_ARM) -DDEBUG_MODE)
	$(eval IOS_GCC_ARM := $(IOS_GCC_ARM) -DDEBUG_MODE)
	$(eval ARCHIVE_NAME := libBwsrHook_DEBUG.a)

Debug: SetDebugMode all

iOS: $(BUILD_DIR)/ios/libBwsrHook

Android: $(BUILD_DIR)/android/libBwsrHook

Archives: clean iOS Android

Examples: clean iOSExample AndroidExample

ExamplesDebug: SetDebugMode Examples

# Ensure ANDROID_NDK is defined
check-android-env:
ifndef ANDROID_NDK
	$(error ANDROID_NDK is undefined)
endif

.DEFAULT_GOAL := all

.PHONY: all clean Debug iOS Android Examples AndroidExample iOSExample
