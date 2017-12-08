
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "pg.h"

typedef struct
{
    double      resolution;
    uint64_t    base;
} PG_TIME_T;
static PG_TIME_T posix_time;


static uint64_t _get_raw_time(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * (uint64_t) 1000000000 + (uint64_t) ts.tv_nsec;
}

double pg_get_time(void)
{
    return (double) (_get_raw_time() - posix_time.base) * posix_time.resolution;
}

void pg_set_time(double time)
{
    if (time != time || time < 0.0 || time > 18446744073.0)
    {
        printf("Invalid time\n");
        return;
    }

    posix_time.base = _get_raw_time() - (uint64_t) (time / posix_time.resolution);
}

void pg_time_init(void)
{
    posix_time.resolution = 1e-9;

    posix_time.base = _get_raw_time();
}

