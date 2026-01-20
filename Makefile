CC = gcc
PREFIX = /usr/local

# Try pkg-config first (version-agnostic search)
PKG_CONFIG_NVML = $(shell pkg-config --list-all 2>/dev/null | grep -o 'nvidia-ml[^ ]*' | head -1)
ifneq ($(PKG_CONFIG_NVML),)
    # Found pkg-config package
    NVML_CFLAGS = $(shell pkg-config --cflags $(PKG_CONFIG_NVML) 2>/dev/null)
    NVML_LIBS = $(shell pkg-config --libs $(PKG_CONFIG_NVML) 2>/dev/null)
else
    # No pkg-config found - check if user provided NVML_CFLAGS and NVML_LIBS
    ifeq ($(NVML_CFLAGS),)
        $(error NVML not found via pkg-config. Please provide NVML_CFLAGS and NVML_LIBS. Example: make NVML_CFLAGS="-I/usr/local/cuda/include" NVML_LIBS="-L/usr/local/cuda/lib64 -lnvidia-ml")
    endif
    ifeq ($(NVML_LIBS),)
        $(error NVML not found via pkg-config. Please provide NVML_CFLAGS and NVML_LIBS. Example: make NVML_CFLAGS="-I/usr/local/cuda/include" NVML_LIBS="-L/usr/local/cuda/lib64 -lnvidia-ml")
    endif
endif

CFLAGS = -Wall -Wextra -std=c99 -O2 $(NVML_CFLAGS)
LDFLAGS = $(NVML_LIBS)

# Directories
SRCDIR = src
BUILDDIR = build

TARGET = $(BUILDDIR)/nvml-tool
SOURCES = $(SRCDIR)/main.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS) | $(BUILDDIR)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Install (default: /usr/local/bin, configurable with PREFIX)
install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

# Uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

# Show detected NVML paths
show-nvml:
	@echo "NVML configuration:"
	@echo "  CFLAGS: $(NVML_CFLAGS)"
	@echo "  LIBS: $(NVML_LIBS)"

# Show help
help:
	@echo "Available targets:"
	@echo "  all                   - Build the program (default)"
	@echo "  clean                 - Remove build artifacts"
	@echo "  install               - Install to PREFIX/bin (default: /usr/local/bin)"
	@echo "  uninstall             - Remove from PREFIX/bin"
	@echo "  show-nvml             - Show detected NVML paths"
	@echo "  compile_commands.json - Generate clangd compile database"
	@echo "  help                  - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX      - Installation prefix (default: /usr/local)"
	@echo "                Example: make install PREFIX=/usr"
	@echo "  NVML_CFLAGS - NVML compiler flags (auto-detected or user-provided)"
	@echo "                Example: make NVML_CFLAGS=\"-I/usr/local/cuda/include\""
	@echo "  NVML_LIBS   - NVML linker flags (auto-detected or user-provided)"
	@echo "                Example: make NVML_LIBS=\"-L/usr/local/cuda/lib64 -lnvidia-ml\""

# Generate compile_commands.json for clangd
compile_commands.json: $(SOURCES)
	@printf '[\n  {\n    "directory": "%s",\n    "file": "%s",\n    "command": "%s %s -c %s -o %s"\n  }\n]\n' \
		"$(CURDIR)" "$(CURDIR)/$(SOURCES)" "$(CC)" "$(CFLAGS)" "$(CURDIR)/$(SOURCES)" "$(CURDIR)/$(OBJECTS)" > $@
	@echo "Generated $@"

.PHONY: all clean install uninstall show-nvml help