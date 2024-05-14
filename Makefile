CC := $(CXX)
DEBUG := -g # -fprofile-arcs -ftest-coverage
THREADS := -pthread
STD=11
CXXFLAGS := -Wall -Wextra -I. -std=c++$(STD) $(DEBUG) $(THREADS)
LDFLAGS := $(DEBUG) $(THREADS)
.PHONY:		all clean
all:		test_suite
test_suite.o:	test_suite.cpp thread_pool.hpp

clean:
		rm -f test_suite test_suite.o *.gcov gmon.out *.gcno *.gcda core
