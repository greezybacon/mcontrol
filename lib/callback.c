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
static pthread_t timer_thread_id = (pthread_t) 0;
static pthread_mutex_t callback_list_lock = PTHREAD_MUTEX_INITIALIZER;

static const int TIMER_SIGNAL = SIGUSR1;

static void
_mcCallbackListPop(void) {
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
}

static void
_NoopSignalHandler(int signo) {}

static void *
_mcCallbackThread(void *arg) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, TIMER_SIGNAL);

    struct sigaction sa = { .sa_handler = _NoopSignalHandler };
    sigaction(TIMER_SIGNAL, &sa, NULL);

    // Block the delivery of the timer signal to this thread. It should only
    // be received at times when expected
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    int signal, status;
    struct timespec now;
    while (true) {
        // Adjust timer time
        signal = 0;
        if (callbacks) {
            // Await delivery of the timer signal to this thread
            pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
            status = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME,
                &callbacks->abstime, NULL);
            pthread_sigmask(SIG_BLOCK, &mask, NULL);
            if (status == EINTR)
                // SIGINT means callbacks have changed
                continue;
            else if (status == EINVAL)
                // callbacks->abstime is invalid. It should be removed
                // from the list
                _mcCallbackListPop();
            // Continue onward with double-checking the clock
        }
        // Wait for the SIGINT signal
        else if (sigwait(&mask, &signal) != 0)
            // TODO: Display/recover from error here
            break;
        else if (signal == TIMER_SIGNAL)
            // TIMER_SIGNAL is used to signal changes to the callback list
            // and should just result in the timer_settime call and another
            // wait.
            continue;

        if (callbacks == NULL)
            continue;

        // Double check the time
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec < callbacks->abstime.tv_sec
                || (now.tv_sec == callbacks->abstime.tv_sec
                    && now.tv_nsec < callbacks->abstime.tv_nsec))
            // Timer expiration is still in the future
            continue;

        int id = callbacks->id;
        // Perform the callback
        callbacks->callback(callbacks->arg);

        // Remove the HEAD entry from the callback list
        mcCallbackDequeue(id);
    }
    // Signal that the callback thread should be restarted
    timer_thread_id = 0;

    return NULL;
}

/**
 * mcCallbackAbs
 *
 * Client- or server-side callback registry. Enables a connected application
 * to request a function to be called with a given argument at some time in
 * the future.
 *
 * Parameters:
 * when (struct timespec *) - Absolute time in unix epoch when the given
 *      callback should be called back
 * callback - (callback_funcion) Function to be called in the future
 * arg - (void *) Argument to be passed to the function
 *
 * Returns:
 * (int) - if > 0, id number of callback, which should be passed to
 * mcCallbackCancel() if the callback is to be canceled. -EINVAL if invalid
 * pointer was passed.
 */
int
mcCallbackAbs(const struct timespec * when, callback_function callback,
        void * arg) {

    if (when == NULL || callback == NULL)
        return -EINVAL;

    else if (when->tv_nsec < 0 || when->tv_nsec > (int)1e9
            || when->tv_sec < 0)
        return -EINVAL;

    // Setup the struct to hold the callback information
    struct callback_list * info = calloc(1, sizeof(struct callback_list));
    *info = (struct callback_list) {
        .abstime = *when,
        .callback = callback,
        .arg = arg,
        .id = ++callback_uid
    };

    pthread_mutex_lock(&callback_list_lock);

    struct callback_list * current = callbacks, * tail = NULL;
    while (current) {
        if (current->abstime.tv_sec > when->tv_sec
                || (current->abstime.tv_sec == when->tv_sec
                    && current->abstime.tv_nsec > when->tv_nsec)) {
            // New callback occurs before this one. Insert the callback
            // here
            info->next = current;
            if (current->prev) {
                info->prev = current->prev;
                current->prev->next = info;
            }
            current->prev = info;
            if (current == callbacks)
                callbacks = info;
            break;
        }
        tail = current;
        current = current->next;
    }
    // End of list
    if (!current) {
        if (tail) {
            tail->next = info;
            info->prev = tail;
        }
        // Empty list?
        else if (!callbacks)
            callbacks = info;
    }

    pthread_mutex_unlock(&callback_list_lock);

    // Setup callback thread
    if (timer_thread_id == 0) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&timer_thread_id, &attr, _mcCallbackThread, NULL);
    }
    else
        // Signal the thread of callback changes (a new thread doesn't need
        // to be signaled)
        pthread_kill(timer_thread_id, TIMER_SIGNAL);

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

static int
mcCallbackDequeue(int callback_id) {
    pthread_mutex_lock(&callback_list_lock);

    struct callback_list * current = callbacks;
    while (current) {
        if (current->id == callback_id) {
            // Remove the link from the list -- middle of the list
            if (current->prev && current->next) {
                current->prev->next = current->next;
                current->next->prev = current->prev;
            }
            // end of the list
            else if (current->prev) {
                current->prev->next = NULL;
            }
            // beginning of a list with two or more items
            else if (current == callbacks && current->next) {
                callbacks = current->next;
                callbacks->prev = NULL;
            }
            // beginning and only item in the list
            else if (current == callbacks)
                callbacks = NULL;

            free(current);
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&callback_list_lock);
    return (current != NULL) ? 0 : EINVAL;
}

int
mcCallbackCancel(int callback_id) {
    int status = mcCallbackDequeue(callback_id);
    if (status)
        return status;

    // Signal the timer thread of the changes
    pthread_kill(timer_thread_id, TIMER_SIGNAL);

    return 0;
}
