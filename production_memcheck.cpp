/* -------------------------------------------------------------------------------------------------------------------- */

#include "production_memcheck_logging.cpp"

/* --------------------------------------------------------------------------------------------------------------------- */

thread_local size_t allocations_in_overrided_function = 0;

/* --------------------------------------------------------------------------------------------------------------------- */

#include "production_memcheck_config.cpp"

static ProductionMemcheckConfig* production_memcheck_config = nullptr;

/* --------------------------------------------------------------------------------------------------------------------- */

#include "production_memcheck_atomic_array.cpp"

static AtomicArray<MallocInfo> production_memcheck_array;

/* --------------------------------------------------------------------------------------------------------------------- */

#include "production_memcheck_dlfcn.cpp"

static bool production_memcheck_free(void *ptr);
static bool production_memcheck_malloc(const char* malloc_type, void *ptr, size_t size);
static bool production_memcheck_realloc(const char* malloc_type, void *ptr, size_t size, void* old_ptr);

#include "production_memcheck_malloc.cpp"
#include "production_memcheck_stdlib.cpp"
#include "production_memcheck_tcmalloc.cpp"

/* --------------------------------------------------------------------------------------------------------------------- */

#include <execinfo.h>


extern "C" {

void production_memcheck_init()  __attribute__((constructor));
void production_memcheck_finish() __attribute__((destructor));

} // extern "C"


void production_memcheck_collect_allocations();


std::size_t production_memcheck_backtrace_depth = 10;


void production_memcheck_init()
{
  ++allocations_in_overrided_function;

  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(malloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(free);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(realloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(calloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(reallocarray);

  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(memalign);

  production_memcheck_init_tcmalloc();

  logging_init();

  LOG_VERBOSE() << "begin";

  LOG_VERBOSE() << "original_malloc="       << (void*) original_malloc;
  LOG_VERBOSE() << "original_free="         << (void*) original_free;
  LOG_VERBOSE() << "original_realloc="      << (void*) original_realloc;
  LOG_VERBOSE() << "original_memalign="     << (void*) original_memalign;
  LOG_VERBOSE() << "original_calloc="       << (void*) original_calloc;
  LOG_VERBOSE() << "original_reallocarray=" << (void*) original_reallocarray;

  production_memcheck_config = ProductionMemcheckConfig::create();
  assert(production_memcheck_config != nullptr);

  const char* enabled = getenv("PRODUCTION_MEMCHECK_ENABLED");
  if (enabled != nullptr && enabled[0] != '\0')
  {
    LOG_VERBOSE() << "enabling allocations processing due to found environment variable PRODUCTION_MEMCHECK_ENABLED set to '" << enabled << "'";

    production_memcheck_config->process_allocations   = 1;
    production_memcheck_config->process_deallocations = 1;

    LOG_VERBOSE() << "production_memcheck_config=" << *production_memcheck_config;
  }

  const char* backtrace_depth = getenv("PRODUCTION_MEMCHECK_DEPTH");
  if (backtrace_depth != nullptr)
  {
    LOG_VERBOSE() << "Using backtrace depth from environment variable PRODUCTION_MEMCHECK_DEPTH set to '" << backtrace_depth << "'";

    std::stringstream m;
    m << backtrace_depth;
    m >> production_memcheck_backtrace_depth;

    LOG_VERBOSE() << "production_memcheck_backtrace_depth=" << production_memcheck_backtrace_depth;
  }

  LOG_VERBOSE() << "end";

  --allocations_in_overrided_function;
}


void production_memcheck_finish()
{
  ++allocations_in_overrided_function;

  production_memcheck_config->process_allocations   = 0;
  production_memcheck_config->process_deallocations = 0;

  if (production_memcheck_finish_tcmalloc())
  {
    // Do not allow to free pointers from tcmalloc by glibc free.
    original_free = nullptr;
  }

  LOG_VERBOSE() << "begin";

  LOG_VERBOSE() << "production_memcheck_config=" << *production_memcheck_config;

  production_memcheck_collect_allocations();

  production_memcheck_map.clear();

  LOG_VERBOSE() << "end";

  logging_finish();

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

std::atomic<size_t> outfile_counter = 0;

const char* outfile_name = "production_memcheck-";
const char* outfile_ext  = ".txt";

void production_memcheck_collect_allocations()
{
  ++allocations_in_overrided_function;

  LOG_VERBOSE() << "begin";

  struct StackTraceInfo
  {
    StackTraceInfo(const char* t)
      : type(t)
    {}

    const char* type;
    size_t count = 0;
    size_t size  = 0;
    std::vector<void*> ptrs;
  };

  production_memcheck_array.collect();

  std::lock_guard<std::mutex> lock(production_memcheck_map_mutex);

  std::map<std::string, StackTraceInfo> stacktraces_map;
  for (auto& [ptr, malloc_info] : production_memcheck_map)
  {
    std::string key;
    for (size_t i = 0; i < malloc_info.stacktrace_size; ++i)
    {
      key += malloc_info.stacktrace[i];
      key += "\n";
    }

    const auto& [iter, inserted] = stacktraces_map.emplace(std::make_pair(key, StackTraceInfo(malloc_info.type)));

    iter->second.count += 1;
    iter->second.size  += malloc_info.size;
    iter->second.ptrs.push_back(malloc_info.ptr);
  }
  LOG_VERBOSE() << "stacktraces_map.size()=" << stacktraces_map.size();


  std::string outfile = std::string(outfile_name) + std::to_string(getpid()) + "." + std::to_string(outfile_counter++) + std::string(outfile_ext);
  std::string outfile_tmp = outfile + ".tmp";

  int file = open(outfile_tmp.c_str(), O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  std::ostringstream message;

  message << "malloc      : calls=" << malloc_count       << " sum=" << malloc_total_size       << " bytes\n"
          << "free        : calls=" << free_count         << " sum=" << free_total_size         << " bytes\n"
          << "realloc     : calls=" << realloc_count      << " sum=" << realloc_total_size      << " bytes\n"
          << "memalign    : calls=" << memalign_count     << " sum=" << memalign_total_size     << " bytes\n"
          << "calloc      : calls=" << calloc_count       << " sum=" << calloc_total_size       << " bytes\n"
          << "reallocarray: calls=" << reallocarray_count << " sum=" << reallocarray_total_size << " bytes\n";

  message << "\nFound " << stacktraces_map.size() << " items to report:\n";

  size_t pos = 0;
  for (const auto& [key, stacktrace_info] : stacktraces_map)
  {
    ++pos;

    message << "\nItem " << pos << ": ";

    if (stacktrace_info.type == malloc_type_free)
    {
      message << "invalid free";
    }
    else
    {
      message << "unfreed " << stacktrace_info.type;
    }

    message << " - calls=" << stacktrace_info.count;

    if (stacktrace_info.type != malloc_type_free)
    {
      message << " sum=" << stacktrace_info.size << " bytes";
    }

    message << ":\n" << key.c_str() << "pointers:";

    for (void* ptr : stacktrace_info.ptrs)
    {
      message << " " << ptr;
    }

    message << "\n";
  }

  std::string tmp = message.str();
  if (write(file, tmp.c_str(), tmp.size())) {}
  close(file);

  rename(outfile_tmp.c_str(), outfile.c_str());


  LOG_VERBOSE() << "stacktraces_map.size()=" << stacktraces_map.size();

  LOG_VERBOSE() << "end";

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

#include <cstdint>

#include <execinfo.h>


static bool production_memcheck_free(void *ptr)
{
  if (production_memcheck_config->collect.exchange(0))
  {
    production_memcheck_collect_allocations();
  }

  if (ptr == nullptr)
  {
    return false;
  }

  MallocInfo malloc_info(malloc_type_free, ptr);

  void* frames[production_memcheck_backtrace_depth + 1];
  malloc_info.stacktrace_size = backtrace(frames, sizeof(frames) / sizeof(void*));

  if (malloc_info.stacktrace_size > 1)
  {
    malloc_info.stacktrace = backtrace_symbols(&frames[1], --malloc_info.stacktrace_size);

    if (malloc_info.stacktrace == nullptr)
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  production_memcheck_array.push_back(std::move(malloc_info));

  return true;
}


static bool production_memcheck_malloc(const char* malloc_type, void *ptr, size_t size)
{
  if (production_memcheck_config->collect.exchange(0))
  {
    production_memcheck_collect_allocations();
  }

  if (size == 0)
  {
    return false;
  }

  MallocInfo malloc_info(malloc_type, ptr, size);

  void* frames[production_memcheck_backtrace_depth + 1];
  malloc_info.stacktrace_size = backtrace(frames, sizeof(frames) / sizeof(void*));

  if (malloc_info.stacktrace_size > 1)
  {
    malloc_info.stacktrace = backtrace_symbols(&frames[1], --malloc_info.stacktrace_size);

    if (malloc_info.stacktrace == nullptr)
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  production_memcheck_array.push_back(std::move(malloc_info));

  return true;
}


static bool production_memcheck_realloc(const char* malloc_type, void *ptr, size_t size, void* old_ptr)
{
  return production_memcheck_free(old_ptr) || production_memcheck_malloc(malloc_type, ptr, size);
}

/* --------------------------------------------------------------------------------------------------------------------- */
