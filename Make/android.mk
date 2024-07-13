
ifdef ANDROID_NDK

PLATFORM :=

ifeq ($(UNAME),Darwin)
$(eval PLATFORM := darwin)
else ifeq ($(UNAME),Linux)
$(eval PLATFORM := linux)
endif

ANDROID_TOOLCHAIN   := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/clang
ANDROID_SYSROOT     := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/sysroot
AR_android          := $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(PLATFORM)-x86_64/bin/llvm-ar

ANDROID_ARCH_FLAGS  := -target aarch64-none-linux-android30
ANDROID_ARCHS       := arm64
ANDROID_PLATFORMS   := android

ANDROID_DIRS :=                     \
    Memory                          \
    Hook

EXAMPLE_LDFLAGS_android :=          \
    -pie                            \
    -llog                           \
    -I.                             \
    $(ANDROID_ARCH_FLAGS)

ANDROID_GCCFLAGS :=                 \
    $(ANDROID_ARCH_FLAGS)           \
    -Os                             \
    -Wall                           \
    -Wextra                         \
    -Werror                         \
    --sysroot=$(ANDROID_SYSROOT)

GCCFLAGS_android_debug :=           \
    $(ANDROID_GCCFLAGS)             \
    -DDEBUG_MODE

GCCFLAGS_android_release := $(ANDROID_GCCFLAGS)

GCC_android := $(ANDROID_TOOLCHAIN)

android_example_release:                    \
    build/android/release/arm64/Example

android_example_debug:                      \
    build/android/debug/arm64/Example

android_release:                            \
    build/android/release/arm64/libBwsrHook

android_debug:                              \
    build/android/debug/arm64/libBwsrHook

else

android_example_release:
android_example_debug:
android_release:
android_debug:

endif
