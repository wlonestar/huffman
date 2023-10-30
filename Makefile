CC := clang++
CFLAGS := -O2 -g --std=c++20

BUILD_DIR = build
TARGET = huffman

.DEFAULT_GOAL := build

config:
	mkdir -p $(BUILD_DIR)

build: config main.cpp
	$(CC) $(CFLAGS) main.cpp -o $(BUILD_DIR)/$(TARGET)

test-encode:
	$(BUILD_DIR)/$(TARGET) encode a.txt a.huff

test-decode:
	$(BUILD_DIR)/$(TARGET) decode a.huff b.txt

test: test-encode test-decode

format:
	clang-format -i *.cpp

clean:
	rm -rf $(BUILD_DIR)/*
