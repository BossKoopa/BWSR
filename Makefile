
# Directory structure
BUILD_DIR 	 := build
MEMORY_DIR   := Memory
HOOK_DIR 	 := Hook
RESOLVE_DIR  := SymbolResolve/Darwin
ARCHIVE_NAME := libBwsrHook.a

# Source files
SRCS_MEMORY  := $(wildcard $(MEMORY_DIR)/*.c)
SRCS_HOOK 	 := $(wildcard $(HOOK_DIR)/*.c)
SRCS_RESOLVE := $(wildcard $(RESOLVE_DIR)/*.c)

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
PLATFORM := darwin

# iOS Compiler and flags
IOS_GCC_BIN := $(shell xcrun --sdk iphoneos --find clang)
IOS_SDK     := $(shell xcrun --sdk iphoneos --show-sdk-path)
IOS_AR      := $(shell xcrun --sdk iphoneos --find ar)
IOS_ARCH_FLAGS := -arch arm64

# Flags for Example file ONLY
IOS_LDFLAGS := 									\
    -F$(IOS_SDK)/System/Library/Frameworks/ 	\
    -framework CoreFoundation 					\
    -framework AudioToolbox 					\
    -mios-version-min=14.0 						\
    -I.

IOS_GCCFLAGS := 				\
    -mios-version-min=14.0 		\
    -Os 						\
    -Wall 						\
	-Wextra 					\
	-Werror 					\
    -Wimplicit 					\
    -isysroot $(IOS_SDK) 		\
    $(IOS_ARCH_FLAGS)

IOS_GCC_ARM := $(IOS_GCC_BIN) $(IOS_GCCFLAGS)

# Objects
OBJS_MEMORY  := $(patsubst $(MEMORY_DIR)/%.c,  $(BUILD_DIR)/ios/$(MEMORY_DIR)/%.o,  $(SRCS_MEMORY))
OBJS_HOOK 	 := $(patsubst $(HOOK_DIR)/%.c,    $(BUILD_DIR)/ios/$(HOOK_DIR)/%.o,    $(SRCS_HOOK))
OBJS_RESOLVE := $(patsubst $(RESOLVE_DIR)/%.c, $(BUILD_DIR)/ios/$(RESOLVE_DIR)/%.o, $(SRCS_RESOLVE))

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

else
PLATFORM := linux
endif

ANDROID_TOOLCHAIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/clang
ANDROID_SYSROOT   := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/sysroot
ANDROID_AR 		  := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/llvm-ar

# Android Compiler and flags
ANDROID_ARCH_FLAGS := -target aarch64-none-linux-android30

# Flags for Example file ONLY
ANDROID_LDFLAGS := 					\
    -pie 							\
    -llog 							\
	-I.								\
    $(ANDROID_ARCH_FLAGS)

ANDROID_GCCFLAGS := 				\
    $(ANDROID_ARCH_FLAGS) 			\
    -Os 							\
    -Wall 							\
	-Wextra 						\
	-Werror 						\
    --sysroot=$(ANDROID_SYSROOT)

ANDROID_GCC_ARM := $(ANDROID_TOOLCHAIN) $(ANDROID_GCCFLAGS)

OBJS_MEMORY_ANDROID := $(patsubst $(MEMORY_DIR)/%.c, $(BUILD_DIR)/android/$(MEMORY_DIR)/%.o, $(SRCS_MEMORY))
OBJS_HOOK_ANDROID   := $(patsubst $(HOOK_DIR)/%.c,   $(BUILD_DIR)/android/$(HOOK_DIR)/%.o,   $(SRCS_HOOK))

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

Debug: SetDebugMode all

clean:
	@rm -rf $(BUILD_DIR)

ExamplesDebug: SetDebugMode Examples

ifeq ($(UNAME),Darwin)
iOSExample: $(BUILD_DIR)/ios/Example

iOS: $(BUILD_DIR)/ios/libBwsrHook
endif

Android: check-android-env $(BUILD_DIR)/android/libBwsrHook

AndroidExample: check-android-env $(BUILD_DIR)/android/Example

SetDebugMode:
	$(eval ANDROID_LDFLAGS := $(ANDROID_LDFLAGS) -DDEBUG_MODE)
	$(eval ANDROID_GCC_ARM := $(ANDROID_GCC_ARM) -DDEBUG_MODE)
	$(eval ARCHIVE_NAME := libBwsrHook_DEBUG.a)
ifeq ($(UNAME),Darwin)
	$(eval IOS_GCC_ARM := $(IOS_GCC_ARM) -DDEBUG_MODE)
endif

ifeq ($(UNAME),Darwin)
Archives: clean iOS Android

Examples: clean iOSExample AndroidExample
else
Archives: clean Android

Examples: clean AndroidExample
endif

# Ensure ANDROID_NDK is defined
check-android-env:
ifndef ANDROID_NDK
	$(error ANDROID_NDK is undefined)
endif

.DEFAULT_GOAL := all

.PHONY: all clean Debug iOS Android Examples AndroidExample iOSExample
