#include "spin.h"

int spin_init(spin_t *spin) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)spin;
    return pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);
}


int spin_lock(spin_t *spin) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)spin;
    return pthread_spin_lock(lock);
}

int spin_unlock(spin_t *spin) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)spin;
    return pthread_spin_unlock(lock);
}


int spin_destroy(spin_t *spin) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)spin;    
    return pthread_spin_destroy(lock);
}
