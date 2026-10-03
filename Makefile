CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
CPPFLAGS = -Iinclude

.PHONY: all test stress comments clean sanitize
all: build/search_trees build/test_trees build/benchmark

build:
	mkdir -p build

build/search_trees: src/trees.c src/main.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/trees.c src/main.c -o $@

build/test_trees: src/trees.c tests/test_trees.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/trees.c tests/test_trees.c -o $@

build/benchmark: src/trees.c src/benchmark.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/trees.c src/benchmark.c -o $@

test: build/test_trees
	./build/test_trees

stress: build/test_trees
	./build/test_trees --stress

comments:
	python3 tools/check_comments.py

sanitize: | build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer src/trees.c tests/test_trees.c -o build/test_sanitize
	ASAN_OPTIONS=detect_leaks=1 ./build/test_sanitize

clean:
	rm -rf build
