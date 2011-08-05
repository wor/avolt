#include <stdio.h>
#include <stdarg.h>

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
