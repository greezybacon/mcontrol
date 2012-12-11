#include "mdrive.h"
#include "queue.h"

#include <stdio.h>

void
mdrive_response_queue_init(queue_t * queue) {
    queue->head = calloc(16, sizeof *queue->head);
    queue->size = 16;
    queue->length = 0;
}

void
queue_push(queue_t * queue, mdrive_response_t * response) {
    if (queue->size - queue->length <= 0) {
        // XXX: Perhaps resize the queue under some conditions ?
        mcTraceF(1, MDRIVE_CHANNEL_RX, "!!! Unable to queue response: %d, %d",
            queue->size, queue->length);
        return;
    }
    queue->head[queue->length++] = response;
}

mdrive_response_t *
queue_peek(queue_t * queue) {
    if (queue->length < 1)
        return NULL;
    return queue->head[queue->length-1];
}

mdrive_response_t *
queue_pop(queue_t * queue) {
    if (queue->length < 1)
        return NULL;
    return queue->head[--queue->length];
}

int
queue_length(queue_t * queue) {
    if (queue == NULL)
        return 0;
    return queue->length;
}

void
queue_flush(queue_t * queue) {
    while (queue_length(queue))
        free(queue_pop(queue));
}
