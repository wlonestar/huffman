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
	$(BUILD_DIR)/$(TARGET) encode example/a.txt example/a.huff

test-decode:
	$(BUILD_DIR)/$(TARGET) decode example/a.huff example/b.txt

test: test-encode test-decode

format:
	clang-format -i *.cpp *.hpp

clean:
	rm -rf $(BUILD_DIR)/*

commit ?= test

push: clean
	@git add .
	@git commit -m "$(commit)"
	@git push
	@sync #sync all files in os
