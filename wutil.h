#ifndef WUTIL_H_INCLUDED
#define WUTIL_H_INCLUDED
// Header file for random c utility functions

// Enable debug printing
//#define _DEBUG

#ifdef DEBUG_AVOLT
#define PD_M pd
#else
#define PD_M(...)
#endif

// Simple debug printer
void pd(const char *fmt, ...);

#endif
