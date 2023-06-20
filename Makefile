CXXFLAGS  = -m64 -std=c++17 -Wall -Werror -O2 -ggdb3 -fno-omit-frame-pointer
CXXFLAGS += $(EXTRA_CXXFLAGS)

LDFLAGS   = -Wl,--export-dynamic
LDFLAGS  += $(EXTRA_LDFLAGS)

LIB_CXXFLAGS = $(CXXFLAGS) -I/usr/include/libiberty -fPIC -DPIC -D_GNU_SOURCE -DSONAME=\"libproduction_memcheck.so\" -pthread
LIB_LDFLAGS  = $(LDFLAGS) -ldl -pthread

TEST_CXXFLAGS += $(CXXFLAGS) -O0 -Wno-unused-result -Wno-deprecated-declarations
TEST_LDFLAGS  += $(LDFLAGS)


.PHONY: all
all: test libproduction_memcheck.so


.PHONY: run
run: run-test-with-production-memcheck run-test-without-production-memcheck

.PHONY: run-test-with-production-memcheck
run-test-with-production-memcheck: test libproduction_memcheck.so
	./production_memcheck ./test

.PHONY: run-test-without-production-memcheck
run-test-without-production-memcheck: test
	./test


.PHONY: clean
clean:
	rm -f test $(TEST_OBJS)
	rm -f libproduction_memcheck.so $(LIB_OBJS)


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


LIB_SRCS = production_memcheck.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)
$(LIB_OBJS): %.o: %.cpp logging.cpp glibc_versions.h
	$(CXX) $(LIB_CXXFLAGS) -c -o $@ $<

libproduction_memcheck.so: $(LIB_OBJS)
	$(CXX) -o $@ $^ -shared $(LIB_LDFLAGS)


glibc_versions.h:
	strings /lib/x86_64-linux-gnu/libc.so.6                                      \
	    | grep GLIBC_                                                            \
	    | grep -v GLIBC_PRIVATE                                                  \
	    | sort -rV                                                               \
	    | sed -E -e '1 { s/^(.*)$$/const char* glibc_versions[] = { "\1"/ }'     \
	             -e '2,999 { s/^(.*)$$/                               , "\1"/ }' \
	    > $@
	echo "                              };" >> $@
