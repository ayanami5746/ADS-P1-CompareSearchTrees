#ifndef TREES_H
#define TREES_H
/* All implementations store distinct int keys, including INT_MIN/INT_MAX. */
#include <stdbool.h>
#include <stddef.h>
typedef enum { TREE_BST, TREE_AVL, TREE_SPLAY, TREE_RB, TREE_BPLUS, TREE_COUNT } TreeKind;
/* The opaque handle prevents callers from corrupting implementation links. */
typedef struct Tree Tree;
/* Except destroy(NULL), public operations require a valid non-null handle. */
/* Allocation failure prints a diagnostic and terminates with EXIT_FAILURE. */
Tree *tree_create(TreeKind kind);
/* Duplicate inserts and absent deletes return false without changing the set. */
bool tree_insert(Tree *tree, int key);
bool tree_delete(Tree *tree, int key);
/* Splay lookups may change the shape, including unsuccessful lookups. */
bool tree_contains(Tree *tree, int key);
size_t tree_size(const Tree *tree);
/* Validation is intentionally outside benchmark timed sections. */
bool tree_validate(const Tree *tree);
/* Destruction is safe for a NULL tree and for an empty tree. */
void tree_destroy(Tree *tree);
const char *tree_name(TreeKind kind);
#endif
