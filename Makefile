CC = cc
CFLAGS = -Wall -Wextra -O2
LIBS = -lSDL2 -lm

# Detect operating system for SDL2 linking
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    LIBS += -framework Cocoa
    CFLAGS += -I/usr/local/include -L/usr/local/lib
endif

# Directories
SRC_DIR = src
PREFIX = /usr/local
BIN_DIR = $(PREFIX)/bin

# Targets
all: kxn kxasm tinyc

kxn: $(SRC_DIR)/vm.c $(SRC_DIR)/vm.h
	$(CC) $(CFLAGS) -o kxn $(SRC_DIR)/vm.c $(LIBS)

kxasm: $(SRC_DIR)/assembler.c $(SRC_DIR)/vm.h
	$(CC) $(CFLAGS) -o kxasm $(SRC_DIR)/assembler.c

tinyc: $(SRC_DIR)/compiler.c
	$(CC) $(CFLAGS) -o tinyc $(SRC_DIR)/compiler.c -g

install: all
	install -d $(BIN_DIR)
	install -m 755 kxn $(BIN_DIR)
	install -m 755 kxasm $(BIN_DIR)
	install -m 755 tinyc $(BIN_DIR)

clean:
	rm -f kxn kxasm tinyc

.PHONY: all clean install
