#ifndef DEBUG_H_
#define DEBUG_H_
#ifdef DEBUG_
#include <stdio.h>
#include <assert.h>
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) ((void))
#endif
#endif

