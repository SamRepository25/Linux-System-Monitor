# Makefile for linux-system-monitor
# Targets:
#   make        -> build the binary at build/sysmon
#   make clean  -> remove build artifacts
#   make run    -> build (if needed) and run the binary

CC      := gcc
STD     := -std=c17
WARN    := -Wall -Wextra -Werror
CFLAGS  := $(STD) $(WARN) -g -Iinclude
LDFLAGS :=

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/sysmon

# Source files: main.c plus every .c file in modules/
SRCS := src/main.c $(wildcard modules/*.c)

# Turn src/main.c -> build/src/main.o, modules/sysinfo.c -> build/modules/sysinfo.o
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Pattern rule: how to build any build/<path>.o from its matching <path>.c
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts"
