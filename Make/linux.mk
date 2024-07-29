
CLANG_ARCH   :=
UNAME        := $(shell uname -s)

ifeq ($(UNAME),Linux)

LINUX_TOOLCHAIN := clang
AR_linux        := ar
LINUX_ARCHS     := arm64
LINUX_PLATFORMS := linux

LINUX_DIRS :=               \
	Memory                  \
	Hook					\
	SymbolResolve/Linux

LINUX_GCCFLAGS :=           \
	-Os                     \
	-Wall                   \
	-Wextra                 \
	-Werror

EXAMPLE_LDFLAGS_linux :=

GCCFLAGS_linux_debug :=     \
	$(LINUX_GCCFLAGS)       \
	-DDEBUG_MODE

GCCFLAGS_linux_release := $(LINUX_GCCFLAGS)
GCC_linux := $(LINUX_TOOLCHAIN)

$(eval CLANG_ARCH := $(shell clang -dumpmachine | cut -d '-' -f 1))

endif

ifeq ($(CLANG_ARCH),aarch64)

linux_example_release:                              \
	$(BUILD_DIR)/linux/release/arm64/Example

linux_example_debug:                                \
	$(BUILD_DIR)/linux/debug/arm64/Example

linux_release:                                      \
	$(BUILD_DIR)/linux/release/arm64/libBwsrHook

linux_debug:                                        \
	$(BUILD_DIR)/linux/debug/arm64/libBwsrHook

else

linux_example_release:
linux_example_debug:
linux_release:
linux_debug:

endif
