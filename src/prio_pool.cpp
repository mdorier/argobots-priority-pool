/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "prio_pool.h"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <iostream>

struct ArgsWrapper {
    uint64_t priority;
    void (*func)(void*);
    void* args;
};

struct ULT {

    ABT_thread m_thread;

    ULT(ABT_thread t)
    : m_thread(t) {}

    bool operator<(const ULT& other) const {
        uint64_t this_prio = 0, other_prio = 0;
        ArgsWrapper *this_args, *other_args;
        ABT_thread_get_arg(m_thread, (void**)&this_args);
        ABT_thread_get_arg(other.m_thread, (void**)&other_args);
        if(this_args) this_prio = this_args->priority;
        if(other_args) other_prio = other_args->priority;
        if(this_prio < other_prio) return false;
        if(this_prio > other_prio) return true;
        return (intptr_t)m_thread < (intptr_t)other.m_thread;
    }
};

struct data_t {
    std::mutex               mutex;
    std::condition_variable  cond;
    std::priority_queue<ULT> queue;
};

struct QueueAdaptor : public std::priority_queue<ULT> {
    const auto& container() const {
        return this->c;
    }
};

/* Pool functions */

static int pool_init(ABT_pool pool, ABT_pool_config config)
{
    (void)config;
    auto p_data = new data_t{};
    return ABT_pool_set_data(pool, p_data);
}

static void pool_free(ABT_pool pool)
{
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    delete p_data;
}

static ABT_bool pool_is_empty(ABT_pool pool)
{
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    auto guard = std::unique_lock<std::mutex>{p_data->mutex};
    return p_data->queue.empty();
}

static size_t pool_get_size(ABT_pool pool)
{
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    auto guard = std::unique_lock<std::mutex>{p_data->mutex};
    return p_data->queue.size();
}

static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context)
{
    (void)context;
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        p_data->queue.emplace((ABT_thread)unit);
    }
    p_data->cond.notify_one();
}

static void pool_push_many(ABT_pool pool, const ABT_unit *units,
                           size_t num_units, ABT_pool_context context)
{
    (void)context;
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        for(size_t i = 0; i < num_units; ++i)
            p_data->queue.emplace((ABT_thread)units[i]);
    }
    if(num_units == 1)
        p_data->cond.notify_one();
    else
        p_data->cond.notify_all();
}

static ABT_thread pool_pop_wait(ABT_pool pool, double time_secs,
                                ABT_pool_context context)
{
    (void)context;
    data_t* p_data = nullptr;
    ABT_thread result = ABT_THREAD_NULL;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        if(p_data->queue.empty())
            p_data->cond.wait_for(guard, std::chrono::duration<double, std::milli>(time_secs*1000));
        if(!p_data->queue.empty()) {
            result = p_data->queue.top().m_thread;
            p_data->queue.pop();
        }
    }
    return result;
}

static ABT_thread pool_pop(ABT_pool pool, ABT_pool_context context)
{
    (void)context;
    data_t* p_data = nullptr;
    ABT_thread result = ABT_THREAD_NULL;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        if(!p_data->queue.empty()) {
            result = p_data->queue.top().m_thread;
            p_data->queue.pop();
        }
    }
    return result;
}

static void pool_pop_many(ABT_pool pool, ABT_thread *threads,
                          size_t max_threads, size_t *num_popped,
                          ABT_pool_context context)
{
    (void)context;
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        *num_popped = std::min(max_threads, (size_t)p_data->queue.size());
        for(size_t i = 0; i < *num_popped; ++i) {
            threads[i] = p_data->queue.top().m_thread;
            p_data->queue.pop();
        }
    }
}

static void pool_print_all(ABT_pool pool, void *arg,
                           void (*print_fn)(void *, ABT_thread))
{
    data_t* p_data = nullptr;
    ABT_pool_get_data(pool, (void**)&p_data);
    {
        auto guard = std::unique_lock<std::mutex>{p_data->mutex};
        for(auto& ult : static_cast<QueueAdaptor*>(&p_data->queue)->container()) {
            print_fn(arg, ult.m_thread);
        }
    }
}

static ABT_unit pool_create_unit(ABT_pool pool, ABT_thread thread)
{
    (void)pool;
    return (ABT_unit)thread;
}

static void pool_free_unit(ABT_pool pool, ABT_unit unit)
{
    (void)pool;
    (void)unit;
}

extern "C"
int ABT_pool_prio_wait_def_create(ABT_pool_user_def* def)
{
    int ret =ABT_pool_user_def_create(
        pool_create_unit,
        pool_free_unit,
        pool_is_empty,
        pool_pop,
        pool_push,
        def);
    if(ret != ABT_SUCCESS)
        return ret;
    ABT_pool_user_def_set_init(*def, pool_init);
    ABT_pool_user_def_set_free(*def, pool_free);
    ABT_pool_user_def_set_get_size(*def, pool_get_size);
    ABT_pool_user_def_set_pop_wait(*def, pool_pop_wait);
    ABT_pool_user_def_set_pop_many(*def, pool_pop_many);
    ABT_pool_user_def_set_push_many(*def, pool_push_many);
    ABT_pool_user_def_set_print_all(*def, pool_print_all);
    return ABT_SUCCESS;
}

extern "C"
int ABT_pool_prio_wait_def_free(ABT_pool_user_def* def)
{
    return ABT_pool_user_def_free(def);
}

extern "C"
int ABT_thread_create_priority(ABT_pool pool, void (*thread_func)(void *), void *arg,
                               ABT_thread_attr attr, uint64_t priority, ABT_thread *newthread)
{
    auto wrapped_args = new ArgsWrapper{priority, thread_func, arg};
    static auto wrapper_fn = [](void* args) {
        auto wrapped_args = static_cast<ArgsWrapper*>(args);
        (wrapped_args->func)(wrapped_args->args);
        delete wrapped_args;
    };
    int ret = ABT_thread_create(pool, wrapper_fn, wrapped_args, attr, newthread);
    if(ret != ABT_SUCCESS)
        delete wrapped_args;
    return ret;
}
