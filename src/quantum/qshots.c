/*
  QBIT NOVA C v4.7 - Shots Histogram runner.

  Software virtual QCPU on classical hardware. NOT physical quantum hardware.

  Builds the circuit state vector ONCE, then samples N measurement shots
  from |amplitude|^2 -- the same "counts" a real quantum backend returns.

  Circuit format (same as Circuit VM):
    qubits N
    h Q | x Q | y Q | z Q | s Q | t Q
    cx C T | swap A B
    ghz            (macro: h q0 + cx chain)
    measure        (optional; shots always measure)

  Usage: qnova-shots <circuit.qnc> <shots>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#include <unistd.h>

#define QPI 3.14159265358979323846
#define MAX_QUBITS 16
#define MAX_OPS 4096
#define MAX_SHOTS 1000000

typedef struct { char op[8]; int a; int b; } Op;
static int NQ = 0;
static Op OPS[MAX_OPS];
static int NOPS = 0;

static void fail(const char *m) { fprintf(stderr, "ERROR: %s\n", m); exit(1); }
static void add_op(const char *op, int a, int b) {
    if (NOPS >= MAX_OPS) fail("too many ops");
    snprintf(OPS[NOPS].op, sizeof(OPS[NOPS].op), "%s", op);
    OPS[NOPS].a = a; OPS[NOPS].b = b; NOPS++;
}
static void require_qubit(int q) { if (q < 0 || q >= NQ) fail("qubit index out of range"); }
static void add_ghz(void) {
    if (NQ < 2) fail("ghz requires qubits N (N>=2) first");
    add_op("h", 0, 0);
    for (int q = 0; q < NQ - 1; q++) add_op("cx", q, q + 1);
}
static void print_bits(long idx, int n) {
    printf("|");
    for (int j = 0; j < n; j++) printf("%d", (int)((idx >> (n - 1 - j)) & 1));
    printf(">");
}
static void g_single(double complex *a, int n, int q, double complex m00,
                     double complex m01, double complex m10, double complex m11) {
    int p = n - 1 - q; long dim = 1L << n;
    for (long i = 0; i < dim; i++) {
        if ((i >> p) & 1) continue;
        long j = i | (1L << p);
        double complex x = a[i], y = a[j];
        a[i] = m00 * x + m01 * y; a[j] = m10 * x + m11 * y;
    }
}
static void g_h(double complex *a, int n, int q) { double s = 1.0 / sqrt(2.0); g_single(a, n, q, s, s, s, -s); }
static void g_x(double complex *a, int n, int q) { g_single(a, n, q, 0, 1, 1, 0); }
static void g_y(double complex *a, int n, int q) { g_single(a, n, q, 0, -I, I, 0); }
static void g_z(double complex *a, int n, int q) { g_single(a, n, q, 1, 0, 0, -1); }
static void g_s(double complex *a, int n, int q) { g_single(a, n, q, 1, 0, 0, I); }
static void g_t(double complex *a, int n, int q) { g_single(a, n, q, 1, 0, 0, cexp(I * QPI / 4.0)); }
static void g_cx(double complex *a, int n, int c, int t) {
    if (c == t) fail("cx control==target");
    int pc = n - 1 - c, pt = n - 1 - t; long dim = 1L << n;
    for (long i = 0; i < dim; i++) {
        if (!((i >> pc) & 1)) continue;
        if ((i >> pt) & 1) continue;
        long j = i | (1L << pt);
        double complex tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
}
static void g_swap(double complex *a, int n, int x, int y) {
    if (x == y) return; g_cx(a, n, x, y); g_cx(a, n, y, x); g_cx(a, n, x, y);
}
static void load_circuit(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char kw[64] = {0}; int q1 = 0, q2 = 0;
        if (line[0] == '#' || line[0] == '\n') continue;
        int got = sscanf(line, "%63s %d %d", kw, &q1, &q2);
        if (got < 1) continue;
        if (!strcmp(kw, "qubits")) { NQ = q1; if (NQ < 1 || NQ > MAX_QUBITS) fail("qubits 1..16"); }
        else if (!strcmp(kw, "ghz")) add_ghz();
        else if (!strcmp(kw, "h") || !strcmp(kw, "x") || !strcmp(kw, "y") ||
                 !strcmp(kw, "z") || !strcmp(kw, "s") || !strcmp(kw, "t")) { require_qubit(q1); add_op(kw, q1, 0); }
        else if (!strcmp(kw, "cx") || !strcmp(kw, "swap")) { require_qubit(q1); require_qubit(q2); add_op(kw, q1, q2); }
        else if (!strcmp(kw, "measure")) { /* shots always measure */ }
        else { fprintf(stderr, "ERROR: unknown op: %s\n", kw); exit(1); }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <circuit.qnc> <shots>\n", argv[0]); return 2; }
    long shots = atol(argv[2]);
    if (shots < 1 || shots > MAX_SHOTS) fail("shots must be 1..1000000");
    srand((unsigned)(time(NULL) ^ ((unsigned)getpid() << 16) ^ (unsigned)clock()));

    load_circuit(argv[1]);
    if (NQ < 1) fail("circuit missing qubits N");

    long dim = 1L << NQ;
    double complex *amp = calloc((size_t)dim, sizeof(double complex));
    if (!amp) fail("alloc state");
    amp[0] = 1.0;
    for (int i = 0; i < NOPS; i++) {
        Op o = OPS[i];
        if (!strcmp(o.op, "h")) g_h(amp, NQ, o.a);
        else if (!strcmp(o.op, "x")) g_x(amp, NQ, o.a);
        else if (!strcmp(o.op, "y")) g_y(amp, NQ, o.a);
        else if (!strcmp(o.op, "z")) g_z(amp, NQ, o.a);
        else if (!strcmp(o.op, "s")) g_s(amp, NQ, o.a);
        else if (!strcmp(o.op, "t")) g_t(amp, NQ, o.a);
        else if (!strcmp(o.op, "cx")) g_cx(amp, NQ, o.a, o.b);
        else if (!strcmp(o.op, "swap")) g_swap(amp, NQ, o.a, o.b);
    }

    /* cumulative distribution + norm */
    double *cum = malloc((size_t)dim * sizeof(double));
    if (!cum) fail("alloc cum");
    double norm = 0.0;
    for (long i = 0; i < dim; i++) {
        double p = creal(amp[i]) * creal(amp[i]) + cimag(amp[i]) * cimag(amp[i]);
        norm += p; cum[i] = norm;
    }

    long *counts = calloc((size_t)dim, sizeof(long));
    if (!counts) fail("alloc counts");
    for (long s = 0; s < shots; s++) {
        double r = ((double)rand() / RAND_MAX) * norm;
        long lo = 0, hi = dim - 1, pick = dim - 1;
        while (lo <= hi) {                 /* binary search cumulative */
            long mid = (lo + hi) / 2;
            if (r <= cum[mid]) { pick = mid; hi = mid - 1; }
            else lo = mid + 1;
        }
        counts[pick]++;
    }

    printf("=== QBIT NOVA SHOTS HISTOGRAM ===\n");
    printf("[SHOTS] boundary: software virtual QCPU, not physical quantum hardware\n");
    printf("[SHOTS] qubits: %d  shots: %ld  norm: %.6f\n", NQ, shots, norm);
    long total = 0, distinct = 0;
    for (long i = 0; i < dim; i++) {
        if (counts[i] > 0) {
            printf("[SHOTS] "); print_bits(i, NQ);
            printf(": %ld\n", counts[i]);
            total += counts[i]; distinct++;
        }
    }
    printf("[SHOTS] distinct outcomes: %ld\n", distinct);
    printf("[SHOTS] counts sum: %ld\n", total);
    if (total == shots && fabs(norm - 1.0) < 1e-6)
        printf("PASS: QCPU_SHOTS_HISTOGRAM_READY\n");
    else
        printf("FAIL: QCPU_SHOTS_HISTOGRAM_INVALID\n");

    free(amp); free(cum); free(counts);
    return 0;
}
