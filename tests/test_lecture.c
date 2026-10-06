/* White-box checks for the lecture's rotations and node colors.
 * Compile this file alone: it includes the private implementation.
 * The regular set-model suite remains independent in test_trees.c.
 */
#define TREE_TESTING /* Enable counters only in this executable. */
#include "../src/trees.c" /* Expose private nodes without changing the public API. */

/* Keep checks enabled under -O2 and report the failing expression. */
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)

/* Build a specific shape without triggering insertion rebalancing. */
static Node *fixture(int key, bool color, Node *l, Node *r) {
    Node *p = new_node(key);
    p->red = color; p->left = l; p->right = r;
    if (l) l->parent = p; /* Fixtures must satisfy the same parent-link rules. */
    if (r) r->parent = p; /* Right children also point back to their owner. */
    p->height = 777; /* A poison value: Splay and RB must leave it alone. */
    return p;
}

/* Both directions of zig, zig-zig and zig-zag must match slide 14. */
static void splay_shapes(void) {
    for (int mirror = 0; mirror < 2; ++mirror) {
        for (int shape = 0; shape < 3; ++shape) {
            Tree *t = tree_create(TREE_SPLAY);
            int xkey = shape == 2 ? 2 : 1; /* The middle key produces a zig-zag. */
            int pkey = shape == 2 ? 1 : 2; /* Other shapes use an outer grandchild. */
            /* Reflect key order and links to exercise the right-side cases. */
            if (mirror) { xkey = 4 - xkey; pkey = 4 - pkey; }
            Node *x = fixture(xkey, false, NULL, NULL);
            Node *p = fixture(pkey, false, NULL, NULL);
            Node *g = shape ? fixture(mirror ? 1 : 3, false, NULL, NULL) : NULL;
            if (xkey < pkey) p->left = x; else p->right = x;
            x->parent = p; /* X starts one level below P. */
            if (g) {
                if (pkey < g->key) g->left = p; else g->right = p;
                p->parent = g; /* Two-level cases start below G. */
            }
            t->root = g ? g : p; t->size = g ? 3 : 2; /* Set the exact fixture size. */
            linked_rotations = 0; /* Count this access, not fixture setup. */
            CHECK(tree_contains(t, xkey)); /* Exercise the public lookup, not only the helper. */
            CHECK(t->root == x && !x->parent && tree_validate(t)); /* X must be the root. */
            CHECK(linked_rotations == (g ? 2u : 1u)); /* Check one step or a rotation pair. */
            CHECK(x->height == 777 && p->height == 777);
            if (g) CHECK(g->height == 777); /* Rotating G must not update AVL metadata. */
            if (shape == 2) {
                /* Zig-zag leaves the former parent and grandparent beside X. */
                CHECK(x->left == (mirror ? g : p));
                CHECK(x->right == (mirror ? p : g));
            } else {
                CHECK((mirror ? x->left : x->right) == p);
                if (g) CHECK((mirror ? p->left : p->right) == g);
            }
            tree_destroy(t);
        }
    }
    /* Slide 16 accesses the bottom of the chain created by inserting 1..7. */
    Tree *t = tree_create(TREE_SPLAY);
    for (int k = 1; k <= 7; ++k) CHECK(tree_insert(t, k));
    CHECK(tree_contains(t, 1) && t->root->key == 1);
    CHECK(tree_validate(t));
    CHECK(tree_delete(t, 4)); /* Join TL and TR using the maximum of TL. */
    CHECK(t->root->key == 3 && tree_validate(t)); /* The predecessor roots the joined tree. */
    CHECK(!tree_contains(t, 8) && t->root->key == 7); /* A miss exposes the boundary. */
    CHECK(!tree_insert(t, 2) && t->root->key == 2); /* Duplicates still splay. */
    CHECK(tree_validate(t));
    tree_destroy(t);
}

/* Lecture 2, slide 4: insert 4 into the displayed colored tree. */
static void rb_slide_example(void) {
    Tree *t = tree_create(TREE_RB);
    Node *five = fixture(5, true, NULL, NULL);
    Node *eight = fixture(8, true, NULL, NULL);
    Node *seven = fixture(7, false, five, eight);
    Node *two = fixture(2, true, fixture(1, false, NULL, NULL), seven);
    Node *fourteen = fixture(14, false, NULL, fixture(15, true, NULL, NULL));
    Node *eleven = fixture(11, false, two, fourteen);
    t->root = eleven; t->size = 8; /* The slide starts with eight distinct keys. */
    CHECK(tree_validate(t)); /* A red right child is valid in ordinary RB. */
    linked_rotations = 0;
    CHECK(tree_insert(t, 4)); /* Triggers recoloring, an inner turn and an outer turn. */
    CHECK(t->root == seven && !seven->red); /* Match the final root in the slide. */
    CHECK(seven->left == two && seven->right == eleven); /* Check both subtrees. */
    CHECK(two->red && eleven->red && !five->red && !eight->red);
    CHECK(eleven->left == eight && eleven->right == fourteen);
    CHECK(five->left->key == 4 && five->left->red); /* The newly inserted leaf stays red. */
    CHECK(linked_rotations == 2 && tree_validate(t));
    CHECK(seven->height == 777 && two->height == 777 && eleven->height == 777);
    tree_destroy(t);
}

/* Deterministic mixed updates check all mirrored repair branches and bounds. */
static void rb_rotation_bounds(void) {
    Tree *t = tree_create(TREE_RB);
    bool present[257] = {false}; /* Independent membership reference for the trace. */
    uint32_t seed = 1949; /* Fixed seed keeps branch coverage reproducible. */
    for (int step = 0; step < 30000; ++step) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        int k = (int)(seed % 257u);
        linked_rotations = 0;
        if ((seed >> 16) & 1u) {
            CHECK(tree_insert(t, k) == !present[k]);
            present[k] = true;
            CHECK(linked_rotations <= 2); /* Slide 8: at most two on insertion. */
        } else {
            CHECK(tree_delete(t, k) == present[k]);
            present[k] = false;
            CHECK(linked_rotations <= 3); /* Slide 8: at most three on deletion. */
        }
        CHECK(tree_validate(t)); /* Check black height after every update. */
    }
    for (int side = 0; side < 2; ++side) {
        /* Require actual execution of every case, including mirror images. */
        for (int c = 0; c < 3; ++c) CHECK(rb_insert_cases[side][c] > 0);
        for (int c = 0; c < 4; ++c) CHECK(rb_delete_cases[side][c] > 0);
    }
    tree_destroy(t);
}

/* The fifth record overflows an order-four leaf into pages of three and two. */
static void bplus_capacity(void) {
    Tree *t = tree_create(TREE_BPLUS);
    for (int k = 0; k < ORDER; ++k) CHECK(tree_insert(t, k));
    CHECK(t->page->leaf && t->page->n == ORDER); /* Four keys must still fit. */
    CHECK(tree_insert(t, ORDER));
    CHECK(!t->page->leaf && t->page->n == 2); /* Overflow creates a routing root. */
    CHECK(t->page->child[0]->n == 3 && t->page->child[1]->n == 2);
    CHECK(t->page->keys[0] == 3 && tree_validate(t)); /* Separator copies the right minimum. */
    CHECK(tree_delete(t, 4)); /* The right leaf borrows from its left sibling. */
    CHECK(t->page->child[0]->n == 2 && t->page->child[1]->n == 2);
    CHECK(tree_delete(t, 3)); /* A merge collapses the root back to one leaf. */
    CHECK(t->page->leaf && t->page->n == 3 && tree_validate(t));
    tree_destroy(t);
}

int main(void) {
    splay_shapes();
    rb_slide_example();
    rb_rotation_bounds();
    bplus_capacity();
    puts("PASS lecture checks: Splay shapes, RB cases and rotation bounds, B+ capacity.");
    return EXIT_SUCCESS;
}
