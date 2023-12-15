/* 
   Copyright (c) 2015, 2016, 2017 Andreas F. Borchert
   All rights reserved.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
   This header-only C++11 package provides mt::thread_pool which
   assigns incoming tasks to a fixed number of threads.

   Following methods are supported:

    * thread_pool()
      construct thread pool with n threads where
      n = std::thread::hardware_concurrency()

    * thread_pool(unsigned int nofthreads)
      construct thread pool with the given number of threads

    * ~thread_pool()
      the destructor waits for all threads to finish

    * unsigned int size()
      return the number of threads

    * void join()
      wait for all threads to finish

    * void terminate()
      request a speedy termination, i.e. submitted but not yet assigned
      tasks remain undone; threads that wait for the corresponding futures
      will see broken promises

    * std::future<T> submit(F task_function, parameters...)
      submit a task which is to be executed by one of the threads
      of the pool; the future objects allows to synchronize with
      the completion of the task and to receive the return value
      of the submitted task; the future object will be possibly tied
      to a broken promise if join() has already been called or
      terminate() is invoked before the task has started
*/

#ifndef MT_THREAD_POOL_HPP
#define MT_THREAD_POOL_HPP 1

#if __cplusplus < 201103L
#error This file requires compiler and library support for the \
ISO C++ 2011 standard.
#else

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace mt {

   class thread_pool {
      public:
	 thread_pool() : thread_pool(std::thread::hardware_concurrency()) {
	 }
	 thread_pool(unsigned int nofthreads) :
	       threads(nofthreads), active(0),
	       joining(false), joined(false), terminating(false) {
	    for (auto& t: threads) {
	       t = std::thread([this]() mutable -> void {
		  for (;;) {
		     std::function<void()> task;
		     /* fetch next task, if there is any */
		     {
			std::unique_lock<std::mutex> lock(mutex);
			while (!terminating &&
			      tasks.empty() &&
			      (active > 0 || !joining)) {
			   cv.wait(lock);
			}
			if (tasks.empty()) {
			   break; /* all done */
			}
			task = std::move(tasks.front());
			tasks.pop_front();
			++active;
		     }
		     /* process task */
		     task();
		     /* decrement number of active threads */
		     {
			std::unique_lock<std::mutex> lock(mutex);
			--active;
		     }
		  }
		  /* if we are joining and if there are no active
		     threads we have to wake up all the remaining threads
		     such that they terminate as well */
		  cv.notify_all();
	       });
	    }
	 }
	 ~thread_pool() {
	    join();
	 }

	 unsigned int size() const {
	    return threads.size();
	 }

	 void join() {
	    /* as join may be invoked concurrently by different threads 
	       we must make sure that the first invocation only
	       actually joins the threads; all other invocations wait
	       for this operation to complete */
	    bool we_join = false;
	    {
	       std::unique_lock<std::mutex> lock(mutex);
	       if (!joining) {
		  joining = true;
		  we_join = true;
	       }
	    }
	    if (we_join) {
	       /* first invocation of join() for *this */
	       cv.notify_all();
	       for (auto& t: threads) {
		  if (t.joinable()) {
		     t.join();
		  }
	       }
	       {
		  std::unique_lock<std::mutex> lock(mutex);
		  joined = true;
	       }
	       joining_finished.notify_all();
	    } else {
	       /* someone else has already invoked join(),
	          wait until this operation is completed */
	       std::unique_lock<std::mutex> lock(mutex);
	       while (!joined) {
		  joining_finished.wait(lock);
	       }
	    }
	 }

	 void terminate() {
	    {
	       std::unique_lock<std::mutex> lock(mutex);
	       terminating = true;
	       /* we make sure that all promises left are considered broken
		  by emptying the list of remaining tasks;
		  if we do not do it now, the waiting threads would
		  have to wait until this object is destructed
	       */
	       tasks.clear();
	    }
	    cv.notify_all();
	 }

	 template<typename F, typename... Parameters>
	 auto submit(F&& task_function, Parameters&&... parameters)
	       -> std::future<decltype(task_function(parameters...))> {
	    using T = decltype(task_function(parameters...));
	    /* a std::function object cannot be constructed from
	       a std::packaged_task object as std::function requires
	       the callable object to be copy-constructible;
	       packaged tasks are, however, just move-constructible;
	       the following workaround puts the packaged task
	       behind a shared pointer and passes a simple lambda
	       object to the std::function constructor
	    */
	    auto task = std::make_shared<std::packaged_task<T()>>(
	       std::bind(std::forward<F>(task_function),
		  std::forward<Parameters>(parameters)...)
	    );
	    std::future<T> result = task->get_future();

	    std::lock_guard<std::mutex> lock(mutex);
	    /* enqueue new tasks only if there are
	       threads left to execute them;
	       otherwise we will break right away the
	       promise on returning */
	    if (!terminating && !joined) {
	       tasks.emplace_back([task]() { (*task)(); });
	       cv.notify_one();
	    }
	    return result;
	 }

      private:
	 std::mutex mutex;
	 std::condition_variable cv;
	 std::condition_variable joining_finished; // joined==true?
	 std::vector<std::thread> threads; // fixed number of threads
	 std::deque<std::function<void()>> tasks; // submitted tasks
	 unsigned int active; // number of threads that are executing a task
	 bool joining; // initially false, set to true if join() is invoked
	 bool joined; // initially false, set to true if join is completed
	 bool terminating; // initially false, set to true by terminate()
   };

} // namespace mt

#endif // of #if __cplusplus < 201103L #else ...
#endif // of #ifndef MT_THREAD_POOL_HPP
