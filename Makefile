CC = gcc
PREFIX = /usr/local

# Try pkg-config first (version-agnostic search)
PKG_CONFIG_NVML = $(shell pkg-config --list-all 2>/dev/null | grep -o 'nvidia-ml[^ ]*' | head -1)
ifneq ($(PKG_CONFIG_NVML),)
    # Found pkg-config package
    NVML_CFLAGS = $(shell pkg-config --cflags $(PKG_CONFIG_NVML) 2>/dev/null)
    NVML_LIBS = $(shell pkg-config --libs $(PKG_CONFIG_NVML) 2>/dev/null)
    NVML_VERSION = $(shell pkg-config --modversion $(PKG_CONFIG_NVML) 2>/dev/null)
    NVML_METHOD = pkg-config ($(PKG_CONFIG_NVML) v$(NVML_VERSION))
else
    # Fallback to manual detection
    NVML_HEADER_PATHS = /usr/include /usr/local/include /usr/local/cuda*/targets/*/include /usr/local/cuda/include /opt/cuda/include /opt/cuda/targets/*/include
    NVML_INCLUDE = $(shell for path in $(NVML_HEADER_PATHS); do \
        expanded_path=$$(echo $$path); \
        for actual_path in $$expanded_path; do \
            if [ -f "$$actual_path/nvml.h" ]; then echo "-I$$actual_path"; exit 0; fi; \
        done; \
        done)

    NVML_LIB_PATHS = /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib64 /usr/local/lib /usr/local/lib64 /usr/local/cuda*/targets/*/lib /usr/local/cuda/lib64 /opt/cuda/lib64 /opt/cuda/targets/*/lib
    NVML_LIBDIR = $(shell for path in $(NVML_LIB_PATHS); do \
        expanded_path=$$(echo $$path); \
        for actual_path in $$expanded_path; do \
            if [ -f "$$actual_path/libnvidia-ml.so" ] || [ -f "$$actual_path/stubs/libnvidia-ml.so" ]; then echo "-L$$actual_path"; exit 0; fi; \
        done; \
        done)

    NVML_CFLAGS = $(NVML_INCLUDE)
    NVML_LIBS = $(NVML_LIBDIR) -lnvidia-ml
    NVML_METHOD = manual detection
endif

# Check if NVML was found
ifeq ($(NVML_CFLAGS),)
    $(error NVML headers not found. Please install nvidia-ml-dev or CUDA toolkit)
endif
ifeq ($(NVML_LIBS),)
    $(error NVML library not found. Please install nvidia-ml-dev or CUDA toolkit)
endif

CFLAGS = -Wall -Wextra -std=c99 -O2 $(NVML_CFLAGS)
LDFLAGS = $(NVML_LIBS)

TARGET = nvml-tool
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (default: /usr/local/bin, configurable with PREFIX)
install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

# Uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

# Show detected NVML paths
show-nvml:
	@echo "Detected NVML configuration:"
	@echo "  Method: $(NVML_METHOD)"
	@echo "  CFLAGS: $(NVML_CFLAGS)"
	@echo "  LIBS: $(NVML_LIBS)"

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the program (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to PREFIX/bin (default: /usr/local/bin)"
	@echo "  uninstall - Remove from PREFIX/bin"
	@echo "  show-nvml - Show detected NVML paths"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX    - Installation prefix (default: /usr/local)"
	@echo "              Example: make install PREFIX=/usr"

.PHONY: all clean install uninstall show-nvml help