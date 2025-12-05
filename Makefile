# ==============================================================================
# Makefile for Virtual SmartFarm System
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS_PTHREAD = -pthread   # pthread 라이브러리

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
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_sensor.c

# Build actuator process
$(BIN_DIR)/actuator: $(SRC_DIR)/main_actuator.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_actuator.c

# Build server process (with pthread)
$(BIN_DIR)/server: $(SRC_DIR)/main_server.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_server.c $(LDFLAGS_PTHREAD)

# Build monitor process
$(BIN_DIR)/monitor: $(SRC_DIR)/main_monitor.c $(INC_DIR)/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main_monitor.c

# ==============================================================================
# Clean: Remove all built files
# ==============================================================================
clean:
	rm -rf $(BIN_DIR)

# ==============================================================================
# Help: Display available commands
# ==============================================================================
help:
	@echo "======================================"
	@echo "  Virtual SmartFarm Build System"
	@echo "======================================"
	@echo ""
	@echo "Available targets:"
	@echo "  make all     - Build all executables"
	@echo "  make clean   - Remove all built files"
	@echo "  make help    - Display this help message"
	@echo ""
	@echo "Execution order:"
	@echo "  1. ./bin/server   (먼저 실행)"
	@echo "  2. ./bin/sensor"
	@echo "  3. ./bin/actuator"
	@echo "  4. ./bin/monitor"

.PHONY: all clean help
