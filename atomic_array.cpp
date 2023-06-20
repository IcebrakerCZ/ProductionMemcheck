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
    free(stacktrace);
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

std::ostream& operator<<(std::ostream& os, const MallocInfo& mi)
{
  os << "{type=" << mi.type << ", ptr=" << mi.ptr << ", count=" << mi.count << ", size=" << mi.size << "}";

  return os;
}

/* --------------------------------------------------------------------------------------------------------------------- */

#include <map>
#include <mutex>

static std::map<void*, MallocInfo> production_memcheck_map;
static std::mutex                  production_memcheck_map_mutex;

/* --------------------------------------------------------------------------------------------------------------------- */

#include <algorithm>
#include <array>
#include <atomic>


static std::atomic<std::uint64_t> free_total_size;


template<typename VALUE_TYPE, std::size_t SIZE = 1000000>
class AtomicArray
{
public:

  void push_back(VALUE_TYPE&& item)
  {
//    LOG_VERBOSE() << "malloc_info=" << item;

    size_t pos = m_reserved_size++;

    if (pos >= SIZE)
    {
      std::unique_lock swap_lock(m_swap_mutex);

      pos = m_reserved_size++;

      if (pos >= SIZE)
      {
        swap_arrays_and_reserve_pos_and_collect(swap_lock, std::min(m_reserved_size.load(), SIZE));
        pos = 0;
      }
    }

    m_values[pos] = std::move(item);

    ++m_used_size;
  }


  void collect()
  {
    std::unique_lock swap_lock(m_swap_mutex);

    std::size_t reserved_size = m_reserved_size.exchange(SIZE);

    swap_arrays_and_reserve_pos_and_collect(swap_lock, std::min(reserved_size, SIZE), /* do_reserve = */ false);
  }


private:

  void swap_arrays_and_reserve_pos_and_collect(std::unique_lock<std::mutex>& swap_lock, size_t reserved_size, bool do_reserve = true)
  {
    ++allocations_in_overrided_function;

    while (m_used_size != reserved_size)
    {
      LOG_VERBOSE() << "waiting for m_used_size == reserved_size -> " << m_used_size.load() << " == " << reserved_size;
    }

    // Only one collect can be running at a time because we have only one array to swap.
    std::scoped_lock collect_lock(m_collect_mutex);

    std::swap(m_values, m_collect_values);

    // Important: m_allocated_size and m_used_size must be reseted in this order!!!
    m_used_size = 0;
    m_reserved_size = do_reserve ? 1 : 0;

    swap_lock.unlock();

    collect_impl(m_collect_values, reserved_size);

    --allocations_in_overrided_function;
  }


  void collect_impl(std::array<VALUE_TYPE, SIZE>& values, size_t size)
  {
    LOG_VERBOSE() << "malloc info array size=" << size;

    std::scoped_lock<std::mutex> production_memcheck_map_lock(production_memcheck_map_mutex);

    for (std::size_t pos = 0; pos < size; ++pos)
    {
      MallocInfo& malloc_info = values[pos];

      if (malloc_info.type != malloc_type_free)
      {
        production_memcheck_map.emplace(std::make_pair(malloc_info.ptr, std::move(malloc_info)));
        continue;
      }

      auto i = production_memcheck_map.find(malloc_info.ptr);

      if (i == production_memcheck_map.end())
      {
        continue;
      }

      free_total_size += i->second.size;

      production_memcheck_map.erase(i);
    }

    LOG_VERBOSE() << "production_memcheck_map->size()=" << production_memcheck_map.size();
  }


private:

  std::array<VALUE_TYPE, SIZE> m_values;
  std::atomic<std::size_t>     m_reserved_size {0};
  std::atomic<std::size_t>     m_used_size {0};

  std::mutex                   m_swap_mutex;

  std::array<VALUE_TYPE, SIZE> m_collect_values;
  std::mutex                   m_collect_mutex;
};

static AtomicArray<MallocInfo> production_memcheck_array;
