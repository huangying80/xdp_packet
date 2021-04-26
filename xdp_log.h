/*
 * huangying, email: hy_gzr@163.com
 */

#ifndef _XDP_LOG_H_
#define _XDP_LOG_H_

#include <stdio.h>
#include <stdlib.h>


#if defined XDP_DEBUG

#define DEBUG_OUT(format, args...) \
fprintf(stderr, "[DEBUG] ");  \
fprintf(stderr, format, ##args); \
fprintf(stderr, "\n")

#elif defined XDP_DEBUG_VERBOSE

#define DEBUG_OUT(format, args...) \
    fprintf(stderr, "[%s:%d %s DEBUG] ", __FILE__, __LINE__, __func__); \
    fprintf(stderr, format, ##args);\
    fprintf(stderr, "\n")
#else

#define DEBUG_OUT(format, args...) do{}while(0)

#endif

#ifdef XDP_ERROR_VERBOSE

#define XDP_PANIC(format, args...) \
    fprintf(stderr, "[%s:%d %s OOPS] ", __FILE__, __LINE__, __func__); \
    fprintf(stderr, format, ##args);\
    fprintf(stderr, "\n"); \
    abort()

#define ERR_OUT(format, args...) \
    fprintf(stderr, "[%s:%d %s ERR] ", __FILE__, __LINE__, __func__); \
    fprintf(stderr, format, ##args);\
    fprintf(stderr, "\n")

#else

#define ERR_OUT(format, args...) \
fprintf(stderr, "[ERR] ");  \
fprintf(stderr, format, ##args); \
fprintf(stderr, "\n")

#define XDP_PANIC(format, args...) \
fprintf(stderr, "[OOPS] "); \
fprintf(stderr, format, ##args); \
fprintf(stderr, "\n");\
abort()

#endif

#endif
