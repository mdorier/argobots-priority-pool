#ifndef ABT_POOL_PRIO_H
#define ABT_POOL_PRIO_H

#include <abt.h>

#ifdef __cplusplus
extern "C" {
#endif

int ABT_pool_prio_wait_def_create(ABT_pool_user_def* def);

int ABT_pool_prio_wait_def_free(ABT_pool_user_def* def);

int ABT_thread_create_priority(ABT_pool pool, void (*thread_func)(void *), void *arg,
                               ABT_thread_attr attr, uint64_t prio, ABT_thread *newthread);

#ifdef __cplusplus
}
#endif

#endif
