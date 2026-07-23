UNAME_S := $(shell uname -s)

PREFIX ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man

STRIP ?= strip
INSTALL ?= install

BUILD = build
BIN   = gitm

HEADERS   = $(wildcard include/*.h)

SRC       += $(wildcard src/*.c)
SRC       += $(wildcard src/commands/*.c)
SRC       += $(wildcard src/config/*.c)
SRC       += $(wildcard src/core/*.c)
SRC       += $(wildcard src/git/*.c)
SRC       += $(wildcard src/util/*.c)

# Argparse library
SRC       += $(wildcard argparse/src/*.c)

CFLAGS += -Isrc -Iinclude -Iargparse/include -std=c17 -DCOMPILED_TIME_PREFIX='"$(PREFIX)"'

CFLAGS +=  -Wshadow -Wconversion \
           -Wall -Wextra -Wpedantic \
           -Wno-missing-field-initializers \
           -Wstrict-prototypes -Wmissing-prototypes

LDLIBS += -lpthread

# Build options (set via command line, e.g. `make O_DEBUG=1`)
O_DEBUG := 0                     ## Enable debug build (ASan, UBSan, -g3)
O_LOG_SHOW_SOURCE_LOCATION := 0  ## Prepend [file:line:func] to log output
O_LOG_SHOW_TIME_STAMP := 0       ## Prepend [HH:MM:SS.ffffff] to log output

# Auto-enable flags for debug builds
ifneq ($(filter debug,$(MAKECMDGOALS)),)
	O_DEBUG := 1
	O_LOG_SHOW_SOURCE_LOCATION := 1
	O_LOG_SHOW_TIME_STAMP := 1
endif

ifeq ($(strip $(O_DEBUG)),1)
	CFLAGS += -g3 -DDEBUG -DLOG_SHOW_SOURCE_LOCATION

	LDFLAGS += -fsanitize=address -fsanitize=undefined
	CFLAGS += -fstack-usage \
	          -fsanitize=address \
	          -fsanitize=undefined

    ifneq (,$(findstring clang,$(CC)))
		CFLAGS += -ffreestanding
    endif
else
	CFLAGS += -O3
endif

# Convert O_ variables to -D flags
ifeq ($(strip $(O_LOG_SHOW_SOURCE_LOCATION)),1)
	CFLAGS += -DLOG_SHOW_SOURCE_LOCATION
endif

ifeq ($(strip $(O_LOG_SHOW_TIME_STAMP)),1)
	CFLAGS += -DLOG_SHOW_TIME_STAMP
endif

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
	# macOS: no argp needed (we use our own argparse)
else
	# Linux: _GNU_SOURCE for strptime, etc.
	CFLAGS += -D_GNU_SOURCE
endif

OUT += $(SRC:%.c=$(BUILD)/%.o)

all: $(BIN)

help:  ## Show this help
	@echo "Variable:"
	@awk 'BEGIN {FS="  ## "} \
		/^O_[a-zA-Z_]+[[:space:]]*:=/ { \
		split($$1, a, /[[:space:]]*:=/); \
		printf "  \033[36m%-30s\033[0m %s\n", a[1], $$2; \
	}' $(MAKEFILE_LIST)

	@echo
	@echo "Targets:"
	@grep -hE '^[a-zA-Z_-]+:.*  ## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS="  ## "}; {printf "  \033[33m%-15s\033[0m %s\n", $$1, $$2}'

$(BUILD):  ## Create build directories automatically
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(SRC) $(OUT)  ## Build the gitm binary
	$(CC) $(LDFLAGS) -o $@ $(OUT) $(LDLIBS)

debug: $(BIN)  ## Build the debug binary run `make debug -B O_DEBUG=1`

format:  ## Format every (.c & .h) files of entire code base with `clang-format`
	clang-format -i $(SRC) $(HEADERS)

install: all  ## Install the gitm binary
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin

clean:  ## Clean up gitm artifacts
	$(RM) -f $(OUT) $(BIN)

uninstall:  ## Uninstall the gitm binary
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)

strip: $(BIN)  ## Strip the gitm binary
	$(STRIP) $^

.PHONY: all install uninstall strip clean debug
