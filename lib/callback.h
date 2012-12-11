#ifndef CALLBACK_H
#define CALLBACK_H

#include <time.h>

typedef void *(*callback_function)(void * arg);


extern int
mcCallbackAbs(const struct timespec * when, callback_function callback,
    void * arg);

#endif
