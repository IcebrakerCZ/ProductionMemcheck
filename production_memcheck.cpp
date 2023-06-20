/* --------------------------------------------------------------------------------------------------------------------- */

#include "logging.cpp"

/* --------------------------------------------------------------------------------------------------------------------- */

thread_local size_t allocations_in_overrided_function = 0;

/* --------------------------------------------------------------------------------------------------------------------- */

#include <atomic>
#include <stdio.h>
#include <dlfcn.h>


static std::atomic<size_t>  allocations_processing_enabled {0};

void enable_allocations_processing()
{
  ++allocations_in_overrided_function;
  LOG_VERBOSE() << "allocations_processing_enabled=" << ++allocations_processing_enabled;
  --allocations_in_overrided_function;
}

void disable_allocations_processing()
{
  ++allocations_in_overrided_function;
  LOG_VERBOSE() << "allocations_processing_enabled=" << --allocations_processing_enabled;
  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

#include "atomic_array.cpp"

static AtomicArray<MallocInfo> allocations_array;

/* --------------------------------------------------------------------------------------------------------------------- */

#include "production_memcheck_dlfcn.cpp"

static void allocations_malloc_hook(const char* malloc_type, void *ptr, size_t size, void* old_ptr = NULL);

#include "production_memcheck_malloc.cpp"
#include "production_memcheck_stdlib.cpp"

/* --------------------------------------------------------------------------------------------------------------------- */

extern "C" {

void production_memcheck_init()  __attribute__((constructor));
void production_memcheck_finish() __attribute__((destructor));

} // extern "C"


void allocations_collect();


void production_memcheck_init()
{
  ++allocations_in_overrided_function;

  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(malloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(free);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(realloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(calloc);
  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(reallocarray);

  PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(memalign);

  logging_init();

  LOG_VERBOSE() << "begin";

  LOG_VERBOSE() << "original_malloc="        << (void*) original_malloc;
  LOG_VERBOSE() << "original_free="          << (void*) original_free;
  LOG_VERBOSE() << "original_realloc="       << (void*) original_realloc;
  LOG_VERBOSE() << "original_memalign="      << (void*) original_memalign;
  LOG_VERBOSE() << "original_calloc="        << (void*) original_calloc;
  LOG_VERBOSE() << "original_reallocarrayn=" << (void*) original_reallocarray;

  const char* enabled = getenv("PRODUCTION_MEMCHECK_ENABLED");
  if (enabled != nullptr && enabled[0] != '\0')
  {
    LOG_VERBOSE() << "enabling allocations processing due to found environment variable PRODUCTION_MEMCHECK_ENABLED set to '" << enabled << "'";

    allocations_in_overrided_function = 1;
    LOG_VERBOSE() << "allocations_processing_enabled=" << ++allocations_processing_enabled;
  }

  LOG_VERBOSE() << "end";

  --allocations_in_overrided_function;
}


void production_memcheck_finish()
{
  ++allocations_in_overrided_function;

  allocations_processing_enabled = 0;

  LOG_VERBOSE() << "begin";

  LOG_VERBOSE() << "allocations_processing_enabled=" << allocations_processing_enabled;

  allocations_collect();

  production_memcheck_map.clear();

  LOG_VERBOSE() << "end";

  logging_finish();

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

size_t outfile_counter = 0;

const char* outfile_name = "production_memcheck-";
const char* outfile_ext  = ".txt";

size_t allocations_stacktrace_min_count = 1;

void allocations_collect()
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
  };

  allocations_array.collect();

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

    const auto& [iter, inserted] = stacktraces_map.insert(std::make_pair(key, StackTraceInfo(malloc_info.type)));

    iter->second.count += 1;
    iter->second.size  += malloc_info.size;
  }
  LOG_VERBOSE() << "stacktraces_map.size()=" << stacktraces_map.size();


  std::string outfile = std::string(outfile_name) + std::to_string(getpid()) + "." + std::to_string(outfile_counter++) + std::string(outfile_ext);
  std::string outfile_tmp = outfile + ".tmp";

  int file = open(outfile_tmp.c_str(), O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  std::ostringstream message;

  message << "allocations_processing_enabled=" << allocations_processing_enabled << "\n";

  message << "malloc_count="       << malloc_count       << " malloc_total_size="       << malloc_total_size       << "\n"
          << "free_count="         << free_count         << " free_total_size="         << free_total_size         << "\n"
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

  LOG_VERBOSE() << "stacktraces_map.size()=" << stacktraces_map.size();

  LOG_VERBOSE() << "end";

  --allocations_in_overrided_function;
}

/* --------------------------------------------------------------------------------------------------------------------- */

#include <cstdint>

#include <execinfo.h>

static void allocations_malloc_hook(const char* malloc_type, void *ptr, size_t size, void* old_ptr)
{
  // free or one of realloc calls
  if (old_ptr != NULL)
  {
    MallocInfo malloc_info;
    malloc_info.type = malloc_type_free;
    malloc_info.ptr  = old_ptr;

    allocations_array.push_back(std::move(malloc_info));
  }

  // free call only
  if (malloc_type == NULL)
  {
    return;
  }

  // malloc or malloc part of realloc calls
  MallocInfo malloc_info;
  malloc_info.type = malloc_type;
  malloc_info.ptr  = ptr;
  malloc_info.size = size;

  void* frames[10];
  malloc_info.stacktrace_size = backtrace(frames, sizeof(frames) / sizeof(void*));

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
