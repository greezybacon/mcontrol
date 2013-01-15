#include "worker.h"

#include "../lib/dispatch.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

struct worker * AllWorkers;

static void *
DaemonWorkerThread(void * arg) {
    Worker * self = arg;

    // Queue is locked except when waiting for the signal and doing the work
    pthread_mutex_lock(&self->queue_lock);

    while (true) {
        if (self->queue_head) {
            request_message_t * message = &self->queue_head->message;
            
            printf("Received some work\n");
            pthread_mutex_unlock(&self->queue_lock);
            if (message->type > TYPE__FIRST)
                mcDispatchProxyMessage(message);
            pthread_mutex_lock(&self->queue_lock);

            WorkerDequeue(self);
        }
        // Wait for the work-queued signal
        else if (pthread_cond_wait(&self->signal, &self->queue_lock))
            // TODO: Display/recover from error here
            break;
    }

    self->thread_id = 0;

    pthread_mutex_unlock(&self->queue_lock);
    pthread_mutex_destroy(&self->queue_lock);
    pthread_cond_destroy(&self->signal);

    return NULL;
}

int
DaemonWorkerInit(Worker * worker) {
    *worker = (Worker) {
        .available = true,
    };
    pthread_mutex_init(&worker->queue_lock, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_cond_init(&worker->signal, NULL);

    return pthread_create(&worker->thread_id, &attr, DaemonWorkerThread,
        worker);
}

int
DaemonWorkerAdd(void) {
    Worker * worker = AllWorkers;
    for (int i = 0; i < MAX_WORKERS; worker++, i++)
        if (!worker->thread_id)
            return DaemonWorkerInit(worker);

    return E2BIG;
}

int
WorkerEnqueue(Worker * worker, request_message_t * message) {

    struct work_q_item * work = calloc(1, sizeof(struct work_q_item));
    *work = (struct work_q_item) {
        .message = *message
    };

    if (!worker->queue_head)
        worker->queue_head = work;
    else {
        struct work_q_item * q = worker->queue_head;
        while (q->next) q = q->next;
        q->next = work;
    }
    worker->queue_length++;
    worker->available = false;


    // Signal worker thread of new work
    if (worker->queue_length == 1)
        pthread_cond_signal(&worker->signal);

    return 0;
}

int
WorkerDequeue(Worker * worker) {
    if (!worker->queue_head)
        return EINVAL;

    struct work_q_item * dropped = worker->queue_head;

    if (worker->queue_head->next)
        worker->queue_head = worker->queue_head->next;
    else {
        worker->queue_head = NULL;
        worker->available = true;
    }
    worker->queue_length--;

    free(dropped);

    return 0;
}

static void __attribute__((constructor))
_WorkerListInit(void) {
    AllWorkers = calloc(MAX_WORKERS+1, sizeof(Worker));
}
