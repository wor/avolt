#include <stdio.h>
#include <stdarg.h>
#include <time.h>

//void pd(int priority, const char *fmt, ...)
void pd(const char *fmt, ...)
{
    // TODO: Can we get filename to debug message
    printf("DEBUG: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int nsleep(int nanoseconds) {
    // TODO: now only works for times under a second
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = nanoseconds;
    return nanosleep(&sleeptime, NULL);
}
