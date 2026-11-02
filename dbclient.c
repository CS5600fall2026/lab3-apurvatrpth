/*
 * db client for CS5600 Lab3
 *
 * this is a really ugly piece of code, need to clean it up before the
 * next time I assign it.
 *
 * Peter Desnoyers, 2020
 *
 * 2021 fall: modified by CS5600 staff
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <zlib.h>
#include <pthread.h>
#include <argp.h>
#include <assert.h>
#include <stdarg.h>

#include "lab3.h"

/* --------- argument parsing ---------- */

static struct argp_option options[] = {
    {"set",          'S', "KEY",  0, "set KEY to VALUE"},
    {"get",          'G', "KEY",  0, "get value for KEY"},
    {"delete",       'D', "KEY",  0, "delete KEY"},
    {"quit",         'q',  0,     0, "send QUIT command"},
    {"test",         'T', "#threads",  0, "run your concurrency test with given number of threads"},
    {0}
};

enum {OP_SET = 1, OP_GET = 2, OP_DELETE = 3, OP_QUIT = 4, OP_TEST = 5};

struct args {
    int nthreads;
    int count;
    int port;
    int max;
    int op;
    char *key;
    char *val;
    char *logfile;
    FILE *logfp;
    struct sockaddr_in addr;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct args *a = state->input;
    switch (key) {
    case ARGP_KEY_INIT:
        a->nthreads = 1;
        a->count = 1000;
        a->port = 5600;
        a->max = 200;
        a->logfp = NULL;
        break;

    case 'l':
        a->logfile = arg;
        if ((a->logfp = fopen(arg, "w")) == NULL)
            fprintf(stderr, "Error opening logfile : %s : %s\n", arg,
                    strerror(errno)), exit(1);
        break;

    case 'q':
        a->op = OP_QUIT;
        break;

    case 'G':
        a->op = OP_GET;
        if (strlen(arg) > 30)
            printf("key must be <= 30 chars\n"), argp_usage(state);
        a->key = arg;
        break;

    case 'S':
        a->op = OP_SET;
        if (strlen(arg) > 30)
            printf("key must be <= 30 chars\n"), argp_usage(state);
        a->key = arg;
        break;

    case 'T':
        a->op = OP_TEST;
        a->nthreads = atoi(arg);
        if (a->nthreads == 0) {
            printf("#threads must be a number\n"), argp_usage(state);
        }
        break;

    case 'D':
        a->op = OP_DELETE;
        if (strlen(arg) > 30)
            printf("key must be <= 30 chars\n"), argp_usage(state);
        a->key = arg;
        break;

    case 'n':
        a->count = atoi(arg); break;

    case 'p':
        a->port = atoi(arg);
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num == 0 && a->op == OP_SET) {
            a->val = arg;
        printf("val set to %s\n", arg);
    }
        else
            argp_usage(state);
        break;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, NULL, NULL};

/* --------- everything else ---------- */

int do_connect(struct sockaddr_in *addr)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0 || connect(sock, (struct sockaddr*)addr, sizeof(*addr)) < 0)
        fprintf(stderr, "can't connect: %s\n", strerror(errno)), exit(0);
    return sock;
}


void do_del(struct args *args, char *name, char *result, int quiet)
{
    int sock = do_connect(&args->addr);

    struct request rq;
    snprintf(rq.name, sizeof(rq.name), "%s", name);

    rq.op_status = 'D';
    int val = write(sock, &rq, sizeof(rq));
    if ((val = read(sock, &rq, sizeof(rq))) < 0)
        printf("DEL: REPLY: READ ERROR: %s\n", strerror(errno));
    else if (val < sizeof(rq))
        printf("DEL: REPLY: SHORT READ: %d\n", val);
    else if (rq.op_status != 'K' && !quiet)
        printf("DEL: FAILED (%c)\n", rq.op_status);
    else if (!quiet)
        printf("ok\n");

    if (result != NULL)
        *result = rq.op_status;

    close(sock);
}

void do_set(struct args *args, char *name, void *data, int len, char *result, int quiet)
{
    int sock = do_connect(&args->addr);

    struct request rq;
    snprintf(rq.name, sizeof(rq.name), "%s", name);
    int val;

    rq.op_status = 'W';
    sprintf(rq.len, "%d", len);
    write(sock, &rq, sizeof(rq));
    write(sock, data, len);
    if ((val = read(sock, &rq, sizeof(rq))) < 0)
        printf("WRITE: REPLY: READ ERROR: %s\n", strerror(errno));
    else if (val < sizeof(rq))
        printf("WRITE: REPLY: SHORT READ: %d\n", val);
    else if (rq.op_status != 'K' && !quiet)
        printf("WRITE: FAILED (%c)\n", rq.op_status);
    else if (!quiet)
        printf("ok\n");

    if (result != NULL)
        *result = rq.op_status;
    close(sock);
}

void do_quit(struct args *args)
{
    int sock = do_connect(&args->addr);
    struct request rq;
    rq.op_status = 'Q';
    write(sock, &rq, sizeof(rq));
    /* if both sides close-on-exit you won't get TIME_WAIT */
}

void do_get(struct args *args, char *name, void *data, int *len_p, char *result, int quiet)
{
    int val, sock = do_connect(&args->addr);
    struct request rq;
    snprintf(rq.name, sizeof(rq.name), "%s", name);

    rq.op_status = 'R';
    sprintf(rq.len, "%d", 0);
    write(sock, &rq, sizeof(rq));
    if ((val = read(sock, &rq, sizeof(rq))) < 0)
        printf("READ: REPLY: READ ERROR: %s\n", strerror(errno));
    else if (val < sizeof(rq))
        printf("READ: REPLY: SHORT READ: %d\n", val);
    else if (rq.op_status != 'K') {
        if (!quiet)
            printf("READ: FAILED (%c)\n", rq.op_status);
    }
    else {
        int len = atoi(rq.len);
        char buf[len];

        for (void *ptr = buf, *max = ptr+len; ptr < max; ) {
            int n = read(sock, ptr, max-ptr);
            if (n < 0) {
                printf("READ DATA: READ ERROR: %s\n", strerror(errno));
                break;
            }
            ptr += n;
        }
        if (data != NULL) {
            memcpy(data, buf, len);
            *len_p = len;
        }
        else
            printf("=\"%.*s\"\n", len, buf);
    }

    if (result != NULL)
        *result = rq.op_status;

    close(sock);
}


// === concurrency test ===
// helper functions
// (feel free to modify helper functions)

// helper function: generate random string
void randstr(char *buf, int len)
{
    for (int i = 0; i < len; i++)
        buf[i] = 'A' + (random() % 25);
}

// helper function: generate random keys
void gen_randkey(char *buf) {
    int len = random() % 30;
    randstr(buf, len);
}

// helper function: generate random values
void gen_randval(char *buf) {
    int len = random() % 4096;
    randstr(buf, len);
}

// ---- test state shared by test threads ----

#define OPS_PER_THREAD 200
#define KEYS_PER_THREAD 5
#define SHARED_KEYS 3
#define MAX_VAL 4096

int n_errors = 0;
pthread_mutex_t err_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

struct test_arg {
    struct args *a;
    int id;
    unsigned int seed;
};

void report_error(char *fmt, ...)
{
    va_list ap;
    pthread_mutex_lock(&err_lock);
    n_errors++;
    printf("ERROR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    pthread_mutex_unlock(&err_lock);
}

int get_errors()
{
    pthread_mutex_lock(&err_lock);
    int n = n_errors;
    pthread_mutex_unlock(&err_lock);
    return n;
}

// random value of length 1..MAX_VAL-1, null terminated
void make_val(char *buf, unsigned int *seed)
{
    int len = 1 + rand_r(seed) % (MAX_VAL - 1);
    for (int i = 0; i < len; i++)
        buf[i] = 'A' + rand_r(seed) % 26;
    buf[len] = '\0';
}

// read a key. returns 1 if found (value copied to out), 0 if not found
int read_key(struct args *a, char *key, char *out)
{
    int len = 0;
    char result = 0;
    do_get(a, key, out, &len, &result, 1);
    if (result != 'K')
        return 0;
    out[len] = '\0';
    return 1;
}

// run fn in n threads and wait for all of them
void run_threads(struct args *a, int n, void *(*fn)(void *))
{
    pthread_t th[n];
    struct test_arg targs[n];

    pthread_barrier_init(&barrier, NULL, n);
    for (int i = 0; i < n; i++) {
        targs[i].a = a;
        targs[i].id = i;
        targs[i].seed = 5600 + i;
        pthread_create(&th[i], NULL, fn, &targs[i]);
    }
    for (int i = 0; i < n; i++)
        pthread_join(th[i], NULL);
    pthread_barrier_destroy(&barrier);
}


// ---- test 1: each thread owns its own keys ----
// every thread keeps a local copy of what its keys should hold, does random
// read/write/delete and checks every read against the local copy.
// since no other thread touches these keys, the result must match exactly.

void *private_worker(void *p)
{
    struct test_arg *t = p;
    char keys[KEYS_PER_THREAD][32];
    char *expect[KEYS_PER_THREAD];   // NULL means key should not exist
    char *buf = malloc(MAX_VAL + 1);

    for (int i = 0; i < KEYS_PER_THREAD; i++) {
        sprintf(keys[i], "priv_t%d_k%d", t->id, i);
        do_del(t->a, keys[i], NULL, 1);   // start clean
        expect[i] = NULL;
    }

    pthread_barrier_wait(&barrier);

    for (int n = 0; n < OPS_PER_THREAD; n++) {
        int k = rand_r(&t->seed) % KEYS_PER_THREAD;
        int op = rand_r(&t->seed) % 100;
        char result = 0;

        if (op < 50) {
            int found = read_key(t->a, keys[k], buf);
            if (expect[k] == NULL && found)
                report_error("%s should not exist but read returned a value", keys[k]);
            else if (expect[k] != NULL && !found)
                report_error("%s should exist but read failed", keys[k]);
            else if (found && strcmp(buf, expect[k]) != 0)
                report_error("%s has wrong value (got len %d, want len %d)",
                             keys[k], (int)strlen(buf), (int)strlen(expect[k]));
        } else if (op < 85) {
            make_val(buf, &t->seed);
            do_set(t->a, keys[k], buf, strlen(buf), &result, 1);
            if (result != 'K') {
                report_error("write %s failed", keys[k]);
            } else {
                free(expect[k]);
                expect[k] = strdup(buf);
            }
        } else {
            do_del(t->a, keys[k], &result, 1);
            if (result != 'K')
                report_error("delete %s failed", keys[k]);
            free(expect[k]);
            expect[k] = NULL;
        }
    }

    // clean up
    for (int i = 0; i < KEYS_PER_THREAD; i++) {
        do_del(t->a, keys[i], NULL, 1);
        free(expect[i]);
    }
    free(buf);
    return NULL;
}

void test_private_keys(struct args *a, int n)
{
    printf("\n[test 1] %d threads, each with its own %d keys, %d ops each\n",
           n, KEYS_PER_THREAD, OPS_PER_THREAD);
    if (n * KEYS_PER_THREAD > TABLE_MAX) {
        printf("too many threads for this test, skipping\n");
        return;
    }
    int before = get_errors();
    run_threads(a, n, private_worker);
    printf("[test 1] done, errors: %d\n", get_errors() - before);
}


// ---- test 2: all threads hammer the same few keys ----
// we cannot know which write wins, so instead every value carries its own
// check: "writer:seq:len:" followed by len copies of one letter that depends
// on writer and seq. a read must always give back one complete value,
// never a mix of two writes or a half written value.

void make_shared_val(char *buf, int writer, int seq, unsigned int *seed)
{
    int len = 1 + rand_r(seed) % 3000;
    char c = 'a' + (writer * 7 + seq) % 26;
    int h = sprintf(buf, "%d:%d:%d:", writer, seq, len);
    memset(buf + h, c, len);
    buf[h + len] = '\0';
}

int check_shared_val(char *buf)
{
    int writer, seq, len, h = 0;
    if (sscanf(buf, "%d:%d:%d:%n", &writer, &seq, &len, &h) != 3 || h == 0)
        return 0;
    if ((int)strlen(buf) != h + len)
        return 0;
    char c = 'a' + (writer * 7 + seq) % 26;
    for (int i = h; i < h + len; i++)
        if (buf[i] != c)
            return 0;
    return 1;
}

void *shared_worker(void *p)
{
    struct test_arg *t = p;
    char *buf = malloc(MAX_VAL + 1);
    char key[32];

    pthread_barrier_wait(&barrier);

    for (int n = 0; n < OPS_PER_THREAD; n++) {
        sprintf(key, "shared_k%d", rand_r(&t->seed) % SHARED_KEYS);
        int op = rand_r(&t->seed) % 100;
        char result = 0;

        if (op < 50) {
            if (read_key(t->a, key, buf) && !check_shared_val(buf))
                report_error("%s returned a broken value", key);
        } else if (op < 90) {
            make_shared_val(buf, t->id, n, &t->seed);
            do_set(t->a, key, buf, strlen(buf), &result, 1);
            if (result != 'K')
                report_error("shared write %s failed", key);
        } else {
            do_del(t->a, key, &result, 1);
            if (result != 'K')
                report_error("shared delete %s failed", key);
        }
    }

    // after everyone is done, thread 0 deletes and the rest check it is gone
    pthread_barrier_wait(&barrier);
    if (t->id == 0) {
        for (int i = 0; i < SHARED_KEYS; i++) {
            sprintf(key, "shared_k%d", i);
            do_del(t->a, key, NULL, 1);
        }
    }
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < SHARED_KEYS; i++) {
        sprintf(key, "shared_k%d", i);
        if (read_key(t->a, key, buf))
            report_error("%s still exists after delete", key);
    }

    free(buf);
    return NULL;
}

void test_shared_keys(struct args *a, int n)
{
    printf("\n[test 2] %d threads on the same %d keys, %d ops each\n",
           n, SHARED_KEYS, OPS_PER_THREAD);
    int before = get_errors();
    run_threads(a, n, shared_worker);
    printf("[test 2] done, errors: %d\n", get_errors() - before);
}


// ---- test 3: table full ----
// fill the table until writes fail. after the first failure every new key
// must fail too, but overwriting an existing key must still work.
// other keys may already be in the server, so we only count our own.

void test_table_full(struct args *a)
{
    printf("\n[test 3] fill the table (TABLE_MAX = %d)\n", TABLE_MAX);
    int before = get_errors();
    char key[32], buf[64];
    char result;
    int ok = 0, first_fail = -1;
    int total = TABLE_MAX + 10;

    for (int i = 0; i < total; i++) {
        sprintf(key, "full_k%d", i);
        sprintf(buf, "v%d", i);
        do_set(a, key, buf, strlen(buf), &result, 1);
        if (result == 'K') {
            ok++;
            if (first_fail >= 0)
                report_error("write %s worked after table was full", key);
        } else if (first_fail < 0) {
            first_fail = i;
        }
    }
    if (first_fail < 0)
        report_error("wrote %d keys and none failed", total);
    if (ok > TABLE_MAX)
        report_error("stored %d keys, more than TABLE_MAX", ok);

    // overwrite while full
    do_set(a, "full_k0", "new", 3, &result, 1);
    if (result != 'K')
        report_error("overwrite existing key failed while table full");
    else if (!read_key(a, "full_k0", buf) || strcmp(buf, "new") != 0)
        report_error("overwrite value is wrong");

    // free one slot, then a new key should fit
    do_del(a, "full_k1", NULL, 1);
    do_set(a, "full_extra", "x", 1, &result, 1);
    if (result != 'K')
        report_error("write failed after freeing a slot");

    for (int i = 0; i < total; i++) {
        sprintf(key, "full_k%d", i);
        do_del(a, key, NULL, 1);
    }
    do_del(a, "full_extra", NULL, 1);
    printf("[test 3] stored %d keys before full, errors: %d\n",
           ok, get_errors() - before);
}


// ---- test 4: many threads fill the table at the same time ----
// each thread inserts its own new keys until the table is full.
// total successes must never go over TABLE_MAX, and every key that
// was accepted must read back with the right value.

int fill_ok = 0;
pthread_mutex_t fill_lock = PTHREAD_MUTEX_INITIALIZER;
int fill_per_thread = 0;

void *fill_worker(void *p)
{
    struct test_arg *t = p;
    char key[32], val[64], buf[MAX_VAL + 1];
    char result;
    int ok[fill_per_thread];

    pthread_barrier_wait(&barrier);

    for (int i = 0; i < fill_per_thread; i++) {
        sprintf(key, "cfill_t%d_%d", t->id, i);
        sprintf(val, "v_t%d_%d", t->id, i);
        do_set(t->a, key, val, strlen(val), &result, 1);
        ok[i] = (result == 'K');
        if (ok[i]) {
            pthread_mutex_lock(&fill_lock);
            fill_ok++;
            pthread_mutex_unlock(&fill_lock);
        }
    }

    // wait until all inserts are done, then check our own keys
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < fill_per_thread; i++) {
        sprintf(key, "cfill_t%d_%d", t->id, i);
        sprintf(val, "v_t%d_%d", t->id, i);
        int found = read_key(t->a, key, buf);
        if (ok[i] && !found)
            report_error("%s was accepted but is missing", key);
        else if (ok[i] && strcmp(buf, val) != 0)
            report_error("%s has wrong value", key);
        else if (!ok[i] && found)
            report_error("%s was rejected but exists", key);
    }

    pthread_barrier_wait(&barrier);
    for (int i = 0; i < fill_per_thread; i++) {
        sprintf(key, "cfill_t%d_%d", t->id, i);
        do_del(t->a, key, NULL, 1);
    }
    return NULL;
}

void test_concurrent_fill(struct args *a, int n)
{
    printf("\n[test 4] %d threads fill the table at the same time\n", n);
    int before = get_errors();
    fill_ok = 0;
    fill_per_thread = TABLE_MAX / n + 10;  // together they try more than TABLE_MAX

    run_threads(a, n, fill_worker);

    if (fill_ok > TABLE_MAX)
        report_error("stored %d keys, more than TABLE_MAX", fill_ok);
    if (fill_ok == 0)
        report_error("no insert worked");
    printf("[test 4] stored %d of %d tried, errors: %d\n",
           fill_ok, fill_per_thread * n, get_errors() - before);
}


// ---- test 5: key and value edge cases ----

void check_set_get(struct args *a, char *key, char *val)
{
    char result;
    char buf[MAX_VAL + 1];

    do_set(a, key, val, strlen(val), &result, 1);
    if (result != 'K')
        report_error("write [%s] failed", key);
    else if (!read_key(a, key, buf))
        report_error("read [%s] failed", key);
    else if (strcmp(buf, val) != 0)
        report_error("[%s] has wrong value", key);
}

void test_edge_cases(struct args *a)
{
    printf("\n[test 5] key and value edge cases\n");
    int before = get_errors();
    char buf[MAX_VAL + 1];

    // longest allowed key (30 chars)
    char long_key[] = "abcdefghijklmnopqrstuvwxyz0123";
    check_set_get(a, long_key, "long key");

    // keys with odd characters
    char *odd_keys[] = {"xi,0xw32x0s>J%", "KEY-0", "a b c", "!@#$^&*()"};
    for (int i = 0; i < 4; i++)
        check_set_get(a, odd_keys[i], odd_keys[i]);

    // keys that are prefixes of each other must stay separate
    check_set_get(a, "ab", "short");
    check_set_get(a, "abc", "longer");
    if (!read_key(a, "ab", buf) || strcmp(buf, "short") != 0)
        report_error("key ab was changed by writing abc");

    // overwrite a long value with a short one, no old bytes should remain
    char big[MAX_VAL];
    memset(big, 'Z', MAX_VAL - 1);
    big[MAX_VAL - 1] = '\0';
    check_set_get(a, "edge_val", big);
    check_set_get(a, "edge_val", "tiny");

    // deleting a missing key is fine and must not affect others
    char result;
    do_del(a, "edge_missing", &result, 1);
    if (result != 'K')
        report_error("delete of missing key failed");
    if (!read_key(a, "abc", buf) || strcmp(buf, "longer") != 0)
        report_error("delete of missing key changed another key");

    // key is case sensitive
    check_set_get(a, "Case", "upper");
    if (read_key(a, "case", buf))
        report_error("keys should be case sensitive");

    // clean up
    do_del(a, long_key, NULL, 1);
    for (int i = 0; i < 4; i++)
        do_del(a, odd_keys[i], NULL, 1);
    do_del(a, "ab", NULL, 1);
    do_del(a, "abc", NULL, 1);
    do_del(a, "edge_val", NULL, 1);
    do_del(a, "Case", NULL, 1);
    printf("[test 5] done, errors: %d\n", get_errors() - before);
}


/*
 * Exercise 5:
 * Write your own concurrency tests.
 *
 * You should (these are *suggestions* not requirements):
 *
 *  - create multiple threads and send concurrent operations to dbserver
 *
 *  - try to produce unnatural schedules and interleaving too
 *
 *  - you don't have to follow the six rules or always use monitor here
 *    because this is for debugging purpose. For example,
 *
 *    -- you can use sleep,
 *
 *    -- and feel free to use synchronization primitives (e.g., mutexes)
 *    whatever way you like
 *
 *  - to check correctness, we suggest you to compare your writes and check
 *  the return values from reads. How? remember your write values and compare
 *  the values when you read from the same keys.
 *
 *  - hints:
 *    -- read "do_get/do_set/do_del" to see how to issue these operations
 *    -- you can use the above helper functions to generate keys/values
 *    -- if you want share data between multiple threads, protect the shared data with mutex!
 *       (note: you will not lose points due to coding style in Exercise 5.)
 */
void do_test(struct args *a)
{
    // below is a simple single-threaded test
    char data[4096];
    int len = 4096;
    char result;

    do_set(a, "key0", "val0", strlen("val0"), NULL, 1);
    do_get(a, "key0", &data, &len, NULL, 0);
    data[len] = '\0';    // why we add this? for strcmp
    assert (strcmp(data,"val0") == 0);

    do_del(a, "key0", NULL, 1);
    printf("Expected to see: \"READ: FAILED (X)\" below\n");
    do_get(a, "key0", &data, &len, &result, 0); // should have a READ ERROR
    data[len] = '\0';
    assert (result == 'X');


    // fix random seed; easier to reproduce your bugs
    srandom(5600);

    int num_threads = a->nthreads;
    printf("running tests with %d threads\n", num_threads);

    test_private_keys(a, num_threads);
    test_shared_keys(a, num_threads);
    test_table_full(a);
    test_concurrent_fill(a, num_threads);
    test_edge_cases(a);

    printf("\n==== total errors: %d ====\n", get_errors());
    if (get_errors() == 0)
        printf("ALL TESTS PASSED\n");
}



int main(int argc, char **argv)
{
    struct args args;
    memset(&args, 0, sizeof(args));

    argp_parse(&argp, argc, argv, 0, 0, &args);

    args.addr = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(args.port),
        .sin_addr.s_addr = inet_addr("127.0.0.1")}; /* localhost */

    if (args.op == OP_SET)
        do_set(&args, args.key, args.val, strlen(args.val), NULL, 0);
    else if (args.op == OP_GET)
        do_get(&args, args.key, NULL, NULL, NULL, 0);
    else if (args.op == OP_DELETE)
        do_del(&args, args.key, NULL, 0);
    else if (args.op == OP_QUIT)
        do_quit(&args);
    else if (args.op == OP_TEST)
        do_test(&args);
    else
        printf("See usage: \"dbclient --help\"\n");
}

