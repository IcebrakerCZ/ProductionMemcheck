#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sstream>
#include <string>


static int logging_fd = -1;


thread_local std::stringstream* logging_stringstream = nullptr;


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

  logging_stringstream = new std::stringstream;
}


static void logging_finish()
{
  if (logging_fd >= 0)
  {
    close(logging_fd);
  }

  delete logging_stringstream;
}


struct LoggingHelper
{
  LoggingHelper(const char* file, size_t line, const char* func)
  {
    if (logging_stringstream == nullptr)
    {
      return;
    }

    logging_stringstream->str("");
    *logging_stringstream << file << ":" << line << " " << func << ": ";
  }

  ~LoggingHelper()
  {
    if (logging_stringstream == nullptr)
    {
      return;
    }

    *logging_stringstream << "\n";

    const std::string& logging_message = logging_stringstream->str();

    if (logging_fd >= 0)
    {
      if (write(logging_fd, logging_message.c_str(), logging_message.size())) {}
    }
  }

  LoggingHelper& operator()()
  {
    return *this;
  }

  template<typename T>
  LoggingHelper& operator<<(const T& t)
  {
    if (logging_stringstream != nullptr)
    {
      *logging_stringstream << t;
    }

    return *this;
  }
};

#define LOG_VERBOSE LoggingHelper(__FILE__, __LINE__, __FUNCTION__)
