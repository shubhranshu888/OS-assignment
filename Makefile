# CampusCompute Makefile
# Target: Linux / Ubuntu / WSL Ubuntu

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS ?= -pthread -lrt

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN = campuscompute

SRCS = $(SRC_DIR)/common.c \
       $(SRC_DIR)/job.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/scheduler.c \
       $(SRC_DIR)/ipc.c \
       $(SRC_DIR)/banker.c \
       $(SRC_DIR)/worker.c \
       $(SRC_DIR)/rlimit_demo.c \
       $(SRC_DIR)/main.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean test run zip help

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build successful: ./$(BIN)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) logs

clean:
	rm -rf $(BUILD_DIR) $(BIN) logs *.zip
	@echo "Cleanup complete."

test: $(BIN)
	./$(BIN) demo

zip:
	@echo "Creating submission archive..."
	zip -r campuscompute_submission.zip Makefile README.md include src data
	@echo "Archive campuscompute_submission.zip created."

help:
	@echo "CampusCompute Build Targets:"
	@echo "  make         - Compile the project and link $(BIN)"
	@echo "  make clean   - Remove object files, binaries, and temporary logs"
	@echo "  make test    - Run integrated test demonstration"
	@echo "  make zip     - Package source files and Makefile into zip for submission"
