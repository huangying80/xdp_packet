#ifndef _TOKEN_BUCKET_H_
#define _TOKEN_BUCKET_H_
#include <unistd.h>
#include <pthread.h>
#define MAX_BUCKETS 128
class TokenBucket {
private:
    struct Bucket {
        volatile long rate;
        volatile long limit;
        volatile long token;
        pthread_mutex_t mutex;
        pthread_cond_t  cond;
    };
    static pthread_mutex_t mutex;
    static int lastSlot; 
    static Bucket buckets[MAX_BUCKETS];
    volatile static bool running;
    static __thread int slot;
    static pthread_t tid;

    TokenBucket() {};
    static void* updateToken(void *args);

public:
    TokenBucket(long rate, long limit, long token);
    long fetchToken(long size);
    long returnToken(long size);
    static int start(void);
    static void stop(void);
};
#endif
