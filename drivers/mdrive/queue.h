#ifndef QUEUE
#define QUEUE

typedef struct queue queue_t;
struct queue {
    int                 size;       // Max number of items
    int                 length;     // # of items queued
    mdrive_response_t ** head;
};

extern void mdrive_response_queue_init(queue_t *);
extern void queue_push(queue_t *, mdrive_response_t *);
extern mdrive_response_t * queue_peek(queue_t *);
extern mdrive_response_t * queue_pop(queue_t *);
extern int queue_length(queue_t *);
extern void queue_flush(queue_t *);

#endif
