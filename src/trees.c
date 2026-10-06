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
    bool red;            /* Node color used only by the red-black tree. */
    struct Node *left;   /* All descendant keys are smaller. */
    struct Node *right;  /* All descendant keys are larger. */
    struct Node *parent; /* Used only by bottom-up Splay and red-black repair. */
} Node;
/* B+ order means maximum children; leaves hold at most ORDER records. */
#define ORDER 4
#define LEAF_MIN (ORDER / 2)
#define INNER_MIN (ORDER / 2)
/* One extra slot lets insertion temporarily overflow before splitting. */
typedef struct Page {
    bool leaf;           /* Distinguish record pages from routing pages. */
    int n;               /* Leaf: record count. Internal: child count. */
    int minimum;         /* Smallest record reachable from this page. */
    int keys[ORDER + 1];
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
 * Height zero is the base case required by AVL insertion.
 * Red is the initial color required by red-black insertion.
 * No key value is reserved as an empty-node sentinel.
 */
static Node *new_node(int key) {
    Node *p = allocate(sizeof(*p));
    p->key = key; p->height = 0; p->red = true;
    return p;
}
/* Null children have height minus one in the AVL height convention. */
/*
 * An empty subtree contributes minus one to its parent's height.
 */
static int height(Node *p) { return p ? p->height : -1; }
/*
 * In AVL use, both child heights must already be correct.
 * The result counts edges rather than nodes on the longest path.
 * Rotations call it bottom-up so dependent heights are available.
 */
static void fix_height(Node *p) {
    int l = height(p->left), r = height(p->right);
    p->height = 1 + (l > r ? l : r);
}
/* Plain rotations change child links only; AVL owns cached-height updates. */
static Node *rotate_left(Node *p) {
    Node *q = p->right;
    p->right = q->left; q->left = p;
    return q;
}
static Node *rotate_right(Node *p) {
    Node *q = p->left;
    p->left = q->right; q->right = p;
    return q;
}
/* The lowered AVL node must be refreshed before the promoted node. */
static Node *avl_left(Node *p) {
    Node *q = rotate_left(p);
    fix_height(p); fix_height(q);
    return q;
}
static Node *avl_right(Node *p) {
    Node *q = rotate_right(p);
    fix_height(p); fix_height(q);
    return q;
}
/* Splay and RB need parent links, but neither uses cached heights. */
/* Test counters are absent from benchmark and normal library builds. */
#ifdef TREE_TESTING
static unsigned linked_rotations;
static unsigned rb_insert_cases[2][3], rb_delete_cases[2][4];
#define RB_CASE(table, side, index) (++table[(side) ? 0 : 1][index])
#else
#define RB_CASE(table, side, index) ((void)0)
#endif
/* Reconnect the promoted subtree to its former parent or the tree root.
 * The middle subtree changes parents even though its keys do not move.
 * AVL uses its own wrappers and never calls this helper.
 */
static void rotate_linked(Tree *t, Node *p, bool left) {
    Node *parent = p->parent;
    Node *q = left ? rotate_left(p) : rotate_right(p);
    Node *middle = left ? p->right : p->left;
    if (middle) middle->parent = p; /* Repair the transferred subtree link. */
    p->parent = q; q->parent = parent; /* The promoted node inherits the old parent. */
    if (!parent) t->root = q;
    else if (parent->left == p) parent->left = q;
    else parent->right = q;
#ifdef TREE_TESTING
    ++linked_rotations;
#endif
}
/* A failed lookup in a binary tree needs no allocation or sentinel key. */
/*
 * The helper does not splay, rotate, recolor, or allocate.
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
            p->left = avl_left(p->left);
        return avl_right(p);
    }
    if (height(p->right) - height(p->left) > 1) {
        if (height(p->right->right) < height(p->right->left))
            p->right = avl_right(p->right);
        return avl_left(p);
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
    return *changed ? balance(p) : p; /* Unchanged subtrees need no repair. */
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
    return *changed ? balance(p) : p; /* Unchanged subtrees need no repair. */
}

/* ==================== 5. Splay tree ==================== */
/* Lecture 1, slide 14: work upward from X, its parent P and grandparent G. */
/* Parent links avoid recursion even when the search path is a long chain. */
/* Each loop moves X up one or two levels without changing key order.
 * Zig-zig must rotate G first; two repeated rotations of X are different.
 * Only child and parent links change: no height or color work is needed.
 */
static void splay_node(Tree *t, Node *x) {
    while (x->parent) {
        Node *p = x->parent, *g = p->parent; /* Re-read ancestors after each rotation pair. */
        if (!g) {
            /* Zig: a single rotation when the parent is the root. */
            rotate_linked(t, p, x == p->right);
        } else if ((x == p->left) == (p == g->left)) {
            /* Zig-zig: rotate the grandparent first, then the parent. */
            bool left = x == p->right; /* Same-side rotations share a direction. */
            rotate_linked(t, g, left);
            rotate_linked(t, p, left);
        } else {
            /* Zig-zag: rotate X over P, then X over G. */
            rotate_linked(t, p, x == p->right);
            rotate_linked(t, g, x == g->right);
        }
    }
}
/* A miss splays the last visited node; it never changes membership. */
/* Save the final search node before rotating, including on a miss.
 * A successful access exposes the requested key at the root.
 * The boolean result describes membership before and after the access.
 */
static bool splay_find(Tree *t, int key) {
    Node *p = t->root, *last = NULL; /* Empty trees have no access to splay. */
    while (p) {
        last = p; /* Preserve the boundary if the next child is NULL. */
        if (key == p->key) break;
        p = key < p->key ? p->left : p->right;
    }
    if (last) splay_node(t, last);
    return p != NULL;
}
/* Insert as in a BST, then splay the inserted or already-present node. */
/* The slot is an owning link, so root insertion needs no special case.
 * Duplicates still count as accesses and are therefore splayed.
 * Public dispatch updates the record count only on a new insertion.
 */
static bool splay_insert(Tree *t, int key) {
    Node **slot = &t->root, *parent = NULL;
    while (*slot) {
        parent = *slot;
        if (key == parent->key) { splay_node(t, parent); return false; }
        slot = key < parent->key ? &parent->left : &parent->right;
    }
    Node *p = new_node(key);
    p->parent = parent; *slot = p;
    splay_node(t, p); /* The inserted leaf becomes the root. */
    return true;
}
/* Lecture 1, slide 17: expose X, remove it, splay max(TL), attach TR. */
/* Detach both parent links before treating TL and TR as separate trees.
 * With no left subtree, TR becomes the new root immediately.
 * Otherwise max(TL) has no right child, leaving a slot for all of TR.
 */
static bool splay_delete(Tree *t, int key) {
    if (!splay_find(t, key)) return false;
    Node *removed = t->root, *left = removed->left, *right = removed->right;
    if (left) left->parent = NULL;
    if (right) right->parent = NULL;
    t->root = left ? left : right;
    if (left) {
        Node *maximum = left; /* FindMax uses the detached left subtree. */
        while (maximum->right) maximum = maximum->right;
        splay_node(t, maximum);
        t->root->right = right;
        if (right) right->parent = t->root; /* Complete both directions of the join. */
    }
    free(removed);
    return true;
}

/* ==================== 6. Red-black tree ==================== */
/* Lecture 2, slides 4-6: ordinary RB repair, with symmetric cases. */
/* NULL represents a black NIL leaf; no integer key is reserved. */
static bool red(Node *p) { return p && p->red; }

/* The parent and uncle determine whether to recolor or rotate. */
/* The root starts black, so a red parent always has a grandparent.
 * Red uncles need recoloring; black uncles need at most two rotations.
 * Case 2 falls through to case 3 after straightening the inner child.
 * The left flag selects the lecture case or its mirror image.
 */
static void rb_insert_fix(Tree *t, Node *x) {
    while (red(x->parent)) {
        Node *p = x->parent, *g = p->parent;
        bool left = p == g->left; /* Select the uncle on the opposite side. */
        Node *uncle = left ? g->right : g->left;
        if (red(uncle)) {
            /* Case 1: recolor and move the red-red conflict upward. */
            RB_CASE(rb_insert_cases, left, 0);
            p->red = false; uncle->red = false; g->red = true;
            x = g; /* Only recoloring can carry the conflict upward. */
        } else {
            if (x == (left ? p->right : p->left)) {
                /* Case 2: turn an inner child into an outer child. */
                RB_CASE(rb_insert_cases, left, 1);
                x = p;
                rotate_linked(t, x, left);
                p = x->parent; /* Case 2 changed the local parent. */
            }
            /* Case 3: rotate the grandparent and exchange colors. */
            RB_CASE(rb_insert_cases, left, 2);
            p->red = false; g->red = true;
            rotate_linked(t, g, !left);
        }
    }
    t->root->red = false; /* Recoloring may have reached the root. */
}
/* BST placement gives the new red node a parent before repair starts. */
static bool rb_insert(Tree *t, int key) {
    Node **slot = &t->root, *parent = NULL;
    while (*slot) {
        parent = *slot;
        if (key == parent->key) return false;
        slot = key < parent->key ? &parent->left : &parent->right;
    }
    Node *x = new_node(key);
    x->parent = parent; *slot = x;
    rb_insert_fix(t, x); /* The new red leaf preserves black height. */
    return true;
}
/* Replace one subtree, including the root, and preserve its parent link. */
static void rb_replace(Tree *t, Node *old, Node *replacement) {
    if (!old->parent) t->root = replacement;
    else if (old == old->parent->left) old->parent->left = replacement;
    else old->parent->right = replacement;
    if (replacement) replacement->parent = old->parent;
}
/* A separate parent argument represents the parent of a NULL replacement. */
/* The missing black level is conceptual; it is not stored in Node.height. */
/* Enter only after physically removing a black node.
 * A red replacement absorbs the deficit by becoming black.
 * A deficit that reaches the root can be discarded.
 * In case 2, a red parent absorbs it on the next loop condition.
 * Cases 1 and 3 prepare case 4; only case 2 propagates upward.
 * Symmetry changes near/far nephews and the rotation direction together.
 */
static void rb_delete_fix(Tree *t, Node *x, Node *parent) {
    while (x != t->root && !red(x)) {
        bool left = x == parent->left; /* NULL is identified by its owning slot. */
        Node *w = left ? parent->right : parent->left;
        if (red(w)) {
            /* Case 1: make the sibling black and obtain a new sibling. */
            RB_CASE(rb_delete_cases, left, 0);
            w->red = false; parent->red = true;
            rotate_linked(t, parent, left);
            w = left ? parent->right : parent->left;
        }
        /* A valid RB tree has a sibling beside a black-deficient subtree. */
        Node *near = left ? w->left : w->right; /* The nephew on the deficient side. */
        Node *far = left ? w->right : w->left; /* The nephew on the opposite side. */
        if (!red(near) && !red(far)) {
            /* Case 2: pass the deficit to the parent, or absorb it there. */
            RB_CASE(rb_delete_cases, left, 1);
            w->red = true;
            x = parent; parent = x->parent;
        } else {
            if (!red(far)) {
                /* Case 3: convert a red near nephew into a red far nephew. */
                RB_CASE(rb_delete_cases, left, 2);
                near->red = false; w->red = true;
                rotate_linked(t, w, !left);
                w = left ? parent->right : parent->left;
                far = left ? w->right : w->left;
            }
            /* Case 4: a final rotation and recoloring remove the deficit. */
            RB_CASE(rb_delete_cases, left, 3);
            w->red = parent->red; parent->red = false; far->red = false;
            rotate_linked(t, parent, left);
            x = t->root; /* Case 4 ends repair without another rotation. */
        }
    }
    if (x) x->red = false; /* A red replacement absorbs one black level. */
}
/* Copy only the successor key; the destination keeps its original color. */
/* Search once; an absent key leaves all links and colors unchanged.
 * For two children, remove the successor from the right subtree.
 * Its original color determines whether the removal needs repair.
 * Keep the parent separately because the replacement can be NULL.
 */
static bool rb_remove_key(Tree *t, int key) {
    Node *z = t->root;
    while (z && z->key != key) z = key < z->key ? z->left : z->right;
    if (!z) return false;
    if (z->left && z->right) {
        Node *successor = z->right;
        while (successor->left) successor = successor->left;
        z->key = successor->key;
        z = successor; /* Repair follows the physically removed node. */
    }
    /* The physically removed node now has at most one non-NIL child. */
    Node *x = z->left ? z->left : z->right, *parent = z->parent;
    bool was_red = z->red; /* Save the color before freeing the node. */
    rb_replace(t, z, x);
    free(z);
    if (!was_red) rb_delete_fix(t, x, parent); /* Removing red needs no repair. */
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
    if (p->n <= ORDER) return NULL; /* Leaves and internal pages share the capacity. */
    Page *q = new_page(p->leaf);
    int cut = (p->n + 1) / 2; /* Lecture split: ceil on the left, floor on the right. */
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
 * The caller refreshes the parent once, including when no repair is needed.
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
            return splay_find(tree, key);
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
            changed = rb_insert(tree, key);
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
    if (!p) return (Check){true, -1, 1, 0};
    if (p->key <= lo || p->key >= hi) return (Check){false, 0, 0, 0};
    Check l = check_balanced(p->left, lo, p->key, rb);
    Check r = check_balanced(p->right, p->key, hi, rb);
    int h = 1 + (l.height > r.height ? l.height : r.height);
    bool ok = l.ok && r.ok;
    /* Ordinary RB trees allow red children on either side. */
    if (rb) ok = ok && l.black == r.black
        && (!p->left || p->left->parent == p)
        && (!p->right || p->right->parent == p)
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
    if (t->kind == TREE_SPLAY && t->root->parent) return false;
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
            if (t->kind == TREE_SPLAY && p->left->parent != p) { ok = false; break; }
            if (top == t->size) { ok = false; break; }
            stack[top++] = (Frame){p->left, f.lo, p->key};
        }
        if (p->right) {
            if (t->kind == TREE_SPLAY && p->right->parent != p) { ok = false; break; }
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
    if (p->n < min || p->n > ORDER) return false;
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
        return c.ok && c.count == t->size && (!rb || (!red(t->root) && (!t->root || !t->root->parent)));
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
