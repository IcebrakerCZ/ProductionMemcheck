CXXFLAGS  = -m64 -std=c++17 -Wall -O0 -ggdb3 -fno-omit-frame-pointer
CXXFLAGS += $(EXTRA_CXXFLAGS)

LDFLAGS   = -Wl,--export-dynamic
LDFLAGS  += $(EXTRA_LDFLAGS)

LIB_CXXFLAGS  = $(CXXFLAGS) -fPIC -DPIC -D_GNU_SOURCE -DSONAME=\"libproduction_memcheck.so\"
LIB_CXXFLAGS += -nostdlib -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
LIB_LDFLAGS   = $(LDFLAGS) -ldl -lrt
LIB_LDFLAGS  += -Wl,-Bsymbolic

TOOL_CXXFLAGS += $(CXXFLAGS) -fPIE -DPIE
TOOL_LDFLAGS  += $(LDFLAGS) -lrt

TEST_CXXFLAGS += $(CXXFLAGS) -fPIE -DPIE
TEST_CXXFLAGS += $(CXXFLAGS) -Wno-unused-result -Wno-deprecated-declarations
TEST_LDFLAGS  += $(LDFLAGS)


.PHONY: all
all: test production_memcheck_tool libproduction_memcheck.so


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
	rm -f libproduction_memcheck.so $(LIB_OBJS) glibc_versions.h
	rm -f production_memcheck_tool $(TOOL_OBJS)


.PHONY: distclean
distclean: clean
	rm -f production_memcheck-*.log
	rm -f production_memcheck-*.txt


TEST_SRCS = test.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
$(TEST_OBJS): %.o: %.cpp
	$(CXX) $(TEST_CXXFLAGS) -c -o $@ $<

test: $(TEST_OBJS)
	$(CXX) -o $@ $^ $(TEST_LDFLAGS)


TOOL_SRCS = production_memcheck_tool.cpp
TOOL_OBJS = $(TOOL_SRCS:.cpp=.o)
$(TOOL_OBJS): %.o: %.cpp
	$(CXX) $(TOOL_CXXFLAGS) -c -o $@ $<

production_memcheck_tool: $(TOOL_OBJS)
	$(CXX) -o $@ $^ $(TOOL_LDFLAGS)


LIB_SRCS = production_memcheck.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)
$(LIB_OBJS): %.o: %.cpp glibc_versions.h production_memcheck_logging.cpp production_memcheck_dlfcn.cpp production_memcheck_malloc.cpp production_memcheck_stdlib.cpp production_memcheck_atomic_array.cpp
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
