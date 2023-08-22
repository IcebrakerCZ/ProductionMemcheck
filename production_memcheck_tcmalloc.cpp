#include <gperftools/malloc_hook.h>
#include <cassert>


void production_memcheck_tcmalloc_new_hook(const void* ptr, size_t size) {
  if (size == 0)
  {
    return;
  }

  if (!production_memcheck_config->process_allocations || allocations_in_overrided_function)
  {
    return;
  }

  ++allocations_in_overrided_function;

  std::ignore = production_memcheck_malloc(malloc_type_malloc, (void *) ptr, size);

  ++malloc_count;
  malloc_total_size += size;

  --allocations_in_overrided_function;
}


void production_memcheck_tcmalloc_delete_hook(const void* ptr) {
  if (ptr == NULL)
  {
    return;
  }

  if (!production_memcheck_config->process_deallocations || allocations_in_overrided_function)
  {
    return;
  }

  ++allocations_in_overrided_function;

  std::ignore = production_memcheck_free((void *) ptr);

  ++free_count;

  --allocations_in_overrided_function;
}


void production_memcheck_init_tcmalloc()
{
  int (*mallochook_addnewhook)(MallocHook_NewHook) = nullptr;
  production_memcheck_set_rtld_next_symbol(mallochook_addnewhook, "MallocHook_AddNewHook", RTLD_DEFAULT);
  if (!mallochook_addnewhook)
  {
    production_memcheck_set_rtld_next_symbol(mallochook_addnewhook, "MallocHook_AddNewHook", RTLD_NEXT);
  }

  int (*mallochook_adddeletehook)(MallocHook_DeleteHook) = nullptr;
  production_memcheck_set_rtld_next_symbol(mallochook_adddeletehook, "MallocHook_AddDeleteHook", RTLD_DEFAULT);
  if (!mallochook_adddeletehook)
  {
    production_memcheck_set_rtld_next_symbol(mallochook_adddeletehook, "MallocHook_AddDeleteHook", RTLD_NEXT);
  }

  if (mallochook_addnewhook || mallochook_adddeletehook)
  {
    assert(mallochook_addnewhook(&production_memcheck_tcmalloc_new_hook));
    assert(mallochook_adddeletehook(&production_memcheck_tcmalloc_delete_hook));
  }
}


bool production_memcheck_finish_tcmalloc()
{
  int (*mallochook_removenewhook)(MallocHook_NewHook) = nullptr;
  production_memcheck_set_rtld_next_symbol(mallochook_removenewhook, "MallocHook_RemoveNewHook", RTLD_DEFAULT);
  if (!mallochook_removenewhook)
  {
    production_memcheck_set_rtld_next_symbol(mallochook_removenewhook, "MallocHook_RemoveNewHook", RTLD_NEXT);
  }

  int (*mallochook_removedeletehook)(MallocHook_DeleteHook) = nullptr;
  production_memcheck_set_rtld_next_symbol(mallochook_removedeletehook, "MallocHook_RemoveDeleteHook", RTLD_DEFAULT);
  if (!mallochook_removedeletehook)
  {
    production_memcheck_set_rtld_next_symbol(mallochook_removedeletehook, "MallocHook_RemoveDeleteHook", RTLD_NEXT);
  }

  if (mallochook_removenewhook || mallochook_removedeletehook)
  {
    assert(mallochook_removenewhook(&production_memcheck_tcmalloc_new_hook));
    assert(mallochook_removedeletehook(&production_memcheck_tcmalloc_delete_hook));
    return true;
  }

  return false;
}
