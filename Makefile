CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude

PLATFORM ?= sdl2

VM_SOURCES = src/vm.c
COMMON_HEADERS = src/vm.h src/platform_io.h

ifeq ($(PLATFORM),sdl2)
    PLATFORM_SOURCES = src/platforms/sdl2/platform_io.c
    PLATFORM_LIBS = -lSDL2
    TARGET = kxn
    PLATFORM_CFLAGS = 
endif

# All sources
SOURCES = $(VM_SOURCES) $(PLATFORM_SOURCES)
OBJECTS = $(SOURCES:.c=.o)

# Build targets
.PHONY: all clean sdl2 esp32 install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(PLATFORM_LIBS)
	@echo "Built $(TARGET) for $(PLATFORM) platform"

%.o: %.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) $(INCLUDES) -c $< -o $@

sdl2:
	$(MAKE) PLATFORM=sdl2

clean:
	rm -f $(OBJECTS) kxn 
	@echo "Cleaned build artifacts"

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed $(TARGET) to /usr/local/bin/"


# Development help
.PHONY: help
help:
	@echo "KXN VM Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build for default platform (SDL2)"
	@echo "  sdl2       - Build for SDL2 platform"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install to system"
	@echo "  examples   - Show example usage"
	@echo ""
	@echo "Platform selection:"
	@echo "  make PLATFORM=sdl2   - Build SDL2 version"
