CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I/usr/local/cuda-12.9/targets/x86_64-linux/include
LDFLAGS = -L/usr/local/cuda-12.9/targets/x86_64-linux/lib -lnvidia-ml

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

# Install to /usr/local/bin (requires sudo)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall from /usr/local/bin (requires sudo)
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the program (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall - Remove from /usr/local/bin (requires sudo)"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall help