# Minimal Makefile to build lencod on Unix-like systems
CC ?= gcc
CFLAGS ?= -O2 -std=c99
INCLUDES = -Ilencod/inc
SRC = lencod/src/ads_harness.c lencod/src/ads_search.c
BIN_DIR = bin
TARGET = $(BIN_DIR)/lencod

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)

clean:
	rm -rf $(BIN_DIR) build
