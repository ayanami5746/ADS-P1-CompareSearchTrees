CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
CPPFLAGS = -Iinclude

.PHONY: all test stress comments clean sanitize
all: build/test_trees build/benchmark build/test_lecture

build:
	mkdir -p build

build/test_trees: src/trees.c tests/test_trees.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/trees.c tests/test_trees.c -o $@

build/benchmark: src/trees.c src/benchmark.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/trees.c src/benchmark.c -o $@

build/test_lecture: tests/test_lecture.c src/trees.c include/trees.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_lecture.c -o $@

test: build/test_trees build/test_lecture
	./build/test_lecture
	./build/test_trees

stress: build/test_trees build/test_lecture
	./build/test_lecture
	./build/test_trees --stress

comments:
	python3 tools/check_comments.py

sanitize: | build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer src/trees.c tests/test_trees.c -o build/test_sanitize
	ASAN_OPTIONS=detect_leaks=1 ./build/test_sanitize

	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_lecture.c -o build/lecture_sanitize
	ASAN_OPTIONS=detect_leaks=1 ./build/lecture_sanitize

clean:
	rm -rf build
