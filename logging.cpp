#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>


static int logging_fd = -1;


static void logging_init()
{
  logging_fd = open( ("production_memcheck-" + std::to_string(getpid()) + ".log").c_str()
                   , O_CREAT | O_TRUNC | O_WRONLY
                   , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


static void logging_finish()
{
  close(logging_fd);
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

    if (write(logging_fd, logging_message.c_str(), logging_message.size())) {}

    if (m_fd >= 0)
    {
      if (write(m_fd, logging_message.c_str(), logging_message.size())) {}
    }
  }

  std::stringstream& operator()()
  {
    return logging_stringstream;
  }

  std::stringstream& operator()(int fd)
  {
    m_fd = fd;
    return logging_stringstream;
  }

  int m_fd = -1;
};

#define LOG LoggingHelper(__FILE__, __LINE__, __FUNCTION__)
