#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>


static int logging_fd = -1;


static void logging_init()
{
  if (!getenv("PRODUCTION_MEMCHECK_VERBOSE"))
  {
    return;
  }

//  LOG_VERBOSE("PRODUCTION_MEMCHECK_VERBOSE=" << getenv("PRODUCTION_MEMCHECK_VERBOSE"));

  logging_fd = open( ("production_memcheck-" + std::to_string(getpid()) + ".log").c_str()
                   , O_CREAT | O_TRUNC | O_WRONLY
                   , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


static void logging_finish()
{
  if (logging_fd >= 0)
  {
    close(logging_fd);
  }
}


#include <sstream>

thread_local std::stringstream logging_stringstream;

struct LoggingHelper
{
  LoggingHelper(const char* file, size_t line, const char* func)
  {
    logging_stringstream.str("");
    logging_stringstream << file << ":" << line << " " << func << ": ";
  }

  ~LoggingHelper()
  {
    logging_stringstream << "\n";

    const std::string& logging_message = logging_stringstream.str();

    if (logging_fd >= 0)
    {
      if (write(logging_fd, logging_message.c_str(), logging_message.size())) {}
    }

    if (m_fd >= 0)
    {
      if (write(m_fd, logging_message.c_str(), logging_message.size())) {}
    }
  }

  LoggingHelper& operator()()
  {
    return *this;
  }

  LoggingHelper& operator()(int fd)
  {
    m_fd = fd;
    return *this;
  }

  template<typename T>
  LoggingHelper& operator<<(const T& t)
  {
    if (logging_fd >= 0 || m_fd >= 0)
    {
      logging_stringstream << t;
    }

    return *this;
  }

  int m_fd = -1;
};

#define LOG_VERBOSE LoggingHelper(__FILE__, __LINE__, __FUNCTION__)
