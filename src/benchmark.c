/* POSIX clock_gettime declarations must precede system includes. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "trees.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* Use monotonic wall time rather than process CPU time or calendar time. */
/*
 * Return a monotonic timestamp measured in seconds.
 * Windows uses its high-resolution performance counter.
 * POSIX systems use CLOCK_MONOTONIC to avoid wall-clock corrections.
 * Only differences between two returned values are meaningful.
 * Conversion to double retains useful subsecond timing resolution.
 * Timer failures abort instead of generating misleading measurements.
 * Each operation phase takes one start and one end timestamp.
 * The benchmark reports elapsed wall time, including OS scheduling.
 * External background workloads can therefore affect measurements.
 * Repetition ranges expose some of this variation in the plots.
 */
static double seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER now, frequency;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&now)) {
        fputs("High resolution timer failed\n", stderr); exit(EXIT_FAILURE);
    }
    return (double)now.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) { perror("clock_gettime"); exit(EXIT_FAILURE); }
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#endif
}
/* The PRNG is independent of the platform's implementation of rand(). */
static uint32_t state;
/*
 * Advance the deterministic nonzero xorshift32 state.
 * All shifts and arithmetic operate on an unsigned 32-bit value.
 * The state must never be initialized to zero.
 * The three shift distances define the same sequence on each platform.
 * This generator is for repeatable experiments, not cryptography.
 * Its output range excludes zero and includes UINT32_MAX.
 * The bounded helper accounts for that exact output range.
 * Random generation occurs before any tree timing begins.
 * All implementations share the already generated operation arrays.
 * The case seed is recorded alongside each measurement.
 */
static uint32_t random32(void) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
/* Xorshift outputs 1..UINT32_MAX; subtract one and reject the incomplete tail. */
/*
 * Sample uniformly from the integers zero through bound minus one.
 * The caller supplies a strictly positive bound.
 * The generator has UINT32_MAX possible nonzero outputs.
 * Subtracting one maps these to a zero-based contiguous range.
 * Discarding its incomplete last block avoids modulo bias.
 * Accepted values partition into equally sized residue classes.
 * Fisher-Yates calls this with bounds no larger than the input size.
 * The retry loop is part of untimed sequence preparation.
 * Unsigned types make the endpoint calculations well-defined.
 * The helper updates the shared deterministic PRNG state.
 */
static uint32_t bounded(uint32_t bound) {
    uint32_t limit = UINT32_MAX - UINT32_MAX % bound, value;
    do { value = random32() - 1u; } while (value >= limit);
    return value % bound;
}
/*
 * Permute an initialized array in place using Fisher-Yates.
 * At step i the remaining choice is uniformly drawn from zero to i.
 * The selected element is swapped into its final suffix position.
 * The loop stops at one because the last choice is forced.
 * Every input key remains present exactly once.
 * The insertion and deletion arrays are shuffled separately.
 * Both use consecutive draws from the same recorded case seed.
 * The input must contain at least n initialized integer elements.
 * Time is linear in n and extra storage is constant.
 * This work is deliberately excluded from reported tree times.
 */
static void shuffle(int *a, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = (int)bounded((uint32_t)(i + 1)), tmp = a[i];
        a[i] = a[j]; a[j] = tmp;
    }
}
/* Reject malformed, negative, overflowing, or impractically large CLI values. */
/*
 * Parse a positive decimal CLI argument with an explicit upper bound.
 * Reject a leading minus sign before calling strtoul.
 * An empty string is not interpreted as zero.
 * Trailing nonnumeric characters are rejected by checking end.
 * errno detects conversion overflow in the platform unsigned long type.
 * The maximum also protects later narrowing conversions.
 * Zero is forbidden for repetitions, limits, and PRNG seeds.
 * Malformed input prints the original token for diagnosis.
 * Failure occurs before an output CSV file is opened.
 * Successful values are returned without locale-specific formatting.
 */
static unsigned long number(const char *s, unsigned long maximum) {
    char *end;
    errno = 0;
    if (!s[0] || s[0] < '0' || s[0] > '9') goto invalid;
    unsigned long n = strtoul(s, &end, 10);
    if (errno || *end || n == 0 || n > maximum) goto invalid;
    return n;
invalid:
    fprintf(stderr, "Invalid numeric argument: %s\n", s); exit(EXIT_FAILURE);
}
/* Allocate and prepare sequences before entering any measured interval. */
/*
 * Measure all five implementations on one fixed experimental case.
 * The record set is always the distinct integers zero through n-1.
 * Only insertion and deletion order differ between scenarios.
 * Both operation arrays are shared across all tree kinds.
 * A fresh empty tree is created for each measured implementation.
 * Successful operation counts prevent silent duplicate or missing work.
 * Full structural checks run between phases and after deletion.
 * These checks are not included in either elapsed interval.
 * CSV rows are flushed as cases finish to preserve partial progress.
 * The caller controls repetition, input size, scenario, and case seed.
 * Tree order rotates by repetition to reduce systematic ordering bias.
 * Cleanup releases the already emptied tree and the shared arrays.
 */
static void run(FILE *out, int n, int mode, int repeat, uint32_t seed) {
    static const char *const modes[] = {"ascending_ascending", "ascending_descending", "random_random"};
    int *ins = malloc((size_t)n * sizeof(*ins)), *del = malloc((size_t)n * sizeof(*del));
    if (!ins || !del) { fputs("Out of memory\n", stderr); exit(EXIT_FAILURE); }
    for (int i = 0; i < n; ++i) { ins[i] = i; del[i] = mode == 1 ? n - 1 - i : i; }
    state = seed;
    if (mode == 2) { shuffle(ins, n); shuffle(del, n); }
    /* Rotate the tree order across repetitions to reduce fixed-order bias. */
    for (int offset = 0; offset < TREE_COUNT; ++offset) {
        TreeKind kind = (TreeKind)((offset + repeat - 1) % TREE_COUNT);
        Tree *t = tree_create(kind);
        double start = seconds();
        size_t inserted = 0, deleted = 0;
        for (int i = 0; i < n; ++i) inserted += tree_insert(t, ins[i]);
        double insertion = seconds() - start;
        /* No validation, sequence generation, or output is included in timing. */
        if (inserted != (size_t)n || tree_size(t) != (size_t)n || !tree_validate(t)) {
            fprintf(stderr, "Insertion validation failed: %s\n", tree_name(kind)); exit(EXIT_FAILURE);
        }
        start = seconds();
        for (int i = 0; i < n; ++i) deleted += tree_delete(t, del[i]);
        double deletion = seconds() - start;
        if (deleted != (size_t)n || tree_size(t) != 0 || !tree_validate(t)) {
            fprintf(stderr, "Deletion validation failed: %s\n", tree_name(kind)); exit(EXIT_FAILURE);
        }
        /* The per-case seed lets classmates reproduce both random permutations. */
        if (fprintf(out, "%s,%s,%d,%d,%lu,%.9f,%.9f,%.9f\n", tree_name(kind), modes[mode], n,
            repeat, (unsigned long)seed, insertion, deletion, insertion + deletion) < 0 || fflush(out) != 0) {
            perror("CSV write"); exit(EXIT_FAILURE);
        }
        fprintf(stderr, "%s %-23s N=%d repeat=%d total=%.6fs\n", tree_name(kind), modes[mode], n, repeat, insertion + deletion);
        tree_destroy(t);
    }
    free(ins); free(del);
}
/*
 * Parse experiment settings and emit the complete measurement grid.
 * The default five sizes span the required 10^3 through 10^5 range.
 * Each size is tested in all three required ordering scenarios.
 * Every scenario has three independent repetitions by default.
 * The maximum-size option truncates the fixed size list.
 * It is a convenience for smoke tests, not a new workload definition.
 * The base seed is mixed with size, scenario, and repetition index.
 * Unsigned arithmetic intentionally defines wraparound in seed mixing.
 * The output header names units explicitly as seconds.
 * Closing the file is checked so late I/O errors are reported.
 * Invalid options return a failure exit status and usage text.
 * No plotting dependency is needed to execute the C benchmark.
 */
int main(int argc, char **argv) {
    const int sizes[] = {1000, 3000, 10000, 30000, 100000};
    int repeats = 3, max_n = 100000;
    uint32_t seed = 20261003u;
    const char *path = "benchmark.csv";
    /* Paired options keep the runner simple and make experiments explicit. */
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) goto usage;
        if (strcmp(argv[i], "--output") == 0) path = argv[i + 1];
        else if (strcmp(argv[i], "--repeats") == 0) repeats = (int)number(argv[i + 1], 100);
        else if (strcmp(argv[i], "--max-n") == 0) max_n = (int)number(argv[i + 1], 100000);
        else if (strcmp(argv[i], "--seed") == 0) seed = (uint32_t)number(argv[i + 1], UINT32_MAX);
        else goto usage;
    }
    if (max_n < 1000) goto usage;
    FILE *out = fopen(path, "w");
    if (!out) { perror(path); return EXIT_FAILURE; }
    fputs("tree,scenario,n,repeat,seed,insert_seconds,delete_seconds,total_seconds\n", out);
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
        if (sizes[s] > max_n) continue;
        for (int mode = 0; mode < 3; ++mode) for (int r = 1; r <= repeats; ++r) {
            /* Unsigned overflow is defined; replace the forbidden zero PRNG state. */
            uint32_t case_seed = seed ^ ((uint32_t)sizes[s] * 2654435761u) ^ ((uint32_t)r * 2246822519u) ^ (uint32_t)mode;
            run(out, sizes[s], mode, r, case_seed ? case_seed : 1u);
        }
    }
    if (fclose(out) != 0) { perror("CSV close"); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
usage:
    fputs("Usage: benchmark [--output FILE] [--repeats 1..100] [--max-n 1000..100000] [--seed 1..4294967295]\n", stderr);
    return EXIT_FAILURE;
}
