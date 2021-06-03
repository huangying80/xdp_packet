#include <stdio.h>
#include <string.h>
#include "tokenbucket.h"
int TokenBucket::lastSlot = 0;
TokenBucket::Bucket TokenBucket::buckets[MAX_BUCKETS];
volatile bool TokenBucket::running = true;
pthread_mutex_t TokenBucket::mutex = PTHREAD_MUTEX_INITIALIZER;
__thread int TokenBucket::slot = -1;
pthread_t TokenBucket::tid = -1;

TokenBucket::TokenBucket(long rate, long limit, long token)
{
    if (!rate) {
        running = false;
        return;
    }
    pthread_mutex_lock(&mutex);
    buckets[lastSlot].rate = rate;
    buckets[lastSlot].limit = limit;
    buckets[lastSlot].token = token;
    pthread_mutex_init(&buckets[lastSlot].mutex, NULL);
    pthread_cond_init(&buckets[lastSlot].cond, NULL);
    slot = lastSlot;
    lastSlot++;
    pthread_mutex_unlock(&mutex);
}

long TokenBucket::fetchToken(long size)
{
    long n;
    if (slot < 0) {
        return size;
    }
    if (size <= 0) {
        return -1;
    }
    pthread_mutex_lock(&buckets[slot].mutex);
    while (buckets[slot].token <= 0) {
        pthread_cond_wait(&buckets[slot].cond, &buckets[slot].mutex);
    }
    
    n = buckets[slot].token < size ? buckets[slot].token : size;
    buckets[slot].token -= n;
    pthread_mutex_unlock(&buckets[slot].mutex);
    return n;
}

long TokenBucket::returnToken(long size)
{
    if (size <= 0) {
        return -1;
    }
    pthread_mutex_lock(&buckets[slot].mutex);
    buckets[slot].token += size;
    if (buckets[slot].token > buckets[slot].limit) {
        buckets[slot].token = buckets[slot].limit;
    }
    pthread_cond_broadcast(&buckets[slot].cond);
    pthread_mutex_unlock(&buckets[slot].mutex);
    return size;
}

int TokenBucket::start(void)
{
    int err;
    err = pthread_create(&tid, NULL, updateToken, NULL);
    if (err) {
        fprintf(stderr, "create TokenBucket failed, err %s\n", strerror(err));
        return -1;
    }
    return 0;
}

void TokenBucket::stop(void)
{
    running = false;
}

void *TokenBucket::updateToken(void *args)
{
    while (running) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < lastSlot; i++) {
            pthread_mutex_lock(&buckets[i].mutex);
            buckets[i].token += buckets[i].rate;
            if (buckets[i].token > buckets[i].limit) {
                buckets[i].token = buckets[i].limit;
            }
            pthread_cond_broadcast(&buckets[i].cond);
            pthread_mutex_unlock(&buckets[i].mutex);

        }
        pthread_mutex_unlock(&mutex);
        sleep(1);
    }
    printf("rate-limiting disable\n");
    return NULL;
}
