#include "prio_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct ult_args {
    uint64_t     i;
    uint64_t     p;
};

void ult(void* args) {
    ABT_unit_id id;
    struct ult_args* ult_args = (struct ult_args*)args;
    ABT_thread self_ult = ABT_THREAD_NULL;
    ABT_thread_self(&self_ult);
    ABT_thread_get_id(self_ult, &id);
    printf("ULT %lu: i=%lu, p=%lu\n", id, ult_args->i, ult_args->p);
}

int main(int argc, char** argv)
{
    ABT_init(argc, argv);

    ABT_pool_user_def prio_pool_def;
    ABT_pool_prio_wait_def_create(&prio_pool_def);

    ABT_pool pool = ABT_POOL_NULL;
    ABT_pool_create(prio_pool_def, ABT_POOL_CONFIG_NULL, &pool);

    struct ult_args* args = (struct ult_args*)calloc(100, sizeof(*args));
    ABT_thread* ults      = (ABT_thread*)calloc(100, sizeof(*ults));
    for(size_t i = 0; i < 100; ++i) {
        uint64_t priority = rand() % 1000;
        args[i].i         = i;
        args[i].p         = priority;
        ABT_thread_create_priority(pool, ult, args+i, ABT_THREAD_ATTR_NULL, priority, ults+i);
    }

    ABT_xstream xstream;
    ABT_xstream_create_basic(ABT_SCHED_BASIC_WAIT, 1, &pool, ABT_SCHED_CONFIG_NULL, &xstream);

    for(size_t i = 0; i < 100; ++i) {
        ABT_thread_join(ults[i]);
        ABT_thread_free(ults+i);
    }

    ABT_xstream_join(xstream);
    ABT_xstream_free(&xstream);

    ABT_pool_prio_wait_def_free(&prio_pool_def);
    ABT_finalize();
    return 0;
}
