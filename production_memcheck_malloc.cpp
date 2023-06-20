#include <malloc.h>

#include <atomic>


static std::atomic<std::uint64_t> memalign_count {0};
static std::atomic<std::uint64_t> memalign_total_size {0};


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

  void *ptr = original_memalign(alignment, size);

  if (production_memcheck_malloc(malloc_type_memalign, ptr, size))
  {
    ++memalign_count;
    memalign_total_size += size;
  }

  --allocations_in_overrided_function;

  return ptr;
}
