#include "mutex.h"

int mutex_init(mutex_t *mutex) {
    int ret = 0;
    pthread_mutexattr_t mattr;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;

    pthread_mutexattr_init(&mattr);
    ret = pthread_mutex_init(lock, &mattr);

    return ret;
}


int mutex_lock(mutex_t *mutex) {
    int ret = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;

    ret = pthread_mutex_lock(lock);

    return ret;
}

int mutex_trylock(mutex_t *mutex) {
    int ret = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;
    ret = pthread_mutex_trylock(lock);
    return ret;
}


int mutex_unlock(mutex_t *mutex) {
    int ret = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;
    ret = pthread_mutex_unlock(lock);
    return ret;
}


int mutex_destroy(mutex_t *mutex) {
    int ret = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;    
    ret = pthread_mutex_destroy(lock);
    return ret;
}
