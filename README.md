# Argobots priority pool

This repository contains the definition of a priority pool
for [Argobots](https://www.argobots.org/). This type of pool
allows associating a `uint64_t` value to ULTs created in the
pool. _The lower this value, the higher the priority_.

To create a pool of this type, you will first need to create
the corresponding `ABT_pool_user_def`, as follows.

```
#include "prio_pool.h"

...

ABT_pool_user_def prio_pool_def;
ABT_pool_prio_wait_def_create(&prio_pool_def);
```

This custom pool definition needs to be freed at the end of
the program, using the following function.

```
ABT_pool_prio_wait_def_free(&prio_pool_def);
```

Once you have your pool definition, creating a pool of this type
is done as follows.

```
ABT_pool pool = ABT_POOL_NULL;
ABT_pool_create(prio_pool_def, ABT_POOL_CONFIG_NULL, &pool);
```

Schedulers and execution streams can then be created that use the pool.

To associate a `uint64_t` when creating ULTs, you can use two methods.
The first one is to ensure that the first field in the structure passed
as argument to your ULT is a `uint64_t` value corresponding to said priority.
For instance:

```
struct myArgs {
    uint64_t priority;
    /* ... rest of your data ... */
};

void myFunction(void* a) {
    struct myArgs* args = (struct myArgs*)a;
    /* ... rest of your function ... */
}

ABT_thread thread;
struct myArgs args;
args.priority = 42;
ABT_thread_create(pool, myFunction, &args, ABT_THREAD_ATTR_NULL, &thread);
```

The second solution is to use `ABT_thread_create_priority` instead of
`ABT_thread_create`, as follows.


```
struct myArgs {
    /* ... your data ... */
};

void myFunction(void* a) {
    struct myArgs* args = (struct myArgs*)a;
    /* ... rest of your function ... */
}

ABT_thread thread;
struct myArgs args = { ... };
ABT_thread_create_priority(pool, myFunction, &args, ABT_THREAD_ATTR_NULL, 42, &thread);
```

The first solution is preferable for two reasons:
1. You won't risk forgetting to use `ABT_thread_create_priority` when pushing
   into a priority pool (were you to use `ABT_thread_create`, the pool would cast
   the first 8 bytes of your argument into a `uint64_t` value, so whatever data
   you have there would be considered your priority, whether it makes any sense or not).
2. `ABT_thread_create_priority` makes an extra heap allocation, which can be avoided
   if the `uint64_t` priority value is already part of your argument structure.
