
ifeq ($(UNAME),Darwin)

GCC_BIN_ios  := $(shell xcrun --sdk iphoneos --find clang)
SDK_ios      := $(shell xcrun --sdk iphoneos --show-sdk-path)
AR_ios       := $(shell xcrun --sdk iphoneos --find ar)

GCC_BIN_mac  := $(shell xcrun --sdk macosx --find clang)
SDK_mac      := $(shell xcrun --sdk macosx --show-sdk-path)
AR_mac       := $(shell xcrun --sdk macosx --find ar)

DARWIN_ARCHS := arm64 arm64e

DARWIN_PLATFORMS := ios mac

DARWIN_DIRS :=                  \
    Memory                      \
    Hook                        \
    SymbolResolve/Darwin

DARWIN_GCCFLAGS :=              \
    -Os                         \
    -Wall                       \
    -Wextra                     \
    -Werror                     \
    -Wimplicit

GCCFLAGS_mac_debug :=           \
    $(DARWIN_GCCFLAGS)          \
    -DDEBUG_MODE

GCCFLAGS_ios_debug :=           \
    $(DARWIN_GCCFLAGS)          \
    -DDEBUG_MODE

EXAMPLE_LDFLAGS_ios :=                          \
    -F$(SDK_ios)/System/Library/Frameworks/     \
    -framework CoreFoundation                   \
    -framework AudioToolbox                     \
    -mios-version-min=14.0

EXAMPLE_LDFLAGS_mac :=                          \
    -F$(SDK_mac)/System/Library/Frameworks/     \
    -framework CoreFoundation                   \
    -framework AudioToolbox

GCCFLAGS_mac_release := $(DARWIN_GCCFLAGS)
GCCFLAGS_ios_release := $(DARWIN_GCCFLAGS)

GCC_mac := $(GCC_BIN_mac) -isysroot $(SDK_mac)
GCC_ios := $(GCC_BIN_ios) -isysroot $(SDK_ios) -mios-version-min=14.0

mac_example_release:                                \
    $(BUILD_DIR)/mac/release/arm64/Example          \
    $(BUILD_DIR)/mac/release/arm64e/Example

mac_example_debug:                                  \
    $(BUILD_DIR)/mac/debug/arm64/Example            \
    $(BUILD_DIR)/mac/debug/arm64e/Example

ios_example_release:                                \
    $(BUILD_DIR)/ios/release/arm64/Example          \
    $(BUILD_DIR)/ios/release/arm64e/Example

ios_example_debug:                                  \
    $(BUILD_DIR)/ios/debug/arm64/Example            \
    $(BUILD_DIR)/ios/debug/arm64e/Example

mac_release:                                        \
    $(BUILD_DIR)/mac/release/arm64/libBwsrHook      \
    $(BUILD_DIR)/mac/release/arm64e/libBwsrHook

mac_debug:                                          \
    $(BUILD_DIR)/mac/debug/arm64/libBwsrHook        \
    $(BUILD_DIR)/mac/debug/arm64e/libBwsrHook

ios_release:                                        \
    $(BUILD_DIR)/ios/release/arm64/libBwsrHook      \
    $(BUILD_DIR)/ios/release/arm64e/libBwsrHook

ios_debug:                                          \
    $(BUILD_DIR)/ios/debug/arm64/libBwsrHook        \
    $(BUILD_DIR)/ios/debug/arm64e/libBwsrHook

else

mac_example_release:
mac_example_debug:
ios_example_release:
ios_example_debug:
mac_release:
mac_debug:
ios_release:
ios_debug:

endif
