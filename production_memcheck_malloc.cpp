#include <malloc.h>

#include <atomic>


static std::atomic<std::uint64_t> memalign_count;
static std::atomic<std::uint64_t> memalign_total_size;


PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(memalign, void*, (size_t alignment, size_t size))
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
