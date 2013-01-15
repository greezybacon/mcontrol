/*
 * schedule.c
 *
 * Various scheduling routines which will be used to decide which thread an
 * incoming message should be associated with.
 */

#include "scheduler.h"

#include "worker.h"

/**
 * ScheduleLeastBusy
 *
 * Add the request message to the work queue of the worker with the shortest
 * work queue length. If any worker is completely free (and marked
 * available), it will receive the message
 */
static int
ScheduleLeastBusy(request_message_t * message) {
    // Find an available worker
    int shortest_queue = 10000;
    Worker * least_busy, * worker = AllWorkers;
    for (int i = 0; i < MAX_WORKERS; worker++, i++) {
        if (!worker->thread_id)
            continue;

        if (worker->available)
            return WorkerEnqueue(worker, message);
        else {
            if (worker->queue_length < shortest_queue) {
                shortest_queue = worker->queue_length;
                least_busy = worker;
            }
        }
    }
    
    // No available workers
    if (least_busy)
        return WorkerEnqueue(least_busy, message);

    // Unable to queue -- no workers
    return -1;
}

/**
 * ScheduleDriverGroup
 *
 * Drivers export a group number which will help correlate drivers which
 * will block together. For instance, drivers which share a common serial
 * port will block together. This scheduler will place the message on a
 * worker queue based on the driver group the work is targeted at.
 */
static int
ScheduleDriverGroup(request_message_t * message) {
    return -1;
}

static Scheduler current_scheduler = NULL;

int
GitRDone(request_message_t * message) {
    return current_scheduler(message);
}

int
SchedulerConfigure(void) {
    // TODO: Pass an argument or two to this function to indicate which
    //       scheduler should be used. This will make it configurable if
    //       necessary be config file or command line option.
    //
    //  For now, use the least-busy scheduler which will add the message to
    //  the work queue of the least-busy worker -- the one with the shortest
    //  work queue length.
    current_scheduler = ScheduleLeastBusy;

    // Start up a few workers
    for (int i=0; i<4; i++)
        DaemonWorkerAdd();

    return 0;
}
