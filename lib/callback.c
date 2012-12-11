/**
 * callback.c
 *
 * Single threaded timer callback handler. mcCallback and mcCallbackAbs are
 * used to schedule functions to be called back at some prescriptive time.
 * The function and callback time will be registered and the function will
 * be called back when the specified time arrives. Use mcCallbackCancel to
 * cancel the call if needed.
 *
 * Be nice in the defined callback functions. Because this implementation is
 * single-threaded, spending loads of time in your callback function will
 * hold up the other registered callback functions as the next callback
 * cannot be handled until after the return of the current callback. If you
 * needs lots of processing time, spawn a thread or signal a condition to
 * another thread to perform the processing.
 */
#include "callback.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

typedef struct callback_list callback_list_t;
struct callback_list {
    struct timespec     abstime;
    callback_function   callback;
    void *              arg;
    int                 id;

    callback_list_t *   next;
    callback_list_t *   prev;
};

static int callback_uid = 0;
static struct callback_list * callbacks = NULL;
static timer_t callback_timer = (timer_t) 0;
static pthread_t timer_thread_id = (pthread_t) 0;
static pthread_mutex_t callback_list_lock;

static const int TIMER_SIGNAL = SIGUSR2;

static void *
_mcCallbackThread(void *arg) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);

    // Block the delivery of the timer signal to this thread
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    int signal;
    while (true) {
        // Await delivery of the timer signal to this thread
        // TODO: Display/recover from error here
        if (sigwait(&mask, &signal) != 0) break;

        // Perform the callback
        if (callbacks == NULL)
            continue;
        else
            callbacks->callback(callbacks->arg);

        // Remove the HEAD entry from the callback list
        pthread_mutex_lock(&callback_list_lock);

        if (callbacks->next) {
            callbacks = callbacks->next;
            free(callbacks->prev);
            callbacks->prev = NULL;
        }
        else {
            free(callbacks);
            callbacks = NULL;
        }

        pthread_mutex_unlock(&callback_list_lock);
        struct itimerspec exp = { .it_value = callbacks->abstime };
        timer_settime(callback_timer, TIMER_ABSTIME, &exp, NULL);
    }
    return NULL;
}

int
mcCallbackAbs(const struct timespec * when, callback_function callback,
    void * arg) {

    // Setup the struct to hold the callback information
    struct callback_list * info = calloc(1, sizeof(struct callback_list));
    *info = (struct callback_list) {
        .abstime = *when,
        .callback = callback,
        .arg = arg,
        .id = ++callback_uid
    };

    pthread_mutex_lock(&callback_list_lock);

    struct callback_list * current = callbacks, * tail;
    while (current) {
        if ((current->abstime.tv_sec > when->tv_sec)
                && (current->abstime.tv_nsec > when->tv_nsec)) {
            // New callback occurs before this one. Insert the callback
            // here
            info->next = current;
            info->prev = current->prev;
            if (current->next)
                current->next->prev = info;
            if (current->prev)
                current->prev->next = info;
            break;
        }
        tail = current;
        current = current->next;
    }
    // End of list
    if (tail) {
        tail->next = info;
        info->prev = tail;
    }
    // Empty list?
    else 
        callbacks = info;

    pthread_mutex_unlock(&callback_list_lock);

    // Setup callback thread
    if (timer_thread_id == 0) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&timer_thread_id, &attr, _mcCallbackThread, NULL);
    }

    // Setup the timer
    if (callback_timer == 0) {
        struct sigevent timer_ev = {
            .sigev_notify = SIGEV_THREAD_ID,
            .sigev_signo = TIMER_SIGNAL
        };
        timer_create(CLOCK_REALTIME, &timer_ev, &callback_timer);
    }

    // Adjust timer time
    struct itimerspec exp = { .it_value = callbacks->abstime };
    timer_settime(callback_timer, TIMER_ABSTIME, &exp, NULL);

    return info->id;
}

int
mcCallback(const struct timespec * when, callback_function cb,
        void * arg) {

    struct timespec now, then;
    clock_gettime(CLOCK_REALTIME, &now);
    tsAdd(&now, when, &then);

    return mcCallbackAbs(&then, cb, arg);
}

int
mcCallbackCancel(int callback_id) {
    pthread_mutex_lock(&callback_list_lock);

    struct callback_list * current = callbacks;
    while (current) {
        if (current->id == callback_id) {
            // Remove the link from the list
            if (current->prev && current->next)
                current->prev->next = current->next;
            else if (current->prev)
                current->prev = NULL;
            else if (current == callbacks && current->next)
                callbacks = current->next;
            else
                callbacks = NULL;

            free(current);
            break;
        }
        current = current->next;
    }

    pthread_mutex_lock(&callback_list_lock);

    return (current != NULL) ? 0 : EINVAL;
}

void __attribute__((constructor))
__mcCallbackInit(void) {
    pthread_mutex_init(&callback_list_lock, NULL);
}
