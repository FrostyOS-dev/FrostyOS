#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long spinlock_t;

#define spinlock_init(lock) (*(lock) = 0)
#define spinlock_new(name) spinlock_t name = 0

void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* _SPINLOCK_H */