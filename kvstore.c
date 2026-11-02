#include "kvstore.h"


// Exercise 4: kv-store implemented as a monitor.
// one lock protects both keys[] and values[].
static pthread_mutex_t kv_lock = PTHREAD_MUTEX_INITIALIZER;

#define FREE 0
#define USED 1


// return the index of key, or -1 if not found.
// caller must hold kv_lock.
static int find_key_locked(kvstore_t *kv, char *key) {
    for (int i = 0; i < TABLE_MAX; i++) {
        if (kv->keys[i].stat == USED && strcmp(kv->keys[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

// return the index of a free slot, or -1 if table is full.
// caller must hold kv_lock.
static int find_free_locked(kvstore_t *kv) {
    for (int i = 0; i < TABLE_MAX; i++) {
        if (kv->keys[i].stat == FREE) {
            return i;
        }
    }
    return -1;
}

// make a heap copy of a string
static char* copy_str(char *s) {
    char *c = malloc(strlen(s) + 1);
    if (c != NULL) {
        strcpy(c, s);
    }
    return c;
}


/* read a key from the key-value store.
 *
 * if key doesn't exist, return NULL.
 *
 * returns a copy of the value, so another thread cannot free it while
 * the caller is still using it. caller must free() the result.
 *
 * NOTE: kv-store must be implemented as a monitor.
 */
char* kv_read(kvstore_t *kv, char *key) {
    char *ret = NULL;
    pthread_mutex_lock(&kv_lock);

    int i = find_key_locked(kv, key);
    if (i >= 0) {
        ret = copy_str(kv->values[i]);
    }

    pthread_mutex_unlock(&kv_lock);
    return ret;
}


/* write a key-value pair into the kv-store.
 *
 * - if the key exists, overwrite the old value.
 * - if key doesn't exit,
 *     -- insert one if the number of keys is smaller than TABLE_MAX
 *     -- return failure if the number of keys equals TABLE_MAX
 * - return 0 for success; return 1 for failures.
 *
 * NOTE: kv-store must be implemented as a monitor.
 */
int kv_write(kvstore_t *kv, char *key, char *val) {
    int ret = 1;
    pthread_mutex_lock(&kv_lock);

    int i = find_key_locked(kv, key);
    if (i < 0) {
        i = find_free_locked(kv);
        if (i >= 0) {
            strncpy(kv->keys[i].key, key, sizeof(kv->keys[i].key) - 1);
            kv->keys[i].key[sizeof(kv->keys[i].key) - 1] = '\0';
            kv->values[i] = NULL;
        }
    }

    if (i >= 0) {
        char *new_val = copy_str(val);
        if (new_val != NULL) {
            free(kv->values[i]);
            kv->values[i] = new_val;
            kv->keys[i].stat = USED;
            ret = 0;
        }
    }

    pthread_mutex_unlock(&kv_lock);
    return ret;
}


/* delete a key-value pair from the kv-store.
 *
 * - if the key exists, delete it.
 * - if the key doesn't exits, do nothing.
 *
 * NOTE: kv-store must be implemented as a monitor.
 */
void kv_delete(kvstore_t *kv, char *key) {
    pthread_mutex_lock(&kv_lock);

    int i = find_key_locked(kv, key);
    if (i >= 0) {
        free(kv->values[i]);
        kv->values[i] = NULL;
        kv->keys[i].key[0] = '\0';
        kv->keys[i].stat = FREE;
    }

    pthread_mutex_unlock(&kv_lock);
}


// print kv-store's contents to stdout
void kv_dump(kvstore_t *kv) {
    pthread_mutex_lock(&kv_lock);

    int n = 0;
    for (int i = 0; i < TABLE_MAX; i++) {
        if (kv->keys[i].stat == USED) {
            printf("[%d] %s = %s\n", i, kv->keys[i].key, kv->values[i]);
            n++;
        }
    }
    printf("total keys: %d\n", n);

    pthread_mutex_unlock(&kv_lock);
}
