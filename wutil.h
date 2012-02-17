// Header file for random c utility functions

// Enable debug printing
#define _DEBUG

#ifdef _DEBUG
#define PD_M pd
#else
#define PD_M(...)
#endif

// Simple debug printer
void pd(const char *fmt, ...);
