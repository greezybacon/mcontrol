#ifndef CALLBACK_H
#define CALLBACK_H

#define _POSIX_C_SOURCE 200809L
#define _POSIX_SOURCE 1

#include <time.h>

typedef void *(*callback_function)(void * arg);

extern int
mcCallback(const struct timespec * when, callback_function callback,
    void * arg);

extern int
mcCallbackAbs(const struct timespec * when, callback_function callback,
    void * arg);

extern int
mcCallbackCancel(int id);

#endif
