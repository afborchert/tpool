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
   small test suite for thread_pool.h with 100% C0 coverage

   to test the coverage using gcov,
    * uncomment the -fprofile-arcs and -ftest-coverage flags in
      the Makefile
    * make clean && make
    * ./test_suite
    * gcov -m test_suite.cpp
    * examine thread_pool.hpp.gcov

   gcov -m -b test_suite.cpp delivers following results for
   thread_pool.hpp:

      Lines executed:100.00% of 63
      Branches executed:97.63% of 422
      Taken at least once:51.90% of 422
      Calls executed:73.00% of 626
*/

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>
#include <thread_pool.hpp>

/* trivial test case with two simple tasks that do not overload
   the thread pool */
bool t1() {
   mt::thread_pool tpool(2);
   auto r1 = tpool.submit([]() { return 20; });
   auto r2 = tpool.submit([]() { return 22; });
   return r1.get() + r2.get() == 42;
}

/* trivial test case where we explicitly join
   with two simple tasks that do not overload the thread pool */
bool t2() {
   mt::thread_pool tpool(2);
   auto r1 = tpool.submit([]() { return 20; });
   auto r2 = tpool.submit([]() { return 22; });
   tpool.join();
   return r1.get() + r2.get() == 42;
}

/* trivial test case with two simple tasks where the second
   task has to be queued */
bool t3() {
   mt::thread_pool tpool(1);
   auto r1 = tpool.submit([]() { return 20; });
   auto r2 = tpool.submit([]() { return 22; });
   return r1.get() + r2.get() == 42;
}

/* somewhat larger test case where some tasks are to be queued */
bool t4() {
   constexpr unsigned int size = 2;
   constexpr unsigned int extra = 4;
   mt::thread_pool tpool(size);
   std::future<unsigned int> results[size + extra];
   for (unsigned int i = 0; i < size + extra; ++i) {
      results[i] = tpool.submit([i]() -> unsigned int {
	 std::this_thread::sleep_for(std::chrono::milliseconds(10));
	 return i + 1;
      });
   }
   unsigned int sum = 0;
   for (auto& result: results) {
      sum += result.get();
   }
   return sum == (size + extra) * (size + extra + 1) / 2;
}

/* this test checks if terminate() is done correctly,
   i.e. all tasks shall either complete or deliver broken promises;
   we do not overload the queue here, this is done in t6() */
bool t5() {
   try {
      constexpr unsigned int size = 2;
      std::future<unsigned int> results[size];
      {
	 mt::thread_pool tpool(size);
	 for (unsigned int i = 0; i < size; ++i) {
	    results[i] = tpool.submit([i]() -> unsigned int {
	       std::this_thread::sleep_for(std::chrono::milliseconds(10));
	       return i + 1;
	    });
	 }
	 tpool.terminate();
      }
      unsigned int sum = 0;
      for (auto& result: results) {
	 sum += result.get();
      }
      return sum == size * (size + 1) / 2;
   } catch (std::future_error& e) {
      return e.code() == std::future_errc::broken_promise;
   }
}

/* this tests checks that unprocessed tasks in the queue
   deliver broken promises when terminate() is invoked;
   as we overload the queue broken promises are very likely */
bool t6() {
   try {
      constexpr unsigned int size = 2;
      constexpr unsigned int extra = 4;
      mt::thread_pool tpool(size);
      std::future<unsigned int> results[size + extra];
      for (unsigned int i = 0; i < size + extra; ++i) {
	 results[i] = tpool.submit([i]() -> unsigned int {
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    return i + 1;
	 });
      }
      tpool.terminate();
      unsigned int sum = 0;
      for (auto& result: results) {
	 sum += result.get();
      }
      return sum == (size + extra) * (size + extra + 1) / 2;
   } catch (std::future_error& e) {
      return e.code() == std::future_errc::broken_promise;
   }
}

/* this test checks if it is ok
   when join() is invoked after terminate() */
bool t7() {
   try {
      constexpr unsigned int size = 2;
      constexpr unsigned int extra = 2;
      mt::thread_pool tpool(size);
      std::future<unsigned int> results[size + extra];
      for (unsigned int i = 0; i < size + extra; ++i) {
	 results[i] = tpool.submit([i]() -> unsigned int {
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    return i + 1;
	 });
      }
      tpool.terminate(); tpool.join();
      unsigned int sum = 0;
      for (auto& result: results) {
	 sum += result.get();
      }
      return sum == (size + extra) * (size + extra + 1) / 2;
   } catch (std::future_error& e) {
      return e.code() == std::future_errc::broken_promise;
   }
}

/* this test checks if it is ok
   when terminate() is invoked after join() */
bool t8() {
   constexpr unsigned int size = 2;
   constexpr unsigned int extra = 2;
   mt::thread_pool tpool(size);
   std::future<unsigned int> results[size + extra];
   for (unsigned int i = 0; i < size + extra; ++i) {
      results[i] = tpool.submit([i]() -> unsigned int {
	 std::this_thread::sleep_for(std::chrono::milliseconds(10));
	 return i + 1;
      });
   }
   tpool.join(); tpool.terminate();
   unsigned int sum = 0;
   for (auto& result: results) {
      sum += result.get();
   }
   return sum == (size + extra) * (size + extra + 1) / 2;
}

/* this tests verifies that the destructor
   waits until all tasks are done even if
   they are submitted long after the destructor
   has been invoked */
bool t9() {
   constexpr unsigned int size = 2;
   constexpr unsigned int maxlevel = 4;
   std::atomic<unsigned int> count(0);
   std::function<void(unsigned int)> f;
   {
      mt::thread_pool tpool(size);
      f = [&count, &tpool, &f](unsigned int level) {
	 tpool.submit([&f, level, &count]() {
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    if (level > 0) {
	       f(level-1);
	       f(level-1);
	    }
	    ++count;
	 });
      };
      f(maxlevel);
   }
   return count.load() == (2<<maxlevel) - 1;
}

/* this test checks if tasks that are submitted
   after a completed join() operation return
   future objects whose promise has been broken */
bool t10() {
   constexpr unsigned int size = 2;
   {
      mt::thread_pool tpool(size);
      tpool.join();
      auto f = tpool.submit([]() { return 42; });
      try {
	 f.get();
	 return false;
      } catch (std::future_error& e) {
	 return e.code() == std::future_errc::broken_promise;
      }
   }
}

/* test submissions with parameters */
bool t11() {
   constexpr unsigned int size = 2;
   constexpr unsigned int extra = 4;
   constexpr unsigned int noftasks = size + extra;
   auto f = [](int a, int b) { return a + b; };
   std::future<int> results[noftasks];
   {
      mt::thread_pool tpool(size);
      for (unsigned int i = 0; i < noftasks; ++i) {
	 results[i] = tpool.submit(f, i, 2*i);
      }
   }
   int sum = 0;
   for (auto& result: results) {
      sum += result.get();
   }
   return sum == 3 * noftasks * (noftasks - 1) / 2;
}

/* test default constructor and size method */
bool t12() {
   mt::thread_pool tpool;
   unsigned int size = tpool.size();
   unsigned int extra = size * 2;
   std::vector<std::future<unsigned int>> results(size + extra);
   for (unsigned int i = 0; i < size + extra; ++i) {
      results[i] = tpool.submit([i]() -> unsigned int {
	 std::this_thread::sleep_for(std::chrono::milliseconds(10));
	 return i + 1;
      });
   }
   unsigned int sum = 0;
   for (auto& result: results) {
      sum += result.get();
   }
   return sum == (size + extra) * (size + extra + 1) / 2;
}

/* test concurrent invocations of the join() method */
bool t13() {
   constexpr unsigned int size = 2;
   constexpr unsigned int maxlevel = 4;
   std::atomic<unsigned int> count(0);
   std::function<void(unsigned int)> f;
   mt::thread_pool tpool(size);
   f = [&count, &tpool, &f](unsigned int level) {
      tpool.submit([&f, level, &count]() {
	 std::this_thread::sleep_for(std::chrono::milliseconds(10));
	 if (level > 0) {
	    f(level-1);
	    f(level-1);
	 }
	 ++count;
      });
   };
   f(maxlevel);
   constexpr unsigned int expected = (2<<maxlevel) - 1;
   constexpr unsigned int joining_threads = size * 2;
   std::atomic<unsigned int> ok_count(0);
   {
      mt::thread_pool joining_tpool(joining_threads);
      for (unsigned int i = 0; i < joining_threads; ++i) {
	 joining_tpool.submit([&tpool,&count,&ok_count]() {
	    tpool.join();
	    if (count.load() == expected) ++ok_count;
	 });
      }
   }
   return ok_count.load() == joining_threads;
}

/* test tasks with exceptions */
bool t14() {
   mt::thread_pool tpool(2);
   std::future<void> results[3];
   results[0] = tpool.submit([]() { throw 8; });
   results[1] = tpool.submit([](int val) { throw 2 * val; }, 11);
   results[2] = tpool.submit([](int val1, int val2) { throw val1 * val2; }, 3, 4);
   int sum = 0;
   for (auto& result: results) {
      try {
	 result.get();
      } catch (int& res) {
	 sum += res;
      }
   }
   return sum == 42;
}

/* test that tasks submitted after terminate
   deliver broken promises */
bool t15() {
   mt::thread_pool tpool(2);
   std::promise<void> p1; auto f1 = p1.get_future();
   std::promise<void> p2; auto f2 = p2.get_future();
   auto r1 = tpool.submit([&p1]() {
      p1.set_value();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return 20;
   });
   auto r2 = tpool.submit([&p2]() {
      p2.set_value();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      return 22;
   });
   /* make sure that both initial tasks were started
      before we kill the thread pool */
   f1.get(); f2.get();
   tpool.terminate();
   auto r3 = tpool.submit([]() { return 42; });
   bool ok;
   try {
      r3.get();
      ok = false;
   } catch (std::future_error& e) {
      ok = e.code() == std::future_errc::broken_promise;
   }
   return ok && r1.get() + r2.get() == 42;
}

struct statistics {
   statistics() : passed(0), failed(0), exceptions(0) {
   }
   unsigned int passed;
   unsigned int failed;
   unsigned int exceptions;
};

template<typename F>
void t(const std::string& name, F&& f, statistics& stats) {
   std::cout << name << ": ";
   try {
      if (f()) {
	 ++stats.passed;
	 std::cout << "ok";
      } else {
	 ++stats.failed;
	 std::cout << "failed";
      }
   } catch (std::exception& e) {
      ++stats.exceptions; ++stats.failed;
      std::cout << "failed due to " << e.what();
   }
   std::cout << std::endl;
}

int main() {
   statistics stats;
   t(" t1", t1, stats);
   t(" t2", t2, stats);
   t(" t3", t3, stats);
   t(" t4", t4, stats);
   t(" t5", t5, stats);
   t(" t6", t6, stats);
   t(" t7", t7, stats);
   t(" t8", t8, stats);
   t(" t9", t9, stats);
   t("t10", t10, stats);
   t("t11", t11, stats);
   t("t12", t12, stats);
   t("t13", t13, stats);
   t("t14", t14, stats);
   t("t15", t15, stats);
   unsigned int tests = stats.passed + stats.failed;
   if (tests == stats.passed) {
      std::cout << "all tests passed" << std::endl;
   } else {
      std::cout << stats.passed << " tests passed, " <<
	 stats.failed << " tests failed (" <<
	 (double) stats.failed / tests * 100.0 << "%)" <<
	 std::endl;
   }
}
