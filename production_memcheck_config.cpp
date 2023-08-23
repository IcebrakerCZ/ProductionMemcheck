#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>

#include <atomic>
#include <ostream>
#include <tuple>


struct ProductionMemcheckConfig
{
  std::atomic<size_t> process_allocations {0};
  std::atomic<size_t> process_deallocations {0};
  std::atomic<size_t> collect {0};


  static char shm_path[1024];


  ProductionMemcheckConfig(const ProductionMemcheckConfig& other)
  {
    *this = other;
  }


  ProductionMemcheckConfig& operator=(const ProductionMemcheckConfig& other)
  {
    process_allocations   = other.process_allocations.load();
    process_deallocations = other.process_deallocations.load();
    collect               = other.collect.load();

    return *this;
  }


  static char* get_shm_path(pid_t pid)
  {
    std::ignore = snprintf(shm_path, sizeof(shm_path) - 1, "production_memcheck-%i.cfg", pid);
    return shm_path;
  }


  static ProductionMemcheckConfig* create()
  {
    pid_t pid = getpid();

    shm_unlink(get_shm_path(pid));

    int fd = shm_open(get_shm_path(pid), O_CREAT | O_RDWR, 0600);
    assert(fd >= 0);

    int result = ftruncate(fd, sizeof(ProductionMemcheckConfig));
    assert(result == 0);

    ProductionMemcheckConfig* config = (ProductionMemcheckConfig*) mmap( NULL, sizeof(ProductionMemcheckConfig)
                                                                       , PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(config != nullptr);

    config->process_allocations   = 0;
    config->process_deallocations = 0;

    return config;
  }


  static ProductionMemcheckConfig* connect(pid_t pid)
  {
    int fd = shm_open(get_shm_path(pid), O_RDWR, 0600);
    if (fd < 0)
    {
      return nullptr;
    }

    ProductionMemcheckConfig* config = (ProductionMemcheckConfig*) mmap( NULL, sizeof(ProductionMemcheckConfig)
                                                                       , PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (config == nullptr)
    {
      return nullptr;
    }

    return config;
  }
};


char ProductionMemcheckConfig::shm_path[1024];


std::ostream& operator<<(std::ostream& os, const ProductionMemcheckConfig& config)
{
  os << "{process_allocations=" << config.process_allocations
     << ", process_deallocations=" << config.process_deallocations
     << ", collect=" << config.collect << "}";

  return os;
}


// Do not define for override below for the tool binary.
#ifdef PRODUCTION_MEMCHECK_SYMBOL_DEFINITION

static ProductionMemcheckConfig* production_memcheck_config = nullptr;

PRODUCTION_MEMCHECK_SYMBOL_DEFINITION(fork, pid_t, ())
{
  if (production_memcheck_config == nullptr)
  {
    return original_fork();
  }

  ++allocations_in_overrided_function;

  ProductionMemcheckConfig tmp = *production_memcheck_config;

  pid_t pid = original_fork();

  // Initialize its own shmem config in the child process.
  if (pid == 0)
  {
    production_memcheck_config = ProductionMemcheckConfig::create();

    *production_memcheck_config = tmp;
  }

  --allocations_in_overrided_function;

  return pid;
}

#endif
