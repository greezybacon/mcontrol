#ifndef WORKER_H
#define WORKER_H

#include "../lib/message.h"

typedef struct work_q_item q_item_t;
struct work_q_item {
    request_message_t   message;
    q_item_t *          next;
};

typedef struct worker Worker;
struct worker {
    struct work_q_item * queue_head;
    pthread_mutex_t     queue_lock;
    pthread_cond_t      signal;
    int                 queue_length;
    pthread_t           thread_id;

    // Used by the driver-group scheduler
    int                 group;
};

static const int MAX_WORKERS = 16;
extern struct worker * AllWorkers;

extern int
WorkerEnqueue(Worker * worker, request_message_t * message);

static int
WorkerDequeue(Worker * worker);

extern int
DaemonWorkerAdd(void);

#endif
