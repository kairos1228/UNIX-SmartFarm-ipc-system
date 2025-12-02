# ==============================================================================
# Makefile for Virtual SmartFarm System
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -lrt

SRC_DIR = src
BIN_DIR = bin
INC_DIR = include

TARGETS = $(BIN_DIR)/sensor $(BIN_DIR)/actuator $(BIN_DIR)/server $(BIN_DIR)/monitor

# ==============================================================================
# Default target: Build all executables
# ==============================================================================
all: $(BIN_DIR) $(TARGETS)

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build sensor process
$(BIN_DIR)/sensor: $(SRC_DIR)/main_sensor.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_sensor.c $(LDFLAGS)

# Build actuator process
$(BIN_DIR)/actuator: $(SRC_DIR)/main_actuator.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_actuator.c $(LDFLAGS)

# Build server process
$(BIN_DIR)/server: $(SRC_DIR)/main_server.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_server.c $(LDFLAGS)

# Build monitor process
$(BIN_DIR)/monitor: $(SRC_DIR)/main_monitor.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_monitor.c $(LDFLAGS)

# ==============================================================================
# Clean: Remove all built files
# ==============================================================================
clean:
	rm -rf $(BIN_DIR)

# ==============================================================================
# Help: Display available commands
# ==============================================================================
help:
	@echo "Available targets:"
	@echo "  make all     - Build all executables"
	@echo "  make clean   - Remove all built files"
	@echo "  make help    - Display this help message"

.PHONY: all clean help
