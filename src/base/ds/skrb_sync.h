#ifndef _HPIO_SKRB_SYNC_
#define _HPIO_SKRB_SYNC_

#include <stdint.h>
#include <inttypes.h>
#include "base.h"
#include "skrb.h"
#include "sync/spin.h"
#include "sync/mutex.h"

#define skrb_mutex_empty(tree, mutex) ({	\
	    int __empty = false;		\
	    mutex_lock(mutex);			\
	    __empty = skrb_empty(tree);		\
	    mutex_unlock(mutex);		\
	    __empty;				\
	})

#define skrb_mutex_min(tree, mutex) ({		\
	    skrb_node_t *__node = NULL;		\
	    mutex_lock(mutex);			\
	    __node = skrb_min(tree);		\
	    mutex_unlock(mutex);		\
	    __node;				\
	})

#define skrb_mutex_max(tree, mutex) ({		\
	    skrb_node_t *__node = NULL;		\
	    mutex_lock(mutex);			\
	    __node = skrb_max(tree);		\
	    mutex_unlock(mutex);		\
	    __node;				\
	})

#define skrb_mutex_insert(tree, node, mutex) do {	\
	mutex_lock(mutex);				\
	skrb_insert(tree, node);			\
	mutex_unlock(mutex);				\
    } while (0)

#define skrb_mutex_delete(tree, node, mutex) do {	\
	mutex_lock(mutex);				\
	skrb_delete(tree, node);			\
	mutex_unlock(mutex);				\
    } while (0)


#define skrb_spin_empty(tree, spin) ({		\
	    int __empty = false;		\
	    spin_lock(spin);			\
	    __empty = skrb_empty(tree);		\
	    spin_unlock(spin);			\
	    __empty;				\
	})

#define skrb_spin_min(tree, spin) ({		\
	    skrb_node_t *__node = NULL;		\
	    spin_lock(spin);			\
	    __node = skrb_min(tree);		\
	    spin_unlock(spin);			\
	    __node;				\
	})

#define skrb_spin_max(tree, spin) ({		\
	    skrb_node_t *__node = NULL;		\
	    spin_lock(spin);			\
	    __node = skrb_max(tree);		\
	    spin_unlock(spin);			\
	    __node;				\
	})

#define skrb_spin_insert(tree, node, spin) do {	\
	spin_lock(spin);			\
	skrb_insert(tree, node);		\
	spin_unlock(spin);			\
    } while (0)

#define skrb_spin_delete(tree, node, spin) do {	\
	spin_lock(spin);			\
	skrb_delete(tree, node);		\
	spin_unlock(spin);			\
    } while (0)

#endif /* _H_SKRB_ */
