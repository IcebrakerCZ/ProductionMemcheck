/* --------------------------------------------------------------------------------------------------------------------- */

#include "logging.cpp"

/* --------------------------------------------------------------------------------------------------------------------- */

#include <execinfo.h>
#include <unistd.h>

/* --------------------------------------------------------------------------------------------------------------------- */

#include <stdlib.h>
#include <malloc.h>

typedef void *(*malloc_signature)(size_t size);
static malloc_signature  original_malloc = NULL;

typedef void (*free_signature)(void *ptr);
static free_signature  original_free = NULL;

typedef void *(*realloc_signature)(void *ptr, size_t size);
static realloc_signature  original_realloc = NULL;

typedef void *(*memalign_signature)(size_t alignment, size_t size);
static memalign_signature  original_memalign = NULL;


typedef void *(*calloc_signature)(size_t nmemb, size_t size);
static calloc_signature  original_calloc = NULL;

typedef void *(*reallocarray_signature)(void *ptr, size_t nmemb, size_t size);
static reallocarray_signature  original_reallocarray = NULL;

/* --------------------------------------------------------------------------------------------------------------------- */

extern "C" {

void allocations_init()  __attribute__((constructor));
void allocations_finish() __attribute__((destructor));

} // extern "C"

/* --------------------------------------------------------------------------------------------------------------------- */

#include <memory>

static const char* malloc_type_malloc       = "malloc";
static const char* malloc_type_free         = "free";
static const char* malloc_type_realloc      = "realloc";
static const char* malloc_type_memalign     = "memalign";
static const char* malloc_type_calloc       = "calloc";
static const char* malloc_type_reallocarray = "reallocarray";

struct MallocInfo
{
  MallocInfo() = default;

  ~MallocInfo()
  {
    original_free(stacktrace);
  }

  MallocInfo(const MallocInfo&) = delete;
  MallocInfo& operator=(const MallocInfo&) = delete;

  MallocInfo(MallocInfo&& other)
  {
    *this = std::move(other);
  }

  MallocInfo& operator=(MallocInfo&& other)
  {
    type            = other.type;
    ptr             = other.ptr;
    count           = other.count;
    size            = other.size;
    stacktrace_size = other.stacktrace_size;
    stacktrace      = other.stacktrace;

    other.stacktrace_size = 0;
    other.stacktrace = nullptr;

    return *this;
  }

  const char* type       = nullptr;
  void*  ptr             = nullptr;
  size_t count           = 0;
  size_t size            = 0;
  size_t stacktrace_size = 0;
  char** stacktrace      = nullptr;
};

/* --------------------------------------------------------------------------------------------------------------------- */

thread_local size_t allocations_in_overrided_function = 0;

/* --------------------------------------------------------------------------------------------------------------------- */

#include <map>
#include <mutex>

static std::map<void*, MallocInfo> allocations_map;
static std::mutex allocations_map_mutex;

/* --------------------------------------------------------------------------------------------------------------------- */

#include <algorithm>
#include <array>
#include <atomic>

template<typename VALUE_TYPE, std::size_t SIZE = 1000000>
class ThreadSafeArray
{
public:

  void push_back(VALUE_TYPE&& item)
  {
    size_t pos = m_allocated_size++;

    if (pos >= SIZE)
    {
      std::lock_guard<std::mutex> lock(allocations_map_mutex);
      if (m_allocated_size >= SIZE)
      {
        collect_impl(std::min(m_allocated_size.load(), SIZE));
      }
      pos = 0;
    }

    m_values[pos] = std::move(item);

    ++m_used_size;
  }


  void collect()
  {
    std::lock_guard<std::mutex> lock(allocations_map_mutex);

    collect_impl(std::min(m_allocated_size.exchange(SIZE), SIZE), /* do_reserve = */ false);
  }


private:

  void collect_impl(size_t allocated_size, bool do_reserve = true)
  {
    ++allocations_in_overrided_function;

    while (m_used_size != allocated_size)
    {
      LOG() << "waiting for used_size == allocated_size -> " << m_used_size.load() << " == " << allocated_size;
    }

    LOG() << "allocated_size=" << allocated_size;

    for (std::size_t pos = 0; pos < allocated_size; ++pos)
    {
      MallocInfo& malloc_info = m_values[pos];

      if (malloc_info.type == malloc_type_free)
      {
        allocations_map.erase(malloc_info.ptr);
        continue;
      }

      allocations_map.emplace(std::make_pair(malloc_info.ptr, std::move(malloc_info)));
    }

    // Important: m_allocated_size and m_used_size must be reseted in this order!!!
    m_used_size = 0;
    m_allocated_size = do_reserve ? 1 : 0;

    LOG() << "allocations_map->size()=" << allocations_map.size();

    --allocations_in_overrided_function;
  }


private:

  std::array<VALUE_TYPE, SIZE> m_values;
  std::atomic<std::size_t> m_allocated_size {0};
  std::atomic<std::size_t> m_used_size {0};
};

static ThreadSafeArray<MallocInfo> allocations_array;

/* --------------------------------------------------------------------------------------------------------------------- */

#include <atomic>
#include <stdio.h>
#include <dlfcn.h>


static std::atomic<size_t>  allocations_processing_enabled {0};

void enable_allocations_processing()
{
  ++allocations_in_overrided_function;
  LOG() << "allocations_processing_enabled=" << ++allocations_processing_enabled;
  --allocations_in_overrided_function;
}

void disable_allocations_processing()
{
  ++allocations_in_overrided_function;
  LOG() << "allocations_processing_enabled=" << --allocations_processing_enabled;
  --allocations_in_overrided_function;
}


void allocations_collect();

void allocations_init()
{
  ++allocations_in_overrided_function;

  original_malloc       = (malloc_signature)       dlsym(RTLD_NEXT, "malloc");
  original_free         = (free_signature)         dlsym(RTLD_NEXT, "free");
  original_realloc      = (realloc_signature)      dlsym(RTLD_NEXT, "realloc");
  original_memalign     = (memalign_signature)     dlsym(RTLD_NEXT, "memalign");

  original_calloc       = (calloc_signature)       dlsym(RTLD_NEXT, "calloc");
  original_reallocarray = (reallocarray_signature) dlsym(RTLD_NEXT, "reallocarray");

  logging_init();

  LOG() << "begin";

  LOG() << "original_malloc="        << (void*) original_malloc;
  LOG() << "original_free="          << (void*) original_free;
  LOG() << "original_realloc="       << (void*) original_realloc;
  LOG() << "original_memalign="      << (void*) original_memalign;
  LOG() << "original_calloc="        << (void*) original_calloc;
  LOG() << "original_reallocarrayn=" << (void*) original_reallocarray;

  const char* enabled = getenv("PRODUCTION_MEMCHECK_ENABLED");
  if (enabled != nullptr && enabled[0] != '\0')
  {
    LOG() << "enabling allocations processing due to found environment variable PRODUCTION_MEMCHECK_ENABLED set to '" << enabled << "'";

    allocations_in_overrided_function = 1;
    LOG() << "allocations_processing_enabled=" << ++allocations_processing_enabled;
  }

  LOG() << "end";

  --allocations_in_overrided_function;
}


void allocations_finish()
{
  ++allocations_in_overrided_function;

  allocations_processing_enabled = 0;

  LOG() << "begin";

  LOG() << "allocations_processing_enabled=" << allocations_processing_enabled;

  allocations_collect();

  allocations_map.clear();

  LOG() << "end";

  logging_finish();

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

std::atomic<std::uint64_t> malloc_count;
std::atomic<std::uint64_t> malloc_total_size;

std::atomic<std::uint64_t> free_count;

std::atomic<std::uint64_t> realloc_count;
std::atomic<std::uint64_t> realloc_total_size;

std::atomic<std::uint64_t> memalign_count;
std::atomic<std::uint64_t> memalign_total_size;


std::atomic<std::uint64_t> calloc_count;
std::atomic<std::uint64_t> calloc_total_size;

std::atomic<std::uint64_t> reallocarray_count;
std::atomic<std::uint64_t> reallocarray_total_size;

/* --------------------------------------------------------------------------------------------------------------------- */

size_t outfile_counter = 0;

const char* outfile_name = "production_memcheck-";
const char* outfile_ext  = ".txt";

size_t allocations_stacktrace_min_count = 1;

void allocations_collect()
{
  ++allocations_in_overrided_function;

  LOG() << "begin";

  struct StackTraceInfo
  {
    StackTraceInfo(const char* t)
      : type(t)
    {}

    const char* type;
    size_t count = 0;
    size_t size  = 0;
  };

  allocations_array.collect();

  std::lock_guard<std::mutex> lock(allocations_map_mutex);

  std::map<std::string, StackTraceInfo> stacktraces_map;
  for (auto& [ptr, malloc_info] : allocations_map)
  {
    std::string key;
    for (size_t i = 0; i < malloc_info.stacktrace_size; ++i)
    {
      key += malloc_info.stacktrace[i];
      key += "\n";
    }

    const auto& [iter, inserted] = stacktraces_map.insert(std::make_pair(key, StackTraceInfo(malloc_info.type)));

    iter->second.count += 1;
    iter->second.size  += malloc_info.size;
  }
  LOG() << "stacktraces_map.size()=" << stacktraces_map.size();


  std::string outfile = std::string(outfile_name) + std::to_string(getpid()) + "." + std::to_string(outfile_counter++) + std::string(outfile_ext);
  std::string outfile_tmp = outfile + ".tmp";

  int file = open(outfile_tmp.c_str(), O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  std::ostringstream message;

  message << "allocations_processing_enabled=" << allocations_processing_enabled << "\n";

  message << "malloc_count="       << malloc_count       << " malloc_total_size="       << malloc_total_size       << "\n"
          << "free_count="         << free_count                                                                   << "\n"
          << "realloc_count="      << realloc_count      << " realloc_total_size="      << realloc_total_size      << "\n"
          << "memalign_count="     << memalign_count     << " memalign_total_size="     << memalign_total_size     << "\n"
          << "calloc_count="       << calloc_count       << " calloc_total_size="       << calloc_total_size       << "\n"
          << "reallocarray_count=" << reallocarray_count << " reallocarray_total_size=" << reallocarray_total_size << "\n";

  message << "stacktraces_map.size()=" << stacktraces_map.size() << "\n";

  size_t pos = 0;
  for (const auto& [key, stacktrace_info] : stacktraces_map)
  {
    ++pos;

    if (stacktrace_info.count < allocations_stacktrace_min_count)
    {
      continue;
    }

    message << "\nunfreed " << stacktrace_info.type << " #" << pos << "/" << stacktraces_map.size() << " "
            << stacktrace_info.count << "x total_size=" << stacktrace_info.size << ":\n" << key.c_str();
  }

  std::string tmp = message.str();
  if (write(file, tmp.c_str(), tmp.size())) {}
  close(file);

  rename(outfile_tmp.c_str(), outfile.c_str());

  LOG() << "stacktraces_map.size()=" << stacktraces_map.size();

  LOG() << "end";

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

extern "C" {

#include <string.h>

#if 0
static bool verify_frame(const char *s) {

        /* Generated by glibc's native backtrace_symbols() on Fedora */
        if (strstr(s, "/" SONAME "("))
                return false;

        /* Generated by glibc's native backtrace_symbols() on Debian */
        if (strstr(s, "/" SONAME " ["))
                return false;

        /* Generated by backtrace-symbols.c */
        if (strstr(s, __FILE__":"))
                return false;

        return true;
}
#endif

int light_backtrace(void **buffer, int size) {
  return backtrace(buffer, size);
#if defined(__i386__) || defined(__x86_64__)
  int osize = 0;
  void *stackaddr;
  size_t stacksize;
  void *frame;

  pthread_attr_t attr;

  errno = 0;

  if (pthread_getattr_np(pthread_self(), &attr))
  {
    LOG() << "pthread_getattr_np failed with error " << errno << ": " << strerror(errno);
    return backtrace(buffer, size);
  }

  if (pthread_attr_getstack(&attr, &stackaddr, &stacksize))
  {
    LOG() << "pthread_attr_getstack failed with error " << errno << ": " << strerror(errno);
    return backtrace(buffer, size);
  }

  if (pthread_attr_destroy(&attr))
  {
    LOG() << "pthread_attr_destroy failed with error " << errno << strerror(errno);
    return backtrace(buffer, size);
  }

#if defined(__i386__)
  __asm__("mov %%ebp, %[frame]": [frame] "=r" (frame));
#elif defined(__x86_64__)
  __asm__("mov %%rbp, %[frame]": [frame] "=r" (frame));
#endif

  while (   osize < size
         && frame >= stackaddr
         && frame < (void *)((char *)stackaddr + stacksize)
        )
  {
    buffer[osize++] = *((void **)frame + 1);
    frame = *(void **)frame;
  }

  if (osize <= 1)
  {
    return backtrace(buffer, size);
  }

  return osize;
#else
  return backtrace(buffer, size);
#endif
}

#if 0
static char *stacktrace_to_string(struct stacktrace_info stacktrace) {
        char **strings, *ret, *p;
        int i;
        size_t k;
        bool b;

        strings = (char**) backtrace_symbols(stacktrace.frames, stacktrace.nb_frame);
        //assert(strings);

        k = 0;
        for (i = 0; i < stacktrace.nb_frame; i++)
                k += strlen(strings[i]) + 2;

        ret = (char*) malloc(k + 1);
        //assert(ret);

        b = false;
        for (i = 0, p = ret; i < stacktrace.nb_frame; i++) {
                if (!b && !verify_frame(strings[i]))
                        continue;

                if (!b && i > 0) {
                        /* Skip all but the first stack frame of ours */
                        *(p++) = '\t';
                        strcpy(p, strings[i-1]);
                        p += strlen(strings[i-1]);
                        *(p++) = '\n';
                }

                b = true;

                *(p++) = '\t';
                strcpy(p, strings[i]);
                p += strlen(strings[i]);
                *(p++) = '\n';
        }

        *p = 0;

        free(strings);

        return ret;
}
#endif
} // extern "C"
/* --------------------------------------------------------------------------------------------------------------------- */

#include <cstdint>

#include <execinfo.h>

void allocations_malloc_hook(const char* malloc_type, void *ptr, size_t size, void* old_ptr = NULL)
{
  if (old_ptr != NULL)
  {
    MallocInfo malloc_info;
    malloc_info.type = malloc_type_free;
    malloc_info.ptr  = old_ptr;

    allocations_array.push_back(std::move(malloc_info));
  }

  MallocInfo malloc_info;
  malloc_info.type = malloc_type;
  malloc_info.ptr  = ptr;
  malloc_info.size = size;

  void* frames[31];
  malloc_info.stacktrace_size = light_backtrace(frames, sizeof(frames) / sizeof(void*));

  if (malloc_info.stacktrace_size > 1)
  {
    malloc_info.stacktrace = backtrace_symbols(&frames[1], --malloc_info.stacktrace_size);

    if (malloc_info.stacktrace == nullptr)
    {
      malloc_info.stacktrace_size = 0;
    }
  }
  else
  {
    malloc_info.stacktrace_size = 0;
  }

  allocations_array.push_back(std::move(malloc_info));
}

/* --------------------------------------------------------------------------------------------------------------------- */

void *malloc(size_t size)
{
  if (original_malloc == NULL)
  {
    return NULL;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    return original_malloc(size);
  }

  ++allocations_in_overrided_function;

  ++malloc_count;
  malloc_total_size += size;

  void *ptr = original_malloc(size);

  allocations_malloc_hook(malloc_type_malloc, ptr, size);

  --allocations_in_overrided_function;

  return ptr;
}

/* --------------------------------------------------------------------------------------------------------------------- */

void free(void *ptr)
{
  if (original_free == NULL)
  {
    return;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    original_free(ptr);
    return;
  }

  ++allocations_in_overrided_function;

  ++free_count;

  original_free(ptr);

  MallocInfo malloc_info;
  malloc_info.type = malloc_type_free;
  malloc_info.ptr  = ptr;

  allocations_array.push_back(std::move(malloc_info));

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

void *realloc(void *old_ptr, size_t size)
{
  if (original_realloc == NULL)
  {
    return NULL;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    return original_realloc(old_ptr, size);
  }

  ++allocations_in_overrided_function;

  ++realloc_count;
  realloc_total_size += size;

  void *ptr = original_realloc(old_ptr, size);

  allocations_malloc_hook(malloc_type_realloc, ptr, size, old_ptr);

  --allocations_in_overrided_function;

  return ptr;
}

/* --------------------------------------------------------------------------------------------------------------------- */

void *memalign(size_t alignment, size_t size)
{
  if (original_memalign == NULL)
  {
    return NULL;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    return original_memalign(alignment, size);
  }

  ++allocations_in_overrided_function;

  ++memalign_count;
  memalign_total_size += size;

  void *ptr = original_memalign(alignment, size);

  allocations_malloc_hook(malloc_type_memalign, ptr, size);

  --allocations_in_overrided_function;

  return ptr;
}

/* --------------------------------------------------------------------------------------------------------------------- */

void *calloc(size_t nmemb, size_t size)
{
  if (original_calloc == NULL)
  {
    return NULL;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    return original_calloc(nmemb, size);
  }

  ++allocations_in_overrided_function;

  ++calloc_count;
  calloc_total_size += nmemb * size;

  void *ptr = original_calloc(nmemb, size);

  allocations_malloc_hook(malloc_type_calloc, ptr, nmemb * size);

  --allocations_in_overrided_function;

  return ptr;
}

/* --------------------------------------------------------------------------------------------------------------------- */

void *reallocarray(void* old_ptr, size_t nmemb, size_t size)
{
  if (original_reallocarray == NULL)
  {
    return NULL;
  }

  if (!allocations_processing_enabled || allocations_in_overrided_function)
  {
    return original_reallocarray(old_ptr, nmemb, size);
  }

  ++allocations_in_overrided_function;

  ++reallocarray_count;
  reallocarray_total_size += nmemb * size;

  void *ptr = original_reallocarray(old_ptr, nmemb, size);

  allocations_malloc_hook(malloc_type_reallocarray, ptr, nmemb * size, old_ptr);

  --allocations_in_overrided_function;

  return ptr;
}

/* --------------------------------------------------------------------------------------------------------------------- */
