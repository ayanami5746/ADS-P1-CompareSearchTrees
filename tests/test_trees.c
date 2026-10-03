#include "trees.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Never use assert: these checks must remain active in optimized builds. */
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)
/* A fixed-width generator makes a seed reproduce the same trace everywhere. */
static uint32_t state;
/*
 * Generate a deterministic trace for correctness testing.
 * The caller supplies a nonzero seed before starting a test case.
 * Fixed-width unsigned shifts avoid signed-overflow behavior.
 * The output is reproducible independently of the C library rand.
 * Modulo selection is sufficient for fuzz coverage in these tests.
 * Performance experiments use a separate unbiased bounded sampler.
 * The generator owns no dynamic storage and needs no cleanup.
 * Changing the seed explores a different mixed-operation history.
 * Failures can be reproduced by rerunning the same seed and step.
 */
static uint32_t random32(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
/* Exhaustive membership checks compare against an independent boolean array. */
/*
 * Cross-check every possible key in the bounded reference domain.
 * The boolean array is independent of all five tree implementations.
 * Array index i represents the signed key i minus half the range.
 * The supplied size is maintained from reference-model operations.
 * Validate the structure before checking any membership values.
 * Every expected present and absent key is then queried.
 * For splay trees those queries can rearrange many internal links.
 * A second validation checks that these rearrangements stayed valid.
 * This helper runs only in correctness tests, never in timed phases.
 */
static void compare(Tree *t, const bool *reference, int range, size_t size) {
    CHECK(tree_size(t) == size);
    CHECK(tree_validate(t));
    for (int i = 0; i < range; ++i) CHECK(tree_contains(t, i - range / 2) == reference[i]);
    /* Lookups mutate splay trees, so validate again after scanning the domain. */
    CHECK(tree_validate(t));
}
/*
 * Exercise boundary semantics on a newly created implementation.
 * The empty representation must already satisfy validation.
 * Deleting or searching an absent key must report false.
 * Extreme signed integers ensure no reserved key sentinel is used.
 * Immediate duplicate insertion checks distinct-set semantics.
 * Repeated deletion checks that size cannot underflow.
 * Membership is verified after both insertion and deletion.
 * Every step receives an implementation-specific structural check.
 * Reinsertion after clearing tests whether the empty tree is reusable.
 * Destruction finishes the case without retaining any records.
 */
static void edge_cases(TreeKind kind) {
    Tree *t = tree_create(kind);
    const int keys[] = {0, INT_MIN, INT_MAX, -1, 1, -100, 100};
    CHECK(tree_validate(t));
    CHECK(!tree_delete(t, 42));
    CHECK(!tree_contains(t, 42));
    /* Exercise signed extrema without relying on key +/- 1 arithmetic. */
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK(tree_insert(t, keys[i]));
        CHECK(!tree_insert(t, keys[i]));
        CHECK(tree_contains(t, keys[i]));
        CHECK(tree_validate(t));
    }
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK(tree_delete(t, keys[i]));
        CHECK(!tree_delete(t, keys[i]));
        CHECK(!tree_contains(t, keys[i]));
        CHECK(tree_validate(t));
    }
    CHECK(tree_size(t) == 0);
    /* Reuse after complete deletion catches stale roots and leaf-chain links. */
    CHECK(tree_insert(t, 17));
    CHECK(tree_delete(t, 17));
    CHECK(tree_validate(t));
    tree_destroy(t);
}
/*
 * Run a mixed trace against an independent finite-domain set model.
 * The domain spans negative, zero, and positive integers.
 * Insertions include both new and already present records.
 * Deletions include both successful and unsuccessful operations.
 * Searches check membership and exercise splay access behavior.
 * The model counter changes only when the boolean set changes.
 * Each trace step validates size and the full tree structure.
 * Periodic domain scans detect lost or unexpected records.
 * The final cleanup deletes every model survivor once.
 * An empty valid result is required before releasing storage.
 */
static void randomized(TreeKind kind, uint32_t seed) {
    enum { RANGE = 2048, STEPS = 30000 };
    bool reference[RANGE] = {false};
    size_t size = 0;
    Tree *t = tree_create(kind);
    state = seed;
    /* Mixing insert/delete/search tests duplicates and unsuccessful operations. */
    for (int step = 0; step < STEPS; ++step) {
        int index = (int)(random32() % RANGE), key = index - RANGE / 2;
        uint32_t op = random32() % 3;
        if (op == 0) {
            CHECK(tree_insert(t, key) == !reference[index]);
            if (!reference[index]) { reference[index] = true; ++size; }
        } else if (op == 1) {
            CHECK(tree_delete(t, key) == reference[index]);
            if (reference[index]) { reference[index] = false; --size; }
        } else CHECK(tree_contains(t, key) == reference[index]);
        /* Every mutation receives a full structural check, not just end-state checks. */
        CHECK(tree_size(t) == size);
        CHECK(tree_validate(t));
        if (step % 1000 == 0) compare(t, reference, RANGE, size);
    }
    compare(t, reference, RANGE, size);
    /* Delete all surviving keys and require a valid empty tree. */
    for (int i = 0; i < RANGE; ++i) if (reference[i]) CHECK(tree_delete(t, i - RANGE / 2));
    CHECK(tree_size(t) == 0 && tree_validate(t));
    tree_destroy(t);
}
/* All three required scenarios use the same set and independent permutations. */
/*
 * Exercise one assignment workload outside the performance harness.
 * The input arrays contain identical distinct-key sets.
 * Ascending deletion and reversed deletion use deterministic orders.
 * Random insertion and deletion use separately permuted arrays.
 * Every insertion must report successful set growth.
 * The populated tree must validate and contain n records.
 * Every deletion must report successful set shrinkage.
 * Large traces sample structure during deletion to limit test overhead.
 * Mixed fuzz traces elsewhere already validate after every operation.
 * The final tree must be empty with valid internal structure.
 * The caller selects N=5000 or the full N=100000 stress size.
 */
static void scenario(TreeKind kind, int n, int mode) {
    int *ins = malloc((size_t)n * sizeof(*ins)), *del = malloc((size_t)n * sizeof(*del));
    CHECK(ins && del);
    for (int i = 0; i < n; ++i) { ins[i] = i; del[i] = mode == 1 ? n - 1 - i : i; }
    if (mode == 2) {
        state = 987654321;
        for (int i = n - 1; i > 0; --i) {
            int j = (int)(random32() % (uint32_t)(i + 1)), tmp = ins[i];
            ins[i] = ins[j]; ins[j] = tmp;
            j = (int)(random32() % (uint32_t)(i + 1)); tmp = del[i];
            del[i] = del[j]; del[j] = tmp;
        }
    }
    Tree *t = tree_create(kind);
    for (int i = 0; i < n; ++i) CHECK(tree_insert(t, ins[i]));
    CHECK(tree_validate(t) && tree_size(t) == (size_t)n);
    for (int i = 0; i < n; ++i) {
        CHECK(tree_delete(t, del[i]));
        /* Sample large workloads; small randomized tests validate every operation. */
        if (i % 1024 == 0) CHECK(tree_validate(t));
    }
    CHECK(tree_validate(t) && tree_size(t) == 0);
    free(ins); free(del); tree_destroy(t);
}
/* Small permutations cover every pairing of insertion and deletion orders. */
static int permutations[120][5], permutation_count;
/*
 * Generate all permutations of the five-key exhaustive test set.
 * The prefix before at is already fixed for this recursion level.
 * Each remaining key is swapped into the next prefix position.
 * Recursive return restores the array before trying the next key.
 * A completed permutation is copied into the fixed output table.
 * Five factorial equals 120, matching the table's first dimension.
 * Generation runs once and the table is shared across all tree kinds.
 * The source array contains distinct keys, so no duplicate filtering is
 * needed.
 * Recursion is bounded by five and uses no heap storage.
 */
static void enumerate(int *a, int at) {
    if (at == 5) {
        for (int i = 0; i < 5; ++i) permutations[permutation_count][i] = a[i];
        ++permutation_count; return;
    }
    for (int i = at; i < 5; ++i) {
        int tmp = a[at]; a[at] = a[i]; a[i] = tmp;
        enumerate(a, at + 1);
        tmp = a[at]; a[at] = a[i]; a[i] = tmp;
    }
}
/*
 * Test the Cartesian product of insertion and deletion permutations.
 * There are 120 insertion orders and 120 deletion orders.
 * Each pair starts from a newly allocated empty tree.
 * Checking after every insertion catches transient balancing errors.
 * Checking after every deletion catches incorrect repair propagation.
 * All keys are known to be distinct and present when deleted.
 * Thus every update must report a successful set change.
 * These small cases exhaust binary-tree shapes but not large B+ splits.
 * Random and scenario tests provide the complementary multi-page coverage.
 * No state or allocated nodes are reused across permutation pairs.
 */
static void exhaustive(TreeKind kind) {
    for (int a = 0; a < permutation_count; ++a) for (int b = 0; b < permutation_count; ++b) {
        Tree *t = tree_create(kind);
        for (int i = 0; i < 5; ++i) {
            CHECK(tree_insert(t, permutations[a][i])); CHECK(tree_validate(t));
        }
        for (int i = 0; i < 5; ++i) {
            CHECK(tree_delete(t, permutations[b][i])); CHECK(tree_validate(t));
        }
        tree_destroy(t);
    }
}
/*
 * Run the same suite independently for every tree implementation.
 * The only accepted optional argument is --stress.
 * The ordinary suite keeps scenario sizes modest for CI execution.
 * Stress mode raises every scenario to the assignment's largest size.
 * Invalid arguments fail explicitly rather than silently changing coverage.
 * Exhaustive orders are generated once before any tree-specific tests.
 * Four seeds provide 120000 mixed operations per implementation.
 * A populated ordered tree is destroyed without deleting its keys first.
 * That cleanup case targets nonrecursive destruction on long chains.
 * Progress is flushed after each implementation for long stress runs.
 * The final success line is emitted only after every check has passed.
 */
int main(int argc, char **argv) {
    int a[] = {0, 1, 2, 3, 4};
    int n = 5000;
    /* The optional --stress argument exercises 100000 distinct keys per scenario. */
    if (argc == 2 && argv[1][0] == '-' && argv[1][1] == '-') {
        const char *p = argv[1], *q = "--stress";
        while (*p && *p == *q) { ++p; ++q; }
        if (*p || *q) { fputs("Usage: test_trees [--stress]\n", stderr); return EXIT_FAILURE; }
        n = 100000;
    } else if (argc != 1) { fputs("Usage: test_trees [--stress]\n", stderr); return EXIT_FAILURE; }
    enumerate(a, 0);
    tree_destroy(NULL);
    CHECK(tree_create(TREE_COUNT) == NULL);
    for (TreeKind kind = TREE_BST; kind < TREE_COUNT; ++kind) {
        edge_cases(kind); exhaustive(kind);
        for (uint32_t seed = 1; seed <= 4; ++seed) randomized(kind, seed * 1234567u);
        for (int mode = 0; mode < 3; ++mode) scenario(kind, n, mode);
        /* Destroy a nonempty degenerate tree to test the iterative cleanup path. */
        Tree *t = tree_create(kind);
        for (int i = 0; i < 20000; ++i) CHECK(tree_insert(t, i));
        CHECK(tree_validate(t)); tree_destroy(t);
        printf("PASS %s: edges, 14400 permutation pairs, 120000 mixed operations, scenarios N=%d\n", tree_name(kind), n);
        fflush(stdout);
    }
    puts("All five tree implementations passed.");
    return EXIT_SUCCESS;
}
