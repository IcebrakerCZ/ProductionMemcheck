#include <stdlib.h>

#include <atomic>


static std::atomic<std::uint64_t> malloc_count {0};
static std::atomic<std::uint64_t> malloc_total_size {0};

static std::atomic<std::uint64_t> free_count {0};

static std::atomic<std::uint64_t> realloc_count {0};
static std::atomic<std::uint64_t> realloc_total_size {0};

static std::atomic<std::uint64_t> calloc_count {0};
static std::atomic<std::uint64_t> calloc_total_size {0};

static std::atomic<std::uint64_t> reallocarray_count {0};
static std::atomic<std::uint64_t> reallocarray_total_size {0};


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(malloc, void*, (size_t size))
{
  if (production_memcheck_config == nullptr || original_malloc == nullptr)
  {
    return nullptr;
  }

  if (!production_memcheck_config->process_allocations || allocations_in_overrided_function)
  {
    return original_malloc(size);
  }

  ++allocations_in_overrided_function;

  void *ptr = original_malloc(size);

  if (production_memcheck_malloc(malloc_type_malloc, ptr, size))
  {
    ++malloc_count;
    malloc_total_size += size;
  }

  --allocations_in_overrided_function;

  return ptr;
}


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(free, void, (void* ptr))
{
  if (ptr == nullptr)
  {
    return;
  }

  if (production_memcheck_config == nullptr || original_free == nullptr)
  {
    return;
  }

  if (!production_memcheck_config->process_deallocations || allocations_in_overrided_function)
  {
    original_free(ptr);
    return;
  }

  ++allocations_in_overrided_function;

  original_free(ptr);

  if (production_memcheck_free(ptr))
  {
    ++free_count;
  }

  --allocations_in_overrided_function;
}


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(realloc, void*, (void* old_ptr, size_t size))
{
  if (production_memcheck_config == nullptr || original_realloc == nullptr)
  {
    return nullptr;
  }

  if (!(production_memcheck_config->process_allocations || production_memcheck_config->process_deallocations) || allocations_in_overrided_function)
  {
    return original_realloc(old_ptr, size);
  }

  ++allocations_in_overrided_function;

  void *ptr = original_realloc(old_ptr, size);

  if (production_memcheck_realloc(malloc_type_realloc, ptr, size, old_ptr))
  {
    ++realloc_count;
    realloc_total_size += size;
  }

  --allocations_in_overrided_function;

  return ptr;
}


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(calloc, void*, (size_t nmemb, size_t size))
{
  if (production_memcheck_config == nullptr || original_calloc == nullptr)
  {
    return nullptr;
  }

  if (!production_memcheck_config->process_allocations || allocations_in_overrided_function)
  {
    return original_calloc(nmemb, size);
  }

  ++allocations_in_overrided_function;

  void *ptr = original_calloc(nmemb, size);

  if (production_memcheck_malloc(malloc_type_calloc, ptr, nmemb * size))
  {
    ++calloc_count;
    calloc_total_size += nmemb * size;
  }

  --allocations_in_overrided_function;

  return ptr;
}


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(reallocarray, void*, (void* old_ptr, size_t nmemb, size_t size))
{
  if (production_memcheck_config == nullptr || original_reallocarray == nullptr)
  {
    return nullptr;
  }

  if (!production_memcheck_config->process_allocations || allocations_in_overrided_function)
  {
    return original_reallocarray(old_ptr, nmemb, size);
  }

  ++allocations_in_overrided_function;

  void *ptr = original_reallocarray(old_ptr, nmemb, size);

  if (production_memcheck_realloc(malloc_type_reallocarray, ptr, nmemb * size, old_ptr))
  {
    ++reallocarray_count;
    reallocarray_total_size += nmemb * size;
  }

  --allocations_in_overrided_function;

  return ptr;
}
