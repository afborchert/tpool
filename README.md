# tpool
Thread pool for C++11

## Summary

This header-only C++11 package provides `mt::thread_pool` which
allows to assign tasks to a fixed number of threads:

```C++
mt::thread_pool tpool(2); // thread pool with 2 threads
auto res1 = tpool.submit([]() { return 8; });
auto res2 = tpool.submit([](int val) { return 2 * val; }, 11);
auto res3 = tpool.submit([](int val1, int val2) { return val1 * val2; }, 3, 4);
int answer = res1.get() + res2.get() + res3.get();
```

The _submit_ method accepts a function object and its parameters, if any,
which are added to an internal queue of tasks. Each thread of
the pool continuously fetches a task from the queue and executes it.
A thread pool allows to avoid the repeated construction and destruction
of threads for small tasks. It is ideally suited for a master/worker
pattern where the master submits tasks and where the threads of the
pool represent the workers.

The return value of _submit_ is a _std::future_ object that delivers
either the return value of the submitted task or an exception
if one was thrown.

Other patterns like divide and conquer are supported as well where
each task is free to submit more tasks to the pool. However, tasks
shall never wait for tasks to complete they have spawned
themselves.

## Termination

A synchronization is possible by the returned _std::future_
objects in regard to individual tasks. The _join_ method permits
to synchronize with the termination of the entire thread pool.

After _join_ has been invoked, the thread pool continues as
before. Only if the queue becomes empty and all threads are
idling, the pool is shut down. This behaviour makes sure
that even after _join_ all tasks are free to submit new tasks
to the pool without having the pool prematurely downsized to
fewer threads.

It is also possible to shutdown the thread pool more
urgently using the _terminate_ method. Then all non-idling
threads will just complete their current tasks. If any
tasks are left in the queue, their promises will be broken,
i.e. the _get_ method of the corresponding _std::future_
objects will deliver the exception `std::future_error`
where the _code_ method returns `std::future_errc::broken_promise`.

The destructor of a thread pool invokes the _join_ method.

## License

This package is available under the terms of
the [MIT License](https://opensource.org/licenses/MIT).

## Files

To use `mt::thread_pool`, you will need just to drop
[thread_pool.hpp](https://github.com/afborchert/tpool/blob/master/thread_pool.hpp)
within your project and `#include` it.

The source file `test_suite.cpp` is an associated
test suite and the Makefile helps to compile it.

## Alternatives

This is not the first attempt to provide a thread pool
for C++11.

 * Jakob Progsch offers
   [a similar header-only thread pool](https://github.com/progschj/ThreadPool)
   which, however, stops threads of its pool too early when it is still
   theoretically possible that running threads enqueue new tasks.

 * Vitaliy Vitsentiy has developed
   [two header-only variants](https://github.com/vit-vit/CTPL),
   one of them based on standard C++11, the other one using
   a lock-free queue from the Boost library. They offer methods
   to resize the thread pool, to clear the queue, and to
   access individual threads out of the pool. The _stop_
   method comes with two variants, one of them implies a
   _terminate_ as above but threads stop regardless if
   other threads could still possibly enqueue new tasks.
