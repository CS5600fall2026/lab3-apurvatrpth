#include "queue.h"

// Exercise 2: implement a concurrent queue

// one lock for the queue, and a condition variable to wait when it is empty
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_not_empty = PTHREAD_COND_INITIALIZER;


// add a new task to the end of the queue
// NOTE: queue must be implemented as a monitor
void enqueue(queue_t *q, task_t *t) {
    pthread_mutex_lock(&q_lock);

    t->next = NULL;
    if (q->tail == NULL) {
        q->head = t;
    } else {
        q->tail->next = t;
    }
    q->tail = t;
    q->count++;

    pthread_cond_signal(&q_not_empty);
    pthread_mutex_unlock(&q_lock);
}

// fetch a task from the head of the queue.
// if the queue is empty, the thread should wait.
// NOTE: queue must be implemented as a monitor
task_t* dequeue(queue_t *q) {
    pthread_mutex_lock(&q_lock);

    while (q->count == 0) {
        pthread_cond_wait(&q_not_empty, &q_lock);
    }

    task_t *t = q->head;
    q->head = t->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->count--;
    t->next = NULL;

    pthread_mutex_unlock(&q_lock);
    return t;
}

// return the number of tasks in the queue.
// NOTE: queue must be implemented as a monitor
int queue_count(queue_t *q) {
    pthread_mutex_lock(&q_lock);
    int n = q->count;
    pthread_mutex_unlock(&q_lock);
    return n;
}
