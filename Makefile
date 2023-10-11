CC := clang++
CFLAGS := -O2 -g --std=c++20

BUILD_DIR = build
TARGET = huffman

.DEFAULT_GOAL := build

config:
	mkdir -p $(BUILD_DIR)

build: config main.cpp
	$(CC) $(CFLAGS) main.cpp -o $(BUILD_DIR)/$(TARGET)

run:
	$(BUILD_DIR)/$(TARGET) a.txt

format:
	clang-format -i *.cpp

clean:
	rm -rf $(BUILD_DIR)/*
