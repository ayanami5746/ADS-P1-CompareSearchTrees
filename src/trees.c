/* Search-tree library: no application main() belongs in this file.
 * Reading map: data -> shared primitives -> five algorithms -> public API.
 * Validation and destruction are separate from timed update operations.
 * Start at section 8 to understand dispatch, then read the relevant module.
 * Tests and benchmarks provide their own independent executable entry points.
 */
#include "trees.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


/* ==================== 1. Data structures ==================== */
/* Node serves binary trees; Page serves B+ trees; Tree owns the roots. */

/* Binary nodes share storage; height is used by AVL and red by red-black. */
typedef struct Node {
    int key;             /* The integer stored by this node. */
    int height;          /* Cached AVL height; unused for ordering. */
    bool red;            /* Link color used only by the red-black tree. */
    struct Node *left;   /* All descendant keys are smaller. */
    struct Node *right;  /* All descendant keys are larger. */
} Node;
/* B+ order means maximum children; leaves hold at most ORDER-1 records. */
#define ORDER 16
#define LEAF_MIN (ORDER / 2)
#define INNER_MIN (ORDER / 2)
/* One extra slot lets insertion temporarily overflow before splitting. */
typedef struct Page {
    bool leaf;           /* Distinguish record pages from routing pages. */
    int n;               /* Leaf: record count. Internal: child count. */
    int minimum;         /* Smallest record reachable from this page. */
    int keys[ORDER];
    struct Page *child[ORDER + 1]; /* Include a temporary overflow slot. */
    struct Page *next;            /* Ordered leaf link; not an owning pointer. */
} Page;
/* For an internal page, n counts children and keys[i] = child[i+1].minimum. */
struct Tree {
    TreeKind kind;       /* Select the algorithm at the public API boundary. */
    size_t size;         /* Count distinct records, not B+ separator copies. */
    Node *root;          /* Root for the four binary-tree implementations. */
    Page *page;          /* Root for B+; unused for binary trees. */
};

/* ==================== 2. Shared binary-tree primitives ==================== */
/* Allocation, rotations and ordinary lookup do not choose a tree kind. */

/* Centralized checked allocation avoids returning partially updated trees. */
/*
 * Zero initialization establishes null links and empty counters.
 * Failure is fatal because mutations do not implement rollback.
 * Allocation cost remains part of the measured insert operation.
 */
static void *allocate(size_t n) {
    void *p = calloc(1, n);
    if (!p) { fputs("Out of memory\n", stderr); exit(EXIT_FAILURE); }
    return p;
}
/* All fresh binary nodes are leaves; RB insertion initially colors them red. */
/*
 * Height one is the base case required by AVL insertion.
 * Red is the initial color required by red-black insertion.
 * No key value is reserved as an empty-node sentinel.
 */
static Node *new_node(int key) {
    Node *p = allocate(sizeof(*p));
    p->key = key; p->height = 1; p->red = true;
    return p;
}
/* Null children have height zero in the AVL height convention. */
/*
 * An empty subtree contributes zero to its parent's height.
 */
static int height(Node *p) { return p ? p->height : 0; }
/*
 * In AVL use, both child heights must already be correct.
 * The result counts nodes rather than edges on the longest path.
 * Rotations call it bottom-up so dependent heights are available.
 */
static void fix_height(Node *p) {
    int l = height(p->left), r = height(p->right);
    p->height = 1 + (l > r ? l : r);
}
/* Rotations preserve in-order traversal and repair both affected heights. */
/*
 * The old root and its right child must both exist.
 * Height repair visits the lowered node before the promoted node.
 * Color transfer is deliberately left to the red-black wrapper.
 */
static Node *rotate_left(Node *p) {
    Node *q = p->right;
    p->right = q->left; q->left = p;
    fix_height(p); fix_height(q);
    return q;
}
/*
 * The old root and its left child must both exist.
 * The caller must preserve the returned root pointer.
 * Cached heights are repaired in dependency order.
 */
static Node *rotate_right(Node *p) {
    Node *q = p->left;
    p->left = q->right; q->right = p;
    fix_height(p); fix_height(q);
    return q;
}
/* A failed lookup in a binary tree needs no allocation or sentinel key. */
/*
 * The helper does not splay, rotate, recolor, or allocate.
 * It is also used to establish RB deletion's membership precondition.
 * The work is proportional to the actual search-path height.
 */
static bool binary_contains(Node *p, int k) {
    while (p && p->key != k) p = k < p->key ? p->left : p->right;
    return p != NULL;
}

/* ==================== 3. Unbalanced BST ==================== */
/* Iterative search, insertion and successor-based deletion. */

/* Find the slot that owns a key, or the null slot where it belongs. */
/* Returning a link address makes root and child updates use the same code. */
static Node **bst_find_slot(Node **root, int key) {
    Node **slot = root;
    while (*slot && (*slot)->key != key) {
        slot = key < (*slot)->key ? &(*slot)->left : &(*slot)->right;
    }
    return slot;
}

/* Plain BST insertion performs no balancing or sorted-input shortcut. */
static bool bst_insert(Tree *tree, int key) {
    Node **slot = bst_find_slot(&tree->root, key);
    if (*slot) return false; /* A duplicate must not allocate a second record. */
    *slot = new_node(key);
    return true;
}

/* Unlink exactly one node; the public interface updates the record count. */
static bool bst_delete(Tree *tree, int key) {
    Node **slot = bst_find_slot(&tree->root, key);
    if (!*slot) return false;
    Node *node = *slot;
    if (node->left && node->right) {
        /* Copy the successor key, then remove the successor at its own slot. */
        Node **successor = &node->right;
        while ((*successor)->left) successor = &(*successor)->left;
        node->key = (*successor)->key;
        slot = successor;
        node = *slot;
    }
    /* At this point the physically removed node has at most one child. */
    *slot = node->left ? node->left : node->right;
    free(node);
    return true;
}

/* ==================== 4. AVL tree ==================== */
/* Recursive updates restore heights and balance while unwinding. */

/* AVL repairs the first unbalanced direction, including a double rotation. */
/*
 * Both children must already satisfy the AVL invariant.
 * An inward-heavy child requires a preliminary child rotation.
 * An outward-heavy or equally balanced child needs only one rotation.
 */
static Node *balance(Node *p) {
    if (!p) return NULL;
    fix_height(p);
    if (height(p->left) - height(p->right) > 1) {
        if (height(p->left->left) < height(p->left->right))
            p->left = rotate_left(p->left);
        return rotate_right(p);
    }
    if (height(p->right) - height(p->left) > 1) {
        if (height(p->right->right) < height(p->right->left))
            p->right = rotate_right(p->right);
        return rotate_left(p);
    }
    return p;
}
/* AVL recursion is bounded by logarithmic height, unlike plain BST recursion. */
/*
 * The changed flag is initially false and is set only at allocation.
 * The return value may differ from the incoming subtree root.
 * Balanced height bounds both runtime and recursion by O(log N).
 */
static Node *avl_insert(Node *p, int k, bool *changed) {
    if (!p) { *changed = true; return new_node(k); }
    if (k < p->key) p->left = avl_insert(p->left, k, changed);
    else if (k > p->key) p->right = avl_insert(p->right, k, changed);
    return balance(p);
}
/* Replace a two-child node with its successor, then repair the removal path. */
/*
 * An absent key reaches NULL without setting the changed flag.
 * Removing that successor physically frees exactly one node.
 * The shared changed flag must not be counted twice by the caller.
 */
static Node *avl_delete(Node *p, int k, bool *changed) {
    if (!p) return NULL;
    if (k < p->key) p->left = avl_delete(p->left, k, changed);
    else if (k > p->key) p->right = avl_delete(p->right, k, changed);
    else {
        *changed = true;
        if (!p->left || !p->right) {
            Node *q = p->left ? p->left : p->right;
            free(p); return q;
        }
        Node *q = p->right;
        while (q->left) q = q->left;
        p->key = q->key;
        p->right = avl_delete(p->right, q->key, changed);
    }
    return balance(p);
}

/* ==================== 5. Splay tree ==================== */
/* Top-down splaying partitions the tree before insertion or deletion. */

/* Top-down splaying uses two temporary chains and constant auxiliary space. */
/*
 * Move a matching key, or the last accessed boundary, to the root.
 * A single access can cost O(N); sequence cost is amortized O(log N).
 * The caller replaces its root even when the requested key is absent.
 */
static Node *splay(Node *p, int k) {
    Node dummy = {0}, *l = &dummy, *r = &dummy;
    if (!p) return NULL;
    for (;;) {
        if (k < p->key) {
            if (!p->left) break;
            /* Zig-zig brings the grandchild closer before linking the chain. */
            if (k < p->left->key) p = rotate_right(p);
            if (!p->left) break;
            r->left = p; r = p; p = p->left;
        } else if (k > p->key) {
            if (!p->right) break;
            if (k > p->right->key) p = rotate_left(p);
            if (!p->right) break;
            l->right = p; l = p; p = p->right;
        } else break;
    }
    /* Reassemble: left chain < root < right chain, even for an absent key. */
    l->right = p->left; r->left = p->right;
    p->left = dummy.right; p->right = dummy.left;
    return p;
}


/* Expose the search boundary before attaching a new root. */
static bool splay_insert(Tree *tree, int key) {
    tree->root = splay(tree->root, key);
    if (tree->root && tree->root->key == key) return false;
    Node *node = new_node(key);
    if (tree->root && key < tree->root->key) {
        /* The smaller partition becomes the new root's left subtree. */
        node->left = tree->root->left;
        node->right = tree->root;
        tree->root->left = NULL;
    } else if (tree->root) {
        /* The larger partition becomes the new root's right subtree. */
        node->right = tree->root->right;
        node->left = tree->root;
        tree->root->right = NULL;
    }
    tree->root = node;
    return true;
}

/* Remove the exposed root and join the two surviving ordered partitions. */
static bool splay_delete(Tree *tree, int key) {
    tree->root = splay(tree->root, key);
    if (!tree->root || tree->root->key != key) return false;
    Node *removed = tree->root;
    if (!removed->left) {
        tree->root = removed->right;
    } else {
        /* key exceeds every key on the left, so its maximum becomes root. */
        tree->root = splay(removed->left, key);
        tree->root->right = removed->right;
    }
    free(removed);
    return true;
}

/* ==================== 6. Left-leaning red-black tree ==================== */
/* Rotations and color transfers preserve the black-height invariant. */

/* Left-leaning red-black trees are a standard red-black-tree variant. */
/*
 * Treat null child links as black external leaves.
 */
static bool red(Node *p) { return p && p->red; }
/* RB rotations transfer the former root color and mark the lowered node red. */
/*
 * The caller guarantees that the right child exists.
 * The promoted node inherits the old root's incoming-link color.
 * The lowered root becomes connected by a red link.
 */
static Node *rb_left(Node *p) {
    Node *q = rotate_left(p); q->red = p->red; p->red = true; return q;
}
/*
 * The caller guarantees that the left child exists.
 * The old root color moves to the promoted node.
 * The lowered node receives red, preserving black-height accounting.
 */
static Node *rb_right(Node *p) {
    Node *q = rotate_right(p); q->red = p->red; p->red = true; return q;
}
/* A color flip splits a temporary four-node or combines two two-nodes. */
/*
 * On valid repair paths the relevant child nodes are present.
 * Insertion uses this to split a temporary four-node.
 * Deletion uses the reverse transformation to combine two-nodes.
 */
static void flip(Node *p) {
    p->red = !p->red;
    if (p->left) p->left->red = !p->left->red;
    if (p->right) p->right->red = !p->right->red;
}
/*
 * A red right link is first rotated to the left.
 * Two consecutive left red links are then rotated right.
 * Two red children finally trigger a color split.
 */
static Node *rb_fix(Node *p) {
    if (red(p->right)) p = rb_left(p);
    if (red(p->left) && red(p->left->left)) p = rb_right(p);
    if (red(p->left) && red(p->right)) flip(p);
    return p;
}
/* Insert as a red leaf, then restore left leaning and eliminate red-red edges. */
/*
 * The changed flag distinguishes allocation from duplicate insertion.
 * Ancestor repair removes right-red and consecutive-red configurations.
 * The resulting root color is normalized by tree_insert.
 */
static Node *rb_insert(Node *p, int k, bool *changed) {
    if (!p) { *changed = true; return new_node(k); }
    if (k < p->key) p->left = rb_insert(p->left, k, changed);
    else if (k > p->key) p->right = rb_insert(p->right, k, changed);
    return rb_fix(p);
}
/* Move a red link left so deletion never descends into a black two-node. */
/*
 * The descent target is black and its left child is also black.
 * A color flip temporarily merges the neighboring two-nodes.
 * If the sibling can supply red, rotations transfer that red link.
 */
static Node *move_left(Node *p) {
    flip(p);
    if (p->right && red(p->right->left)) {
        p->right = rb_right(p->right); p = rb_left(p); flip(p);
    }
    return p;
}
/* The symmetric transfer supplies a red link to the right deletion path. */
/*
 * The target lacks a red link available for immediate removal.
 * A red left-left grandchild allows borrowing from the opposite side.
 * Right rotation and recoloring complete that transfer.
 */
static Node *move_right(Node *p) {
    flip(p);
    if (p->left && red(p->left->left)) { p = rb_right(p); flip(p); }
    return p;
}
/*
 * A minimum with no left child has no surviving right child here.
 * Before descent, a red link is moved into a black two-node.
 * Unwinding restores the left-leaning representation.
 */
static Node *rb_delete_min(Node *p) {
    if (!p->left) { free(p); return NULL; }
    if (!red(p->left) && !red(p->left->left)) p = move_left(p);
    p->left = rb_delete_min(p->left);
    return rb_fix(p);
}
/* Public deletion first confirms membership, which guarantees the path exists. */
/*
 * Delete a key known to exist in the incoming subtree.
 * Red links are moved downward before entering black two-nodes.
 * Rotations can change the root key, so equality is tested again.
 */
static Node *rb_delete(Node *p, int k) {
    if (k < p->key) {
        if (!red(p->left) && !red(p->left->left)) p = move_left(p);
        p->left = rb_delete(p->left, k);
    } else {
        if (red(p->left)) p = rb_right(p);
        if (k == p->key && !p->right) { free(p); return NULL; }
        if (!red(p->right) && !red(p->right->left)) p = move_right(p);
        if (k == p->key) {
            Node *q = p->right;
            while (q->left) q = q->left;
            p->key = q->key; p->right = rb_delete_min(p->right);
        } else p->right = rb_delete(p->right, k);
    }
    return rb_fix(p);
}


/* Keep root-color setup and membership preconditions inside the RB module. */
static bool rb_remove_key(Tree *tree, int key) {
    if (!binary_contains(tree->root, key)) return false;
    /* A red root supplies the first top-down color redistribution step. */
    if (!red(tree->root->left) && !red(tree->root->right)) {
        tree->root->red = true;
    }
    tree->root = rb_delete(tree->root, key);
    if (tree->root) tree->root->red = false;
    return true;
}

/* ==================== 7. B+ tree ==================== */
/* Page algorithms handle local repair; wrappers handle root changes. */

/* B+ records exist only in leaves; internal keys are routing copies. */
/*
 * The caller fills records or children before exposing the page.
 * An empty page's minimum is not meaningful yet.
 * Page storage includes one temporary overflow slot.
 */
static Page *new_page(bool leaf) {
    Page *p = allocate(sizeof(*p)); p->leaf = leaf; return p;
}
/* Cached minima make separator repair proportional to order, not tree height. */
/*
 * Internal pages must have at least one initialized child.
 * Each internal separator copies the following child's minimum.
 * Children must be refreshed before their parent is refreshed.
 */
static void refresh(Page *p) {
    if (p->leaf) { if (p->n) p->minimum = p->keys[0]; return; }
    p->minimum = p->child[0]->minimum;
    for (int i = 1; i < p->n; ++i) p->keys[i - 1] = p->child[i]->minimum;
}
/* Equality follows the right child because separators copy its minimum. */
/*
 * The page must be internal and have at least one child.
 * Equality belongs to the child on the separator's right.
 * Linear scanning is bounded by the fixed B+ order.
 */
static int route(Page *p, int k) {
    int i = 0;
    while (i + 1 < p->n && k >= p->keys[i]) ++i;
    return i;
}
/* Return a new right sibling only when this page overflows. */
/*
 * A NULL result means no new sibling was created.
 * It does not indicate whether a duplicate was encountered.
 * The changed flag separately reports successful record insertion.
 */
static Page *bp_insert(Page *p, int k, bool *changed) {
    if (p->leaf) {
        int i = 0;
        while (i < p->n && p->keys[i] < k) ++i;
        if (i < p->n && p->keys[i] == k) return NULL;
        for (int j = p->n; j > i; --j) p->keys[j] = p->keys[j - 1];
        p->keys[i] = k; ++p->n; *changed = true;
    } else {
        int i = route(p, k);
        Page *q = bp_insert(p->child[i], k, changed);
        if (q) {
            /* Child split creates exactly one new routing entry in the parent. */
            for (int j = p->n; j > i + 1; --j) p->child[j] = p->child[j - 1];
            p->child[i + 1] = q; ++p->n;
        }
    }
    refresh(p);
    if (p->n <= (p->leaf ? ORDER - 1 : ORDER)) return NULL;
    Page *q = new_page(p->leaf);
    int cut = p->n / 2;
    q->n = p->n - cut;
    /* Leaves split records; internal pages split child pointers, not records. */
    for (int i = 0; i < q->n; ++i) {
        if (p->leaf) q->keys[i] = p->keys[cut + i];
        else q->child[i] = p->child[cut + i];
    }
    p->n = cut;
    if (p->leaf) { q->next = p->next; p->next = q; }
    refresh(p); refresh(q);
    return q;
}
/* Borrow one item from a sibling, or merge if neither sibling can spare one. */
/*
 * Internal pages transfer child pointers instead of record keys.
 * Merging removes one child entry and can underfill the parent.
 * Leaf merging reconnects next links before freeing the right page.
 */
static void repair_child(Page *p, int i) {
    Page *q = p->child[i];
    int min = q->leaf ? LEAF_MIN : INNER_MIN;
    if (q->n >= min) return;
    if (i > 0 && p->child[i - 1]->n > min) {
        Page *l = p->child[i - 1];
        /* Shift right to accept the predecessor sibling's largest item. */
        for (int j = q->n; j > 0; --j) {
            if (q->leaf) q->keys[j] = q->keys[j - 1];
            else q->child[j] = q->child[j - 1];
        }
        if (q->leaf) q->keys[0] = l->keys[l->n - 1];
        else q->child[0] = l->child[l->n - 1];
        --l->n; ++q->n; refresh(l); refresh(q);
    } else if (i + 1 < p->n && p->child[i + 1]->n > min) {
        Page *r = p->child[i + 1];
        /* Append the successor sibling's smallest item and close its gap. */
        if (q->leaf) q->keys[q->n] = r->keys[0];
        else q->child[q->n] = r->child[0];
        ++q->n; --r->n;
        for (int j = 0; j < r->n; ++j) {
            if (r->leaf) r->keys[j] = r->keys[j + 1];
            else r->child[j] = r->child[j + 1];
        }
        refresh(q); refresh(r);
    } else {
        /* Always merge right into left, preserving the leaf-chain direction. */
        int left = i > 0 ? i - 1 : i;
        Page *l = p->child[left], *r = p->child[left + 1];
        for (int j = 0; j < r->n; ++j) {
            if (l->leaf) l->keys[l->n + j] = r->keys[j];
            else l->child[l->n + j] = r->child[j];
        }
        l->n += r->n;
        if (l->leaf) l->next = r->next;
        free(r); refresh(l);
        --p->n;
        for (int j = left + 1; j < p->n; ++j) p->child[j] = p->child[j + 1];
    }
    refresh(p);
}
/* Underflow propagates upward at most one page per level. */
/*
 * An absent record returns false and leaves structure unchanged.
 * Only one underflow path can propagate toward the root.
 * At fixed order, the update cost is logarithmic in the record count.
 */
static bool bp_delete(Page *p, int k) {
    if (p->leaf) {
        int i = 0;
        while (i < p->n && p->keys[i] < k) ++i;
        if (i == p->n || p->keys[i] != k) return false;
        --p->n;
        for (int j = i; j < p->n; ++j) p->keys[j] = p->keys[j + 1];
        refresh(p); return true;
    }
    int i = route(p, k);
    if (!bp_delete(p->child[i], k)) return false;
    repair_child(p, i); refresh(p);
    return true;
}


/* Routing copies never count as records: membership is decided at a leaf. */
static bool bplus_contains(const Tree *tree, int key) {
    Page *page = tree->page;
    while (page && !page->leaf) page = page->child[route(page, key)];
    if (!page) return false;
    for (int i = 0; i < page->n; ++i) {
        if (page->keys[i] == key) return true;
    }
    return false;
}

/* This wrapper owns root creation; bp_insert owns recursive page splitting. */
static bool bplus_insert(Tree *tree, int key) {
    bool changed = false;
    if (!tree->page) tree->page = new_page(true);
    Page *sibling = bp_insert(tree->page, key, &changed);
    if (sibling) {
        /* Only splitting the old root increases the tree height. */
        Page *root = new_page(false);
        root->n = 2;
        root->child[0] = tree->page;
        root->child[1] = sibling;
        refresh(root);
        tree->page = root;
    }
    return changed;
}

/* Root occupancy has special rules that do not belong in recursive repair. */
static bool bplus_delete(Tree *tree, int key) {
    if (!tree->page) return false;
    bool changed = bp_delete(tree->page, key);
    if (!tree->page->leaf && tree->page->n == 1) {
        Page *old_root = tree->page;
        tree->page = old_root->child[0];
        free(old_root); /* The sole child replaces an unnecessary root level. */
    }
    if (tree->page->leaf && tree->page->n == 0) {
        free(tree->page);
        tree->page = NULL; /* Empty trees use the same state as new trees. */
    }
    return changed;
}

/* ==================== 8. Public API and operation dispatch ==================== */
/* This layer owns kind selection and distinct-record counting. */

/*
 * An out-of-range kind returns NULL rather than creating invalid state.
 * Allocation failure follows the common fatal-allocation policy.
 * The returned object belongs to the caller until tree_destroy.
 */
Tree *tree_create(TreeKind kind) {
    if (kind < TREE_BST || kind >= TREE_COUNT) return NULL;
    Tree *t = allocate(sizeof(*t)); t->kind = kind; return t;
}
/*
 * Names are used verbatim as identifiers in the benchmark CSV.
 * The result points to static read-only storage and must not be freed.
 * Changing these names also requires updating the plotting script.
 */
const char *tree_name(TreeKind kind) {
    static const char *const names[] = {"BST", "AVL", "Splay", "RedBlack", "BPlus"};
    return kind >= TREE_BST && kind < TREE_COUNT ? names[kind] : "Unknown";
}
/*
 * The counter excludes B+ internal separator copies.
 */
size_t tree_size(const Tree *t) { return t->size; }
/* Public operations dispatch by kind; balancing stays in the modules above. */
/* Querying a splay tree may rearrange its nodes without changing membership. */
bool tree_contains(Tree *tree, int key) {
    switch (tree->kind) {
        case TREE_BPLUS:
            return bplus_contains(tree, key);
        case TREE_SPLAY:
            tree->root = splay(tree->root, key);
            return binary_contains(tree->root, key);
        default:
            return binary_contains(tree->root, key);
    }
}

/* All implementations report success; only this layer increments size. */
bool tree_insert(Tree *tree, int key) {
    bool changed = false;
    switch (tree->kind) {
        case TREE_BST:
            changed = bst_insert(tree, key);
            break;
        case TREE_AVL:
            tree->root = avl_insert(tree->root, key, &changed);
            break;
        case TREE_SPLAY:
            changed = splay_insert(tree, key);
            break;
        case TREE_RB:
            tree->root = rb_insert(tree->root, key, &changed);
            tree->root->red = false; /* The external root is always black. */
            break;
        case TREE_BPLUS:
            changed = bplus_insert(tree, key);
            break;
        default:
            return false; /* Valid handles never carry an unknown kind. */
    }
    if (changed) ++tree->size;
    return changed;
}

/* Keeping the size update here prevents double counting successor removal. */
bool tree_delete(Tree *tree, int key) {
    bool changed = false;
    switch (tree->kind) {
        case TREE_BST:
            changed = bst_delete(tree, key);
            break;
        case TREE_AVL:
            tree->root = avl_delete(tree->root, key, &changed);
            break;
        case TREE_SPLAY:
            changed = splay_delete(tree, key);
            break;
        case TREE_RB:
            changed = rb_remove_key(tree, key);
            break;
        case TREE_BPLUS:
            changed = bplus_delete(tree, key);
            break;
        default:
            return false;
    }
    if (changed) --tree->size;
    return changed;
}

/* ==================== 9. Structural validation ==================== */
/* Independent checks are used outside benchmark timed regions. */

/* Balanced-tree validation returns computed height and black height. */
typedef struct { bool ok; int height, black; size_t count; } Check;
/*
 * Exclusive int64 bounds express strict order for every int key.
 * Null links contribute one black level by a consistent convention.
 * This diagnostic traverses all nodes and is excluded from timing.
 */
static Check check_balanced(Node *p, int64_t lo, int64_t hi, bool rb) {
    if (!p) return (Check){true, 0, 1, 0};
    if (p->key <= lo || p->key >= hi) return (Check){false, 0, 0, 0};
    Check l = check_balanced(p->left, lo, p->key, rb);
    Check r = check_balanced(p->right, p->key, hi, rb);
    int h = 1 + (l.height > r.height ? l.height : r.height);
    bool ok = l.ok && r.ok;
    /* LLRB additionally forbids right-leaning red links. */
    if (rb) ok = ok && l.black == r.black && !red(p->right)
        && !(p->red && (red(p->left) || red(p->right)));
    else ok = ok && p->height == h && abs(l.height - r.height) <= 1;
    return (Check){ok, h, l.black + !p->red, 1 + l.count + r.count};
}
/* Bounds wider than int admit both extreme keys without arithmetic overflow. */
typedef struct { Node *node; int64_t lo, hi; } Frame;
/*
 * Each explicit frame carries a subtree and its allowed key interval.
 * Push bounds and visited count detect excess reachable nodes.
 * Strict ancestor bounds reject misplaced descendants and duplicate keys.
 */
static bool check_binary(const Tree *t) {
    if (!t->root) return t->size == 0;
    if (t->size == 0 || t->size > SIZE_MAX / sizeof(Frame)) return false;
    Frame *stack = allocate(t->size * sizeof(*stack));
    size_t top = 0, count = 0;
    stack[top++] = (Frame){t->root, INT64_MIN, INT64_MAX};
    bool ok = true;
    /* An explicit stack also validates a 100000-node unbalanced chain safely. */
    while (top && ok) {
        Frame f = stack[--top]; Node *p = f.node;
        if (++count > t->size || p->key <= f.lo || p->key >= f.hi) { ok = false; break; }
        if (p->left) {
            if (top == t->size) { ok = false; break; }
            stack[top++] = (Frame){p->left, f.lo, p->key};
        }
        if (p->right) {
            if (top == t->size) { ok = false; break; }
            stack[top++] = (Frame){p->right, p->key, f.hi};
        }
    }
    free(stack); return ok && count == t->size;
}
/* B+ validation checks occupancy, equal leaf depth, bounds, and leaf links. */
/*
 * Root occupancy differs from occupancy of ordinary child pages.
 * Half-open intervals match the equality-to-right routing convention.
 * The previous leaf pointer checks links against structural traversal.
 */
static bool check_page(Page *p, bool root, int depth, int *leaf_depth,
                       int64_t lo, int64_t hi, Page **previous, size_t *count) {
    if (!p || depth > 64) return false;
    int min = root ? (p->leaf ? 1 : 2) : (p->leaf ? LEAF_MIN : INNER_MIN);
    if (p->n < min || p->n > (p->leaf ? ORDER - 1 : ORDER)) return false;
    if (p->leaf) {
        if (*leaf_depth == -1) *leaf_depth = depth;
        if (*leaf_depth != depth || p->minimum != p->keys[0]) return false;
        if (*previous && (*previous)->next != p) return false;
        for (int i = 0; i < p->n; ++i) {
            if (p->keys[i] < lo || p->keys[i] >= hi) return false;
            if (i && p->keys[i - 1] >= p->keys[i]) return false;
        }
        *previous = p; *count += (size_t)p->n; return true;
    }
    /* Each separator must exactly match the right child's cached minimum. */
    if (!p->child[0] || p->minimum != p->child[0]->minimum) return false;
    for (int i = 0; i < p->n; ++i) {
        if (!p->child[i]) return false;
        if (i && (p->keys[i - 1] != p->child[i]->minimum || p->keys[i - 1] <= lo)) return false;
        if (i > 1 && p->keys[i - 2] >= p->keys[i - 1]) return false;
        if (!check_page(p->child[i], false, depth + 1, leaf_depth,
            i ? p->keys[i - 1] : lo, i + 1 < p->n ? p->keys[i] : hi, previous, count)) return false;
    }
    return true;
}
/*
 * Check implementation-specific invariants without changing the tree.
 * The function is a diagnostic for valid allocated tree objects.
 * It is not a memory-safety sandbox for arbitrary corrupted pointers.
 */
bool tree_validate(const Tree *t) {
    if (t->kind == TREE_BPLUS) {
        if (!t->page) return t->size == 0;
        int depth = -1; size_t count = 0; Page *previous = NULL;
        return check_page(t->page, true, 0, &depth, INT64_MIN, INT64_MAX, &previous, &count)
            && previous && !previous->next && count == t->size;
    }
    if (t->kind == TREE_AVL || t->kind == TREE_RB) {
        bool rb = t->kind == TREE_RB;
        Check c = check_balanced(t->root, INT64_MIN, INT64_MAX, rb);
        return c.ok && c.count == t->size && (!rb || !red(t->root));
    }
    return check_binary(t);
}

/* ==================== 10. Memory cleanup ==================== */
/* Binary cleanup is iterative; B+ cleanup follows only owned child links. */

/* B+ depth is logarithmic; page destruction does not follow the leaf chain. */
/*
 * Only internal child pointers define ownership of pages.
 * Leaf next pointers are traversal links and must not be freed through.
 * B+ balancing keeps recursive depth logarithmic.
 */
static void free_page(Page *p) {
    if (!p) return;
    if (!p->leaf) for (int i = 0; i < p->n; ++i) free_page(p->child[i]);
    free(p);
}
/*
 * NULL is explicitly accepted by this public cleanup function.
 * Binary storage uses rotations to eliminate left links iteratively.
 * This avoids stack overflow on a long BST or splay chain.
 */
void tree_destroy(Tree *t) {
    if (!t) return;
    free_page(t->page);
    /* Rotate left subtrees upward until nodes can be freed without recursion. */
    Node *p = t->root;
    while (p) {
        if (p->left) {
            Node *q = p->left; p->left = q->right; q->right = p; p = q;
        } else { Node *q = p->right; free(p); p = q; }
    }
    free(t);
}
