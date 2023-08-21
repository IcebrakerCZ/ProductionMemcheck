#include "glibc_versions.h"

#include <dlfcn.h>


#define PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(symbol_name, return_type, input_args) \
    extern "C"                                                                      \
    {                                                                               \
      typedef return_type (*symbol_name ## _signature)input_args;                   \
      static symbol_name ## _signature  original_ ## symbol_name = nullptr;         \
    }                                                                               \
                                                                                    \
    return_type symbol_name input_args


#define PRODUCTION_MEMCHECK_INITIALIZE_SYMBOL(symbol_name) production_memcheck_set_rtld_next_symbol(original_ ## symbol_name, #symbol_name)


/**
 * Get newest symbol available including newest versioned glibc symbols.
 */
template<typename T>
void production_memcheck_set_rtld_next_symbol(T& t, const char* name)
{
//  LOG_VERBOSE() << "dlsym(RTLD_NEXT, " << name << ")";

  t = (T) dlsym(RTLD_NEXT, name);
  if (t != nullptr)
  {
//    LOG_VERBOSE() << "success " << name;
    return;
  }

  for (const char* glibc_version : glibc_versions)
  {
//    LOG_VERBOSE() << "dlvsym(RTLD_NEXT, " << name << ", " << glibc_version << ")";

    t = (T) dlvsym(RTLD_NEXT, name, glibc_version);
    if (t != nullptr)
    {
//      LOG_VERBOSE() << "success " << name << " (" << glibc_version << ")";
      return;
    }
  }

//  LOG_VERBOSE() << "original symbol for symbol '" << name << "' not found";
}
