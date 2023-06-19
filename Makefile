CXXFLAGS  = -m64 -std=c++17 -Wall -Werror -O2 -ggdb3 -fno-omit-frame-pointer
CXXFLAGS += $(EXTRA_CXXFLAGS)

LDFLAGS   = -Wl,--export-dynamic
LDFLAGS  += $(EXTRA_LDFLAGS)

PRODUCTION_MEMCHECK_CXXFLAGS = $(CXXFLAGS) -I/usr/include/libiberty -fPIC -DPIC -D_GNU_SOURCE -DSONAME=\"libproduction_memcheck.so\" -pthread
PRODUCTION_MEMCHECK_LDFLAGS  = $(LDFLAGS) -ldl -pthread

PRODUCTION_MEMCHECK2_CXXFLAGS += $(PRODUCTION_MEMCHECK_CXXFLAGS)
PRODUCTION_MEMCHECK2_LDFLAGS  += $(PRODUCTION_MEMCHECK_LDFLAGS)

TEST_CXXFLAGS += $(CXXFLAGS) -O0 -Wno-unused-result -Wno-deprecated-declarations
TEST_LDFLAGS  += $(LDFLAGS)


.PHONY: all
all: test libproduction_memcheck.so libproduction_memcheck2.so


.PHONY: run
run: run-test-with-production-memcheck run-test-with-production-memcheck2 run-test-without-production-memcheck

.PHONY: run-test-with-production-memcheck
run-test-with-production-memcheck: test libproduction_memcheck.so
	./production_memcheck ./test

.PHONY: run-test-with-production-memcheck2
run-test-with-production-memcheck2: test libproduction_memcheck2.so
	./production_memcheck2 ./test

.PHONY: run-test-without-production-memcheck
run-test-without-production-memcheck: test
	./test


.PHONY: clean
clean:
	rm -f test $(TEST_OBJS)
	rm -f libproduction_memcheck.so $(PRODUCTION_MEMCHECK_OBJS)
	rm -f libproduction_memcheck2.so $(PRODUCTION_MEMCHECK2_OBJS)


.PHONY: distclean
distclean: clean
	rm -f production_memcheck-*.log
	rm -f production_memcheck-*.txt


TEST_SRCS = test.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
$(MAIN_OBJS): %.o: %.cpp
	$(CXX) $(TEST_CXXFLAGS) -c -o $@ $<

test: $(TEST_OBJS)
	$(CXX) -o $@ $^ $(LTEST_DFLAGS)


PRODUCTION_MEMCHECK_SRCS = production_memcheck.cpp
PRODUCTION_MEMCHECK_OBJS = $(PRODUCTION_MEMCHECK_SRCS:.cpp=.o)
$(PRODUCTION_MEMCHECK_OBJS): %.o: %.cpp logging.cpp
	$(CXX) $(PRODUCTION_MEMCHECK_CXXFLAGS) -c -o $@ $<

libproduction_memcheck.so: $(PRODUCTION_MEMCHECK_OBJS)
	$(CXX) -o $@ $^ -shared $(PRODUCTION_MEMCHECK_LDFLAGS)


PRODUCTION_MEMCHECK2_SRCS = production_memcheck2.cpp
PRODUCTION_MEMCHECK2_OBJS = $(PRODUCTION_MEMCHECK2_SRCS:.cpp=.o)
$(PRODUCTION_MEMCHECK2_OBJS): %.o: %.cpp logging.cpp
	$(CXX) $(PRODUCTION_MEMCHECK2_CXXFLAGS) -c -o $@ $<

libproduction_memcheck2.so: $(PRODUCTION_MEMCHECK2_OBJS)
	$(CXX) -o $@ $^ -shared $(PRODUCTION_MEMCHECK2_LDFLAGS)
