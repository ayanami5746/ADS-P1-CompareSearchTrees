#include "trees.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Binary nodes share storage; height is used by AVL and red by red-black. */
typedef struct Node {
    int key, height;
    bool red;
    struct Node *left, *right;
} Node;
/* B+ order means maximum children; leaves hold at most ORDER-1 records. */
#define ORDER 16
#define LEAF_MIN (ORDER / 2)
#define INNER_MIN (ORDER / 2)
/* One extra slot lets insertion temporarily overflow before splitting. */
typedef struct Page {
    bool leaf;
    int n, minimum;
    int keys[ORDER];
    struct Page *child[ORDER + 1], *next;
} Page;
/* For an internal page, n counts children and keys[i] = child[i+1].minimum. */
struct Tree {
    TreeKind kind;
    size_t size;
    Node *root;
    Page *page;
};

/* Centralized checked allocation avoids returning partially updated trees. */
/*
 * Allocate zero-initialized storage owned by the caller.
 * Zero initialization establishes null links and empty counters.
 * The byte count must already include any array multiplication checks.
 * Failure is fatal because mutations do not implement rollback.
 * A successful result is never NULL and can be passed to free.
 * This helper performs no tree-specific initialization.
 * Allocation cost remains part of the measured insert operation.
 */
static void *allocate(size_t n) {
    void *p = calloc(1, n);
    if (!p) { fputs("Out of memory\n", stderr); exit(EXIT_FAILURE); }
    return p;
}
/* All fresh binary nodes are leaves; RB insertion initially colors them red. */
/*
 * Construct a detached binary-tree leaf containing the supplied key.
 * Both child pointers are initially null through zero initialization.
 * Height one is the base case required by AVL insertion.
 * Red is the initial color required by red-black insertion.
 * BST and splay trees ignore the stored color and AVL metadata.
 * The caller must attach the result exactly once to avoid leaks.
 * No key value is reserved as an empty-node sentinel.
 */
static Node *new_node(int key) {
    Node *p = allocate(sizeof(*p));
    p->key = key; p->height = 1; p->red = true;
    return p;
}
/* Null children have height zero in the AVL height convention. */
/*
 * Read the cached AVL height without dereferencing a null child.
 * An empty subtree contributes zero to its parent's height.
 * Only AVL balancing relies on the correctness of this metadata.
 * The helper takes constant time and does not modify the node.
 */
static int height(Node *p) { return p ? p->height : 0; }
/*
 * Recompute one node's height from its immediate children.
 * The node argument must be non-null.
 * In AVL use, both child heights must already be correct.
 * The result counts nodes rather than edges on the longest path.
 * This function does not rotate or recolor the tree.
 * Rotations call it bottom-up so dependent heights are available.
 * Other binary-tree variants do not consult the resulting height.
 */
static void fix_height(Node *p) {
    int l = height(p->left), r = height(p->right);
    p->height = 1 + (l > r ? l : r);
}
/* Rotations preserve in-order traversal and repair both affected heights. */
/*
 * Promote the right child and return the new subtree root.
 * The old root and its right child must both exist.
 * The promoted child's left subtree becomes the old root's right child.
 * All keys retain their relative in-order positions.
 * The caller reconnects the returned pointer to the surrounding tree.
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
 * Promote the left child and return the new subtree root.
 * The old root and its left child must both exist.
 * The promoted child's right subtree becomes the old root's left child.
 * No allocation or deallocation occurs during the rotation.
 * The caller must preserve the returned root pointer.
 * Cached heights are repaired in dependency order.
 * This operation is the mirror image of rotate_left.
 */
static Node *rotate_right(Node *p) {
    Node *q = p->left;
    p->left = q->right; q->right = p;
    fix_height(p); fix_height(q);
    return q;
}
/* AVL repairs the first unbalanced direction, including a double rotation. */
/*
 * Restore the AVL invariant at a single changed subtree root.
 * Both children must already satisfy the AVL invariant.
 * Insertion or deletion can create a height difference of two.
 * An inward-heavy child requires a preliminary child rotation.
 * An outward-heavy or equally balanced child needs only one rotation.
 * Returning NULL for an empty subtree simplifies deletion unwind.
 * The work per call is constant, giving logarithmic AVL updates.
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
 * Insert a distinct key while unwinding a balanced search path.
 * The changed flag is initially false and is set only at allocation.
 * Equality leaves the existing record and total size untouched.
 * Recursive results replace the corresponding child links.
 * Each ancestor recomputes its height and repairs any imbalance.
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
 * Remove a key and restore AVL heights along the search path.
 * An absent key reaches NULL without setting the changed flag.
 * A node with at most one child can be replaced directly.
 * A two-child node copies the smallest key in its right subtree.
 * Removing that successor physically frees exactly one node.
 * The shared changed flag must not be counted twice by the caller.
 * Balancing on every return also handles deletion height decreases.
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

/* Top-down splaying uses two temporary chains and constant auxiliary space. */
/*
 * Move a matching key, or the last accessed boundary, to the root.
 * The temporary dummy node anchors the smaller and larger chains.
 * Chain links are repaired before the stack-local dummy goes away.
 * Repeated same-direction descent uses a zig-zig rotation.
 * No parent pointers, recursive calls, or auxiliary arrays are needed.
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

/* Left-leaning red-black trees are a standard red-black-tree variant. */
/*
 * Treat null child links as black external leaves.
 * Only the red-black implementation interprets the color field.
 * Testing a link color never changes the represented set.
 * This convention avoids allocating shared sentinel nodes.
 */
static bool red(Node *p) { return p && p->red; }
/* RB rotations transfer the former root color and mark the lowered node red. */
/*
 * Perform a left rotation with red-black color transfer.
 * The caller guarantees that the right child exists.
 * The promoted node inherits the old root's incoming-link color.
 * The lowered root becomes connected by a red link.
 * This preserves the subtree's contribution to black height.
 * Further color flips may still be required by the caller.
 * The returned node replaces the former subtree root.
 */
static Node *rb_left(Node *p) {
    Node *q = rotate_left(p); q->red = p->red; p->red = true; return q;
}
/*
 * Perform the symmetric red-black right rotation.
 * The caller guarantees that the left child exists.
 * The old root color moves to the promoted node.
 * The lowered node receives red, preserving black-height accounting.
 * Deletion can use this to move a red link onto its search path.
 * Key order remains the responsibility of the shared rotation.
 * The surrounding parent must install the returned root.
 */
static Node *rb_right(Node *p) {
    Node *q = rotate_right(p); q->red = p->red; p->red = true; return q;
}
/* A color flip splits a temporary four-node or combines two two-nodes. */
/*
 * Toggle the parent and both child link colors together.
 * The parent must exist; null children remain conceptual black leaves.
 * On valid repair paths the relevant child nodes are present.
 * Insertion uses this to split a temporary four-node.
 * Deletion uses the reverse transformation to combine two-nodes.
 * This operation never exchanges keys or child pointers.
 * Root normalization happens only after the whole public operation.
 */
static void flip(Node *p) {
    p->red = !p->red;
    if (p->left) p->left->red = !p->left->red;
    if (p->right) p->right->red = !p->right->red;
}
/*
 * Normalize a subtree after insertion or deletion recursion.
 * A red right link is first rotated to the left.
 * Two consecutive left red links are then rotated right.
 * Two red children finally trigger a color split.
 * The order of these repairs is significant for left leaning.
 * The subtree root may remain red for its parent to handle.
 * The public operation forces the overall root to black.
 */
static Node *rb_fix(Node *p) {
    if (red(p->right)) p = rb_left(p);
    if (red(p->left) && red(p->left->left)) p = rb_right(p);
    if (red(p->left) && red(p->right)) flip(p);
    return p;
}
/* Insert as a red leaf, then restore left leaning and eliminate red-red edges. */
/*
 * Recursively add a red leaf following strict binary search order.
 * The changed flag distinguishes allocation from duplicate insertion.
 * Only one branch is visited at each level.
 * Ancestor repair removes right-red and consecutive-red configurations.
 * Existing keys retain their values and node ownership.
 * The resulting root color is normalized by tree_insert.
 * Red-black height bounds recursion and runtime by O(log N).
 */
static Node *rb_insert(Node *p, int k, bool *changed) {
    if (!p) { *changed = true; return new_node(k); }
    if (k < p->key) p->left = rb_insert(p->left, k, changed);
    else if (k > p->key) p->right = rb_insert(p->right, k, changed);
    return rb_fix(p);
}
/* Move a red link left so deletion never descends into a black two-node. */
/*
 * Prepare a left child for top-down red-black deletion.
 * The descent target is black and its left child is also black.
 * A color flip temporarily merges the neighboring two-nodes.
 * If the sibling can supply red, rotations transfer that red link.
 * A second flip restores the local representation after transfer.
 * The returned root may have changed during these rotations.
 * The caller continues deletion through the returned left child.
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
 * Prepare a right child for top-down red-black deletion.
 * The target lacks a red link available for immediate removal.
 * Color flipping first combines the local two-node representation.
 * A red left-left grandchild allows borrowing from the opposite side.
 * Right rotation and recoloring complete that transfer.
 * Keys are not inserted or removed by this preparatory step.
 * The caller must use the returned root for further comparisons.
 */
static Node *move_right(Node *p) {
    flip(p);
    if (p->left && red(p->left->left)) { p = rb_right(p); flip(p); }
    return p;
}
/*
 * Remove the smallest node from a nonempty LLRB subtree.
 * A minimum with no left child has no surviving right child here.
 * The function therefore frees it and returns an empty subtree.
 * Before descent, a red link is moved into a black two-node.
 * Unwinding restores the left-leaning representation.
 * This helper supports successor replacement in general deletion.
 * The public size counter is updated only by tree_delete.
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
 * The public membership check makes child dereferences on the path valid.
 * Red links are moved downward before entering black two-nodes.
 * Rotations can change the root key, so equality is tested again.
 * A two-child match is replaced by its in-order successor.
 * Successor removal and ancestor repair preserve black height.
 * The caller normalizes the final root color and decrements size once.
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

/* B+ records exist only in leaves; internal keys are routing copies. */
/*
 * Create an empty B+ leaf or internal page.
 * Zero initialization clears child pointers and the leaf-chain link.
 * The caller fills records or children before exposing the page.
 * For leaves, n counts keys; for internal pages, n counts children.
 * An empty page's minimum is not meaningful yet.
 * Insertion refreshes metadata after filling the first item.
 * Page storage includes one temporary overflow slot.
 */
static Page *new_page(bool leaf) {
    Page *p = allocate(sizeof(*p)); p->leaf = leaf; return p;
}
/* Cached minima make separator repair proportional to order, not tree height. */
/*
 * Rebuild routing metadata from a page's current contents.
 * A nonempty leaf caches its first record as its minimum.
 * An empty leaf keeps an unused minimum until removal or refilling.
 * Internal pages must have at least one initialized child.
 * Each internal separator copies the following child's minimum.
 * Children must be refreshed before their parent is refreshed.
 * At fixed order the work is constant per visited level.
 */
static void refresh(Page *p) {
    if (p->leaf) { if (p->n) p->minimum = p->keys[0]; return; }
    p->minimum = p->child[0]->minimum;
    for (int i = 1; i < p->n; ++i) p->keys[i - 1] = p->child[i]->minimum;
}
/* Equality follows the right child because separators copy its minimum. */
/*
 * Select an internal child using the copied separator keys.
 * The page must be internal and have at least one child.
 * A key below every separator follows child zero.
 * Equality belongs to the child on the separator's right.
 * A key beyond the last separator follows the last child.
 * The result is always an index in the current child array.
 * Linear scanning is bounded by the fixed B+ order.
 */
static int route(Page *p, int k) {
    int i = 0;
    while (i + 1 < p->n && k >= p->keys[i]) ++i;
    return i;
}
/* Return a new right sibling only when this page overflows. */
/*
 * Insert into a B+ subtree and propagate a possible split upward.
 * A NULL result means no new sibling was created.
 * It does not indicate whether a duplicate was encountered.
 * The changed flag separately reports successful record insertion.
 * Only leaves store actual records; parents store routing copies.
 * A split divides an overflow into two legal occupancy ranges.
 * The public caller creates a higher root if the old root splits.
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
 * Repair one underfull B+ child after a successful deletion.
 * The parent has a neighboring child available on a valid repair path.
 * The left sibling is preferred when it can spare an item.
 * Otherwise the right sibling is tried before performing a merge.
 * Internal pages transfer child pointers instead of record keys.
 * Merging removes one child entry and can underfill the parent.
 * Leaf merging reconnects next links before freeing the right page.
 * Refreshed child minima are propagated into the parent's separators.
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
 * Remove a record from a B+ subtree without rebuilding the tree.
 * An absent record returns false and leaves structure unchanged.
 * Leaf deletion closes the key-array gap and updates the minimum.
 * Each successful recursive return repairs its affected child.
 * Only one underflow path can propagate toward the root.
 * The root's special occupancy exceptions are handled by tree_delete.
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

/* A failed lookup in a binary tree needs no allocation or sentinel key. */
/*
 * Perform an ordinary iterative binary search for an exact key.
 * The helper does not splay, rotate, recolor, or allocate.
 * It is also used to establish RB deletion's membership precondition.
 * An empty subtree immediately reports absence.
 * Equality stops the search before following another link.
 * The work is proportional to the actual search-path height.
 * No assumptions about key spacing or key sign are needed.
 */
static bool binary_contains(Node *p, int k) {
    while (p && p->key != k) p = k < p->key ? p->left : p->right;
    return p != NULL;
}
/*
 * Create an empty set of the requested implementation kind.
 * An out-of-range kind returns NULL rather than creating invalid state.
 * Valid trees start with zero size and both root pointers null.
 * Only the root corresponding to the selected kind is later used.
 * Allocation failure follows the common fatal-allocation policy.
 * The returned object belongs to the caller until tree_destroy.
 * Creation itself is kept outside the benchmark timing intervals.
 */
Tree *tree_create(TreeKind kind) {
    if (kind < TREE_BST || kind >= TREE_COUNT) return NULL;
    Tree *t = allocate(sizeof(*t)); t->kind = kind; return t;
}
/*
 * Return a stable display name for a tree implementation.
 * Names are used verbatim as identifiers in the benchmark CSV.
 * The result points to static read-only storage and must not be freed.
 * Unknown enum values return a diagnostic name safely.
 * Name lookup never reads or changes a tree object.
 * Changing these names also requires updating the plotting script.
 * The order matches the TreeKind enumeration in the public header.
 */
const char *tree_name(TreeKind kind) {
    static const char *const names[] = {"BST", "AVL", "Splay", "RedBlack", "BPlus"};
    return kind >= TREE_BST && kind < TREE_COUNT ? names[kind] : "Unknown";
}
/*
 * Return the number of distinct records in a valid tree object.
 * The counter excludes B+ internal separator copies.
 * Duplicate insertion and absent deletion leave it unchanged.
 * Reading this maintained counter takes constant time.
 */
size_t tree_size(const Tree *t) { return t->size; }
/*
 * Check membership using the selected implementation's search rules.
 * The tree pointer must refer to a live object from tree_create.
 * B+ lookup follows internal separators then examines one leaf.
 * Binary implementations follow strict less-than/greater-than order.
 * Splay lookup intentionally changes the shape on success or failure.
 * Membership lookup never changes the set size.
 * The return value is true exactly when the record exists.
 */
bool tree_contains(Tree *t, int k) {
    if (t->kind == TREE_BPLUS) {
        Page *p = t->page;
        while (p && !p->leaf) p = p->child[route(p, k)];
        if (!p) return false;
        for (int i = 0; i < p->n; ++i) if (p->keys[i] == k) return true;
        return false;
    }
    /* Access itself is part of the splay-tree algorithm. */
    if (t->kind == TREE_SPLAY) t->root = splay(t->root, k);
    return binary_contains(t->root, k);
}
/*
 * Insert one key and report whether the distinct-key set grew.
 * The maintained size increments only after a successful insertion.
 * Each implementation owns all nodes or pages it allocates.
 * BST follows the entire search path even for monotonic inputs.
 * Splay insertion makes the new key the root of the partitioned tree.
 * AVL and RB install the possibly rotated root returned by recursion.
 * B+ insertion handles a root split as the only height-increase case.
 */
bool tree_insert(Tree *t, int k) {
    bool changed = false;
    if (t->kind == TREE_BST) {
        Node **link = &t->root;
        /* Pointer-to-pointer traversal handles the root without a special case. */
        while (*link && (*link)->key != k)
            link = k < (*link)->key ? &(*link)->left : &(*link)->right;
        if (!*link) { *link = new_node(k); changed = true; }
    } else if (t->kind == TREE_AVL) t->root = avl_insert(t->root, k, &changed);
    else if (t->kind == TREE_RB) {
        t->root = rb_insert(t->root, k, &changed); t->root->red = false;
    } else if (t->kind == TREE_SPLAY) {
        t->root = splay(t->root, k);
        if (!t->root || t->root->key != k) {
            Node *q = new_node(k);
            /* Splaying partitions the old tree around the new key. */
            if (t->root && k < t->root->key) {
                q->left = t->root->left; q->right = t->root; t->root->left = NULL;
            } else if (t->root) {
                q->right = t->root->right; q->left = t->root; t->root->right = NULL;
            }
            t->root = q; changed = true;
        }
    } else {
        if (!t->page) t->page = new_page(true);
        Page *q = bp_insert(t->page, k, &changed);
        if (q) {
            /* Only a root split increases B+ height. */
            Page *p = new_page(false); p->n = 2;
            p->child[0] = t->page; p->child[1] = q; refresh(p); t->page = p;
        }
    }
    if (changed) ++t->size;
    return changed;
}
/*
 * Delete one key and report whether the distinct-key set shrank.
 * An absent key must never decrement the maintained size.
 * BST physically removes a node after any successor-key replacement.
 * AVL handles successor replacement within its recursive helper.
 * RB checks membership before entering its top-down delete routine.
 * Splay deletion joins the surviving left and right partitions.
 * B+ deletion collapses a single-child root and removes an empty leaf.
 * No implementation rebuilds the whole tree to perform deletion.
 */
bool tree_delete(Tree *t, int k) {
    bool changed = false;
    if (t->kind == TREE_BST) {
        Node **link = &t->root;
        while (*link && (*link)->key != k)
            link = k < (*link)->key ? &(*link)->left : &(*link)->right;
        if (*link) {
            Node *p = *link;
            /* Two-child removal splices the successor out of its original slot. */
            if (p->left && p->right) {
                Node **next = &p->right;
                while ((*next)->left) next = &(*next)->left;
                p->key = (*next)->key; link = next; p = *link;
            }
            *link = p->left ? p->left : p->right; free(p); changed = true;
        }
    } else if (t->kind == TREE_AVL) t->root = avl_delete(t->root, k, &changed);
    else if (t->kind == TREE_RB) {
        if (binary_contains(t->root, k)) {
            /* A temporarily red root allows top-down color redistribution. */
            if (!red(t->root->left) && !red(t->root->right)) t->root->red = true;
            t->root = rb_delete(t->root, k);
            if (t->root) t->root->red = false;
            changed = true;
        }
    } else if (t->kind == TREE_SPLAY) {
        t->root = splay(t->root, k);
        if (t->root && t->root->key == k) {
            Node *p = t->root;
            if (!p->left) t->root = p->right;
            else {
                /* Splaying an upper bound exposes the left subtree's maximum. */
                t->root = splay(p->left, k); t->root->right = p->right;
            }
            free(p); changed = true;
        }
    } else if (t->page) {
        changed = bp_delete(t->page, k);
        if (!t->page->leaf && t->page->n == 1) {
            Page *p = t->page; t->page = p->child[0]; free(p);
        }
        /* Keep the empty representation identical to a newly created tree. */
        if (t->page->leaf && t->page->n == 0) { free(t->page); t->page = NULL; }
    }
    if (changed) --t->size;
    return changed;
}

/* Balanced-tree validation returns computed height and black height. */
typedef struct { bool ok; int height, black; size_t count; } Check;
/*
 * Independently recompute structural metadata for AVL or RB validation.
 * Exclusive int64 bounds express strict order for every int key.
 * The result accumulates actual node count and computed height.
 * Null links contribute one black level by a consistent convention.
 * AVL stored heights are compared with independently computed heights.
 * RB checks equal black heights and the absence of red-red edges.
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
 * Validate unbalanced binary trees without recursive traversal.
 * Each explicit frame carries a subtree and its allowed key interval.
 * The array capacity is bounded by the maintained record count.
 * Push bounds and visited count detect excess reachable nodes.
 * Strict ancestor bounds reject misplaced descendants and duplicate keys.
 * The final count must match the public size counter exactly.
 * All temporary validation storage is freed before returning.
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
 * Recursively audit the B+ page hierarchy and ordered leaf chain.
 * Root occupancy differs from occupancy of ordinary child pages.
 * Every leaf must occur at the same depth in the hierarchy.
 * Half-open intervals match the equality-to-right routing convention.
 * The previous leaf pointer checks links against structural traversal.
 * Only leaf records contribute to the accumulated element count.
 * The public validator additionally requires the last link to be null.
 * A conservative depth guard rejects malformed excessively deep pages.
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
 * An empty root must agree with a zero element count.
 * Binary trees are checked against strict global search ordering.
 * Balanced variants additionally verify their balancing metadata.
 * B+ validation checks separators, occupancy, depth, and leaf links.
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
/* B+ depth is logarithmic; page destruction does not follow the leaf chain. */
/*
 * Release a B+ hierarchy in child-before-parent order.
 * Only internal child pointers define ownership of pages.
 * Leaf next pointers are traversal links and must not be freed through.
 * Following both kinds of link would double-free leaf storage.
 * NULL is accepted to simplify empty-tree destruction.
 * B+ balancing keeps recursive depth logarithmic.
 * No temporary allocation is needed for cleanup.
 */
static void free_page(Page *p) {
    if (!p) return;
    if (!p->leaf) for (int i = 0; i < p->n; ++i) free_page(p->child[i]);
    free(p);
}
/*
 * Release every node or page and finally the tree object.
 * NULL is explicitly accepted by this public cleanup function.
 * B+ storage uses its balanced page-hierarchy traversal.
 * Binary storage uses rotations to eliminate left links iteratively.
 * This avoids stack overflow on a long BST or splay chain.
 * Every binary node is freed after its remaining right link is saved.
 * The caller must not use the tree pointer after destruction.
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
