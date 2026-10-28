// Name and Surname : Hatice Sila Ceylan <251ADB129>
// Github Repository : https://github.com/silaceylan26/calc.c-sila
// Compile with : gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used : AI tool (claude) link : https://claude.ai/share/15c147dc-f039-4128-bd00-6e66ea3acf36

#define STUDENT_NAME     "Hatice Sila"
#define STUDENT_LASTNAME "Ceylan"
#define STUDENT_ID       "251ADB129"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "calc: out of memory\n"); exit(1); }
    return p;
}

// AI prompt: Which data structure should I use to read the file?
typedef struct { char *data; size_t len, cap; } Out;



// AI prompt: How can a buffer be grown efficiently consider 
// the approach of doubling its size using `realloc`.
static void out_append(Out *o, const char *s, size_t n) {
    if (o->len + n > o->cap) {
        size_t ncap = o->cap ? o->cap : 256;
        while (ncap < o->len + n) ncap *= 2;
        char *nd = realloc(o->data, ncap);
        if (!nd) { fprintf(stderr, "calc: out of memory\n"); exit(1); }
        o->data = nd;
        o->cap = ncap;
    }
    memcpy(o->data + o->len, s, n);
    o->len += n;
}


static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = xmalloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    *len_out = got;
    return buf;
}

// AI prompt: So, what should I define first in my code? (1)
typedef struct { int is_float; long long i; double d; } Value;

typedef enum {
    T_NUM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_POW, T_LP, T_RP,
    T_EOL,
    T_BAD
} TokType;


typedef struct { TokType type; const char *start; Value val; } Token;

// AI prompt: So, what should I define first in my code? (2)
typedef struct {
    char *text;
    char *end;
    char *p;
    Token tok;
    size_t err_pos;
    int failed;
} Parser;


// We track positions using pointer arithmetic (current pointer minus the start 
// of the text buffer). This avoids costly index lookups and allows O(1) 
// position tracking across large files.
static void next_token(Parser *ps) {
    char *p = ps->p;
    while (*p == ' ' || *p == '\t') p++;
    Token t;
    t.start = p;
    t.val.is_float = 0; t.val.i = 0; t.val.d = 0.0;

    if (p >= ps->end) {
        t.type = T_EOL;
    } else if (*p == '\n') {
        t.type = T_EOL;
    } else if (isdigit((unsigned char)*p)) {
        char *q = p;
        unsigned long long v = 0;
        while (isdigit((unsigned char)*q)) { v = v * 10 + (unsigned long long)(*q - '0'); q++; }
        t.type = T_NUM;

        if (*q == '.' && isdigit((unsigned char)q[1])) {
            char *r = q + 1;
            while (isdigit((unsigned char)*r)) r++;

            char saved = *r;
            *r = '\0';
            t.val.is_float = 1;
            t.val.d = strtod(p, NULL);
            *r = saved;
            q = r;
        } else {
            t.val.i = (long long)v;
        }
        p = q;
        ps->p = p;
        ps->tok = t;
        return;
    } else {

        // AI prompt: What is the cleanest way to implement end-of-line (T_EOL) checks and 
        // error-position tracking within the `next_token` and `parse_expr` functions?
        switch (*p) {
            case '+': t.type = T_PLUS;  p++; break;
            case '-': t.type = T_MINUS; p++; break;
            case '/': t.type = T_SLASH; p++; break;
            case '(': t.type = T_LP;    p++; break;
            case ')': t.type = T_RP;    p++; break;
            case '*':

                if (p[1] == '*') { t.type = T_POW; p += 2; }
                else             { t.type = T_STAR; p++; }
                break;
            default: t.type = T_BAD; p++; break;
        }
        ps->p = p;
        ps->tok = t;
        return;
    }
    ps->p = p;
    ps->tok = t;
}


static unsigned long long ipow(unsigned long long base, unsigned long long exp) {
    unsigned long long result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        if (exp) base *= base;
    }
    return result;
}

// Errors are reported at the token's start to satisfy the 1-based position requirement.
static int fail(Parser *ps, const char *at) {
    ps->err_pos = (size_t)(at - ps->text) + 1;
    ps->failed = 1;
    return 0;
}

static int apply(Parser *ps, char op, const Value *a, const Value *b,
                 const char *opAt, const char *rAt, Value *out) {
    if (!a->is_float && !b->is_float) {

        unsigned long long x = (unsigned long long)a->i, y = (unsigned long long)b->i, r = 0;
        switch (op) {
            case '+': r = x + y; break;
            case '-': r = x - y; break;
            case '*': r = x * y; break;
            case '/':
                if (b->i == 0) return fail(ps, rAt);
                if (b->i == -1) r = 0ULL - x;
                else r = (unsigned long long)(a->i / b->i);
                break;
            case '^':
                if (b->i < 0) return fail(ps, rAt);
                r = ipow(x, y);
                break;
        }
        out->is_float = 0;
        out->i = (long long)r;
        out->d = 0.0;
        return 1;
    }


    // AI prompt: How can I ensure that it identifies a number 
    // as a float/double if there are digits following a decimal point?
    double x = a->is_float ? a->d : (double)a->i;
    double y = b->is_float ? b->d : (double)b->i;
    double r = 0.0;
    switch (op) {
        case '+': r = x + y; break;
        case '-': r = x - y; break;
        case '*': r = x * y; break;
        case '/':
            if (y == 0.0) return fail(ps, rAt);
            r = x / y;
            break;
        case '^': r = pow(x, y); break;
    }
    if (!isfinite(r)) return fail(ps, opAt);
    out->is_float = 1;
    out->d = r;
    out->i = 0;
    return 1;
}


static int parse_expr(Parser *ps, Value *out);

static int parse_primary(Parser *ps, Value *out) {
    Token t = ps->tok;
    if (t.type == T_NUM) {
        *out = t.val;
        next_token(ps);
        return 1;
    }
    if (t.type == T_LP) {
        next_token(ps);
        if (!parse_expr(ps, out)) return 0;
        if (ps->tok.type != T_RP) return fail(ps, ps->tok.start);
        next_token(ps);
        return 1;
    }

    return fail(ps, t.start);
}

// Calls itself recursively it makes ** right-associative so that
// 2**3**2 groups as 2**(3**2) rather than (2**3)**2
static int parse_power(Parser *ps, Value *out) {
    Value left;
    if (!parse_primary(ps, &left)) return 0;
    if (ps->tok.type == T_POW) {
        const char *opAt = ps->tok.start;
        next_token(ps);
        const char *rAt = ps->tok.start;
        Value right;
        if (!parse_power(ps, &right)) return 0;
        return apply(ps, '^', &left, &right, opAt, rAt, out);
    }
    *out = left;
    return 1;
}

// AI prompt: What should I pay attention to when writing the priority order?
static int parse_term(Parser *ps, Value *out) {
    Value acc;
    if (!parse_power(ps, &acc)) return 0;
    while (ps->tok.type == T_STAR || ps->tok.type == T_SLASH) {
        char op = ps->tok.type == T_STAR ? '*' : '/';
        const char *opAt = ps->tok.start;
        next_token(ps);
        const char *rAt = ps->tok.start; // keeping to divide by 0 
        Value right;
        if (!parse_power(ps, &right)) return 0;
        if (!apply(ps, op, &acc, &right, opAt, rAt, &acc)) return 0;
    }
    *out = acc;
    return 1;
}

static int parse_expr(Parser *ps, Value *out) {
    Value acc;
    if (!parse_term(ps, &acc)) return 0;
    while (ps->tok.type == T_PLUS || ps->tok.type == T_MINUS) {
        char op = ps->tok.type == T_PLUS ? '+' : '-';
        const char *opAt = ps->tok.start;
        next_token(ps);
        const char *rAt = ps->tok.start;
        Value right;
        if (!parse_term(ps, &right)) return 0;
        if (!apply(ps, op, &acc, &right, opAt, rAt, &acc)) return 0;
    }
    *out = acc;
    return 1;
}

static int evaluate(char *text, size_t len, Out *out, size_t *err_pos) {
    Parser ps;
    ps.text = text;
    ps.end = text + len;
    ps.p = text;
    ps.err_pos = 0;
    ps.failed = 0;

    while (ps.p < ps.end) {
        char *q = ps.p;
        while (*q == ' ' || *q == '\t') q++;
        if (q >= ps.end) break;
        if (*q == '\n') { ps.p = q + 1; continue; }
        if (*q == '#') {
            while (q < ps.end && *q != '\n') q++;
            ps.p = q < ps.end ? q + 1 : ps.end;
            continue;
        }

        Value v;
        next_token(&ps);
        int ok = parse_expr(&ps, &v);
        if (ok && ps.tok.type != T_EOL) ok = fail(&ps, ps.tok.start);
        if (!ok) { *err_pos = ps.err_pos; return 0; }

        char buf[64];
        int n = v.is_float ? snprintf(buf, sizeof buf, "%.15g\n", v.d)
                           : snprintf(buf, sizeof buf, "%lld\n", v.i);
        out_append(out, buf, (size_t)n);

        ps.p = (ps.tok.start < ps.end) ? (char *)ps.tok.start + 1 : ps.end;
    }
    return 1;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 1;
        fprintf(stderr, "calc: %s exists and is not a folder\n", path);
        return 0;
    }
    if (mkdir(path, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "calc: cannot create folder %s: %s\n", path, strerror(errno));
        return 0;
    }
    return 1;
}

static char *base_name(const char *path, int strip_txt) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;
    const char *start = path;
    for (size_t i = 0; i < len; i++) if (path[i] == '/') start = path + i + 1;
    size_t n = (size_t)(path + len - start);
    if (strip_txt && n > 4 && strcmp(start + n - 4, ".txt") == 0) n -= 4;
    char *r = xmalloc(n + 1);
    memcpy(r, start, n);
    r[n] = '\0';
    return r;
}

static int process_file(const char *in_path, const char *outdir) {
    size_t len;
    char *text = read_file(in_path, &len);
    if (!text) {
        fprintf(stderr, "calc: cannot read input file %s\n", in_path);
        return 1;
    }

    Out out = {NULL, 0, 0};
    size_t err_pos = 0;
    if (!evaluate(text, len, &out, &err_pos)) {
        out.len = 0;
        char buf[64];
        int n = snprintf(buf, sizeof buf, "ERROR:%zu\n", err_pos);
        out_append(&out, buf, (size_t)n);
    }
    free(text);

    char *base = base_name(in_path, 1);
    size_t plen = strlen(outdir) + strlen(base) + sizeof STUDENT_NAME + sizeof STUDENT_LASTNAME
                  + sizeof STUDENT_ID + 16;
    char *out_path = xmalloc(plen);
    snprintf(out_path, plen, "%s/%s_%s_%s_%s.txt", outdir, base,
             STUDENT_NAME, STUDENT_LASTNAME, STUDENT_ID);

    int rc = 0;
    FILE *f = fopen(out_path, "wb");
    if (!f || (out.len && fwrite(out.data, 1, out.len, f) != out.len)) {
        fprintf(stderr, "calc: cannot write output file %s\n", out_path);
        rc = 1;
    }
    if (f && fclose(f) != 0) {
        fprintf(stderr, "calc: cannot write output file %s\n", out_path);
        rc = 1;
    }
    free(out_path);
    free(base);
    free(out.data);
    return rc;
}

static int usage(void) {
    fprintf(stderr, "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return 1;
}

int main(int argc, char **argv) {
    const char *dir = NULL, *outdir = NULL, *input = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-d") == 0 || strcmp(a, "--dir") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "calc: option %s needs a value\n", a); return usage(); }
            dir = argv[++i];
        } else if (strcmp(a, "-o") == 0 || strcmp(a, "--output-dir") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "calc: option %s needs a value\n", a); return usage(); }
            outdir = argv[++i];
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "calc: unknown option %s\n", a);
            return usage();
        } else if (input) {
            fprintf(stderr, "calc: only one input file is allowed\n");
            return usage();
        } else {
            input = a;
        }
    }

    if ((dir == NULL) == (input == NULL)) return usage();


    char *default_out = NULL;
    if (!outdir) {
        char *base = base_name(dir ? dir : input, dir ? 0 : 1);
        size_t n = strlen(base) + sizeof STUDENT_NAME + sizeof STUDENT_ID + 4;
        default_out = xmalloc(n);
        snprintf(default_out, n, "%s_%s_%s", base, STUDENT_NAME, STUDENT_ID);
        free(base);
        outdir = default_out;
    }

    int rc = 0;
    if (input) {

        FILE *probe = fopen(input, "rb");
        if (!probe) {
            fprintf(stderr, "calc: cannot read input file %s\n", input);
            free(default_out);
            return 1;
        }
        fclose(probe);
        if (!ensure_dir(outdir)) rc = 1;
        else rc = process_file(input, outdir);
    } else {
        DIR *d = opendir(dir);
        if (!d) {
            fprintf(stderr, "calc: cannot open folder %s: %s\n", dir, strerror(errno));
            free(default_out);
            return 1;
        }
        if (!ensure_dir(outdir)) {
            closedir(d);
            free(default_out);
            return 1;
        }
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t nl = strlen(e->d_name);
            if (nl <= 4 || strcmp(e->d_name + nl - 4, ".txt") != 0) continue;
            size_t plen = strlen(dir) + nl + 2;
            char *path = xmalloc(plen);
            snprintf(path, plen, "%s/%s", dir, e->d_name);
            struct stat st;

            if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
                if (process_file(path, outdir) != 0) rc = 1;
            free(path);
        }
        closedir(d);
    }
    free(default_out);
    return rc;
}