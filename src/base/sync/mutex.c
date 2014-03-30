#include "mutex.h"

int mutex_init(mutex_t *mutex) {
    int rc = 0;
    pthread_mutexattr_t mattr;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;

    pthread_mutexattr_init(&mattr);
    rc = pthread_mutex_init(lock, &mattr);

    return rc;
}


int mutex_lock(mutex_t *mutex) {
    int rc = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;

    rc = pthread_mutex_lock(lock);

    return rc;
}

int mutex_trylock(mutex_t *mutex) {
    int rc = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;
    rc = pthread_mutex_trylock(lock);
    return rc;
}


int mutex_unlock(mutex_t *mutex) {
    int rc = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;
    rc = pthread_mutex_unlock(lock);
    return rc;
}


int mutex_destroy(mutex_t *mutex) {
    int rc = 0;
    pthread_mutex_t *lock = (pthread_mutex_t *)mutex;    
    rc = pthread_mutex_destroy(lock);
    return rc;
}
