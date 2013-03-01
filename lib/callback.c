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
static pthread_cond_t callback_list_changed;

static void
_mcCallbackListPop(void) {
    if (callbacks->next) {
        callbacks = callbacks->next;
        free(callbacks->prev);
        callbacks->prev = NULL;
    }
    else {
        free(callbacks);
        callbacks = NULL;
    }
}

static int
mcCallbackDequeue(int id);

/**
 * _mcCallbackThread
 *
 * Thread function which will await for the callback list to be changed or
 * wait for the absolute time specified in the first item in the list (the
 * head) to be arrive.
 *
 * If the list has at least one item, and the current time is at or after
 * the absolute time specified in the callback item, the callback function
 * will be invoked. Thereafter, the process continues with the first
 * paragraph.
 *
 * Context:
 * callback_list_lock - (pthread_mutex_t) Lock to use when dropping items
 *      off the list
 * callback_list_changed - (pthread_cond_t) To be signaled when the list is
 *      changed
 * callbacks - (struct callback_list *) (Doubly-linked) List of active
 *      callbacks
 *
 * Returns:
 * Never. Bails only on exception. Call in a separate thread.
 */
static void *
_mcCallbackThread(void *arg) {
    pthread_cond_init(&callback_list_changed, NULL);

    pthread_mutex_lock(&callback_list_lock);

    int status;
    struct timespec now;
    while (true) {
        if (callbacks) {
            // Await for signal or timeout
            status = pthread_cond_timedwait(&callback_list_changed,
                &callback_list_lock, &callbacks->abstime);

            if (status == EINVAL)
                // callbacks->abstime is invalid. It should be removed
                // from the list
                _mcCallbackListPop();
            else if (status != ETIMEDOUT)
                // Condition was sent. Don't callback
                continue;
            // Continue onward with double-checking the clock
        }
        if (callbacks == NULL) {
            // Await the signal of a new callback
            pthread_cond_wait(&callback_list_changed,
                &callback_list_lock);
            continue;
        }

        // Double check the time
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec < callbacks->abstime.tv_sec
                || (now.tv_sec == callbacks->abstime.tv_sec
                    && now.tv_nsec < callbacks->abstime.tv_nsec))
            // Timer expiration is still in the future
            continue;

        int id = callbacks->id;

        // Perform the callback with the list unlocked
        pthread_mutex_unlock(&callback_list_lock);
        callbacks->callback(callbacks->arg);
        pthread_mutex_lock(&callback_list_lock);

        // Drop the callback item we just executed
        mcCallbackDequeue(id);
    }
    pthread_cond_destroy(&callback_list_changed);
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
        pthread_cond_signal(&callback_list_changed);

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

    return (current != NULL) ? 0 : EINVAL;
}

int
mcCallbackCancel(int callback_id) {
    pthread_mutex_lock(&callback_list_lock);
    int status = mcCallbackDequeue(callback_id);
    pthread_mutex_unlock(&callback_list_lock);

    if (status)
        return status;

    // Signal the timer thread of the changes
    pthread_cond_signal(&callback_list_changed);

    return 0;
}
