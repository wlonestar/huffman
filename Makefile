CC := clang++
CFLAGS := -O2 -g --std=c++20

BUILD_DIR = build
TARGET = huffman

.DEFAULT_GOAL := build

config:
	mkdir -p $(BUILD_DIR)

build: config main.cpp huffman.hpp
	$(CC) $(CFLAGS) main.cpp -o $(BUILD_DIR)/$(TARGET)

test-encode:
	$(BUILD_DIR)/$(TARGET) -m encode -i example/a.txt -o example/a.huff

test-decode:
	$(BUILD_DIR)/$(TARGET) -m decode -i example/a.huff -o example/b.txt

test: test-encode test-decode

format:
	clang-format -i *.cpp *.hpp

clean:
	rm -rf $(BUILD_DIR)/* example/*.huff example/b.txt

commit ?= test

push: clean
	@git add .
	@git commit -m "$(commit)"
	@git push
	@sync #sync all files in os
