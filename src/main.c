/* Interactive entry point; tree algorithms remain reusable by the tests. */
#include "trees.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a whole line before parsing, so invalid input cannot poison the next read. */
/* Return false at EOF; the caller then releases all trees and exits normally. */
static bool read_integer(const char *prompt, int minimum, int maximum, int *result) {
    char line[256]; /* A bounded buffer prevents user input from overflowing storage. */
    for (;;) { /* Retry the same question until a valid integer or EOF is received. */
        fputs(prompt, stdout); /* Prompts come from the caller, not from user input. */
        fflush(stdout); /* Display prompts even when stdout is redirected. */
        if (!fgets(line, sizeof(line), stdin)) return false; /* EOF cancels this request. */
        if (!strchr(line, '\n') && !feof(stdin)) {
            int ch; /* Discard the rest of an overlong line, not just its prefix. */
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            puts("Input too long. Enter one integer per line.");
            continue; /* No part of a rejected line changes the tree. */
        }
        char *end;
        errno = 0; /* strtol reports values outside the long range through errno. */
        long value = strtol(line, &end, 10); /* Decimal parsing accepts signed integer keys. */
        if (end == line || errno == ERANGE) {
            puts("Invalid integer. Please try again.");
            continue; /* Empty input and nonnumeric input are rejected here. */
        }
        while (isspace((unsigned char)*end)) ++end; /* Allow surrounding whitespace. */
        if (*end != '\0' || value < minimum || value > maximum) { /* Require the whole token. */
            printf("Enter one integer in [%d, %d].\n", minimum, maximum);
            continue; /* Reject extra tokens and values outside the requested range. */
        }
        *result = (int)value; /* The range check makes this conversion safe. */
        return true;
    }
}

/* Keep selection separate from operations so the main loop reads like a menu. */
static bool choose_tree(int *selected) {
    puts("\nChoose a tree:");
    for (int i = 0; i < TREE_COUNT; ++i) {
        printf("  %d. %s\n", i + 1, tree_name((TreeKind)i)); /* Match the enum's fixed order. */
    }
    int choice; /* Human-facing choices start at one; enum values start at zero. */
    if (!read_integer("Tree [1-5]: ", 1, TREE_COUNT, &choice)) return false;
    *selected = choice - 1; /* Convert the checked menu number into an array index. */
    return true; /* Switching trees preserves each tree's independently stored keys. */
}

/* One operation consumes one command and, when necessary, one separate key line. */
static void print_menu(const Tree *tree, int selected) {
    printf("\nCurrent tree: %s | size: %zu\n", tree_name((TreeKind)selected), tree_size(tree));
    puts("1. Insert a key");
    puts("2. Delete a key");
    puts("3. Search for a key");
    puts("4. Show size");
    puts("5. Validate tree structure");
    puts("6. Switch tree (preserve all data)");
    puts("0. Exit");
}

/* This is the executable's entry point: create -> select -> operate -> destroy. */
int main(void) {
    Tree *trees[TREE_COUNT]; /* Each kind has its own set for manual exploration. */
    for (int i = 0; i < TREE_COUNT; ++i) {
        trees[i] = tree_create((TreeKind)i); /* All five sets initially contain no keys. */
    }
    puts("Search Tree Playground - enter one integer per line.");
    int selected = 0; /* Initialize before passing its address to the selection helper. */
    bool running = choose_tree(&selected); /* EOF here skips the operation loop safely. */
    while (running) {
        Tree *current = trees[selected]; /* All operations below use the selected set. */
        print_menu(current, selected);
        int command;
        if (!read_integer("Command [0-6]: ", 0, 6, &command)) break; /* EOF exits the menu. */
        if (command == 0) break; /* A normal exit still executes the cleanup below. */
        if (command == 6) {
            running = choose_tree(&selected); /* Cancellation leaves cleanup to the loop exit. */
            continue; /* Refresh the current pointer at the start of the next iteration. */
        }
        int key = 0; /* This placeholder is unused by commands that do not need a key. */
        if (command >= 1 && command <= 3) { /* Only key operations ask for a key. */
            if (!read_integer("Key: ", INT_MIN, INT_MAX, &key)) break;
        }
        switch (command) {
            case 1: /* A duplicate leaves the set and its size unchanged. */
                puts(tree_insert(current, key) ? "Inserted." : "Key already exists.");
                break;
            case 2: /* Deleting a missing key is a supported, nonfatal operation. */
                puts(tree_delete(current, key) ? "Deleted." : "Key not found.");
                break;
            case 3: /* Searching a splay tree may change its shape, but not its keys. */
                puts(tree_contains(current, key) ? "Found." : "Key not found.");
                break;
            case 4: /* The maintained counter avoids a full traversal. */
                printf("Size: %zu\n", tree_size(current));
                break;
            case 5: /* Structural validation is available explicitly for manual testing. */
                puts(tree_validate(current) ? "Structure valid." : "Structure INVALID.");
                break;
            default: /* Input validation makes all other values unreachable. */
                break;
        }
    }
    for (int i = 0; i < TREE_COUNT; ++i) {
        tree_destroy(trees[i]); /* Also free trees that were not selected at exit. */
    }
    if (ferror(stdin)) { /* Distinguish a real input-stream failure from normal EOF. */
        fputs("Input stream error.\n", stderr);
        return EXIT_FAILURE;
    }
    puts("Goodbye.");
    return EXIT_SUCCESS;
}
