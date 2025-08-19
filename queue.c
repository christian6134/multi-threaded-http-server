// Christian Garces — queue.c

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "queue.h"

// simple bounded, thread-safe queue of void* with blocking push/pop

struct queue {
    void   **buf;        // ring buffer
    int      cap;        // capacity
    int      head;       // next pop index
    int      tail;       // next push index
    int      count;      // items in queue
    pthread_mutex_t m;   // mutex
    pthread_cond_t  cv_nempty; // signaled when not empty
    pthread_cond_t  cv_nfull;  // signaled when not full
};

queue_t *queue_new(int size) {
    if (size <= 0) return NULL;
    queue_t *q = (queue_t *)calloc(1, sizeof(queue_t));
    if (!q) return NULL;
    q->buf = (void **)calloc((size_t)size, sizeof(void *));
    if (!q->buf) { free(q); return NULL; }
    q->cap = size;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->cv_nempty, NULL);
    pthread_cond_init(&q->cv_nfull, NULL);
    return q;
}

void queue_delete(queue_t **pq) {
    if (!pq || !*pq) return;
    queue_t *q = *pq;
    pthread_mutex_destroy(&q->m);
    pthread_cond_destroy(&q->cv_nempty);
    pthread_cond_destroy(&q->cv_nfull);
    free(q->buf);
    free(q);
    *pq = NULL;
}

bool queue_push(queue_t *q, void *elem) {
    if (!q) return false;
    pthread_mutex_lock(&q->m);
    while (q->count == q->cap) {
        pthread_cond_wait(&q->cv_nfull, &q->m);
    }
    q->buf[q->tail] = elem;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
    pthread_cond_signal(&q->cv_nempty);
    pthread_mutex_unlock(&q->m);
    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (!q || !elem) return false;
    pthread_mutex_lock(&q->m);
    while (q->count == 0) {
        pthread_cond_wait(&q->cv_nempty, &q->m);
    }
    *elem = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    pthread_cond_signal(&q->cv_nfull);
    pthread_mutex_unlock(&q->m);
    return true;
}

