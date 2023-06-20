#include <stdlib.h>

#include <atomic>


static std::atomic<std::uint64_t> malloc_count;
static std::atomic<std::uint64_t> malloc_total_size;

static std::atomic<std::uint64_t> free_count;

static std::atomic<std::uint64_t> realloc_count;
static std::atomic<std::uint64_t> realloc_total_size;

static std::atomic<std::uint64_t> calloc_count;
static std::atomic<std::uint64_t> calloc_total_size;

static std::atomic<std::uint64_t> reallocarray_count;
static std::atomic<std::uint64_t> reallocarray_total_size;


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(malloc, void*, (size_t size))
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


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(free, void, (void* ptr))
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

  allocations_malloc_hook(malloc_type_free, NULL, 0, ptr);

  --allocations_in_overrided_function;
}


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(realloc, void*, (void* old_ptr, size_t size))
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


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(calloc, void*, (size_t nmemb, size_t size))
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


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(reallocarray, void*, (void* old_ptr, size_t nmemb, size_t size))
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
