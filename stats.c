
#include "stats.h"

// Exercise 3: stats implemented as a monitor.
// every function takes stats_lock on entry and releases it before return.

int n_writes = 0;  // number of writes
int n_reads = 0;   // number of reads
int n_deletes = 0; // number of deletes
int n_fails = 0;   // number of failed operations

static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;


void inc_write() {
    pthread_mutex_lock(&stats_lock);
    n_writes++;
    pthread_mutex_unlock(&stats_lock);
}

void inc_read() {
    pthread_mutex_lock(&stats_lock);
    n_reads++;
    pthread_mutex_unlock(&stats_lock);
}

void inc_delete() {
    pthread_mutex_lock(&stats_lock);
    n_deletes++;
    pthread_mutex_unlock(&stats_lock);
}

void inc_fail() {
    pthread_mutex_lock(&stats_lock);
    n_fails++;
    pthread_mutex_unlock(&stats_lock);
}


int get_writes() {
    pthread_mutex_lock(&stats_lock);
    int n = n_writes;
    pthread_mutex_unlock(&stats_lock);
    return n;
}

int get_reads() {
    pthread_mutex_lock(&stats_lock);
    int n = n_reads;
    pthread_mutex_unlock(&stats_lock);
    return n;
}

int get_deletes() {
    pthread_mutex_lock(&stats_lock);
    int n = n_deletes;
    pthread_mutex_unlock(&stats_lock);
    return n;
}

int get_fails() {
    pthread_mutex_lock(&stats_lock);
    int n = n_fails;
    pthread_mutex_unlock(&stats_lock);
    return n;
}
