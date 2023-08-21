#include "production_memcheck_config.cpp"

#include <sstream>
#include <iostream>


int main(int argc, char* argv[])
{
  if (argc < 2 || argc > 4)
  {
    std::cerr << std::endl
              << "ERROR: wrong number of cmdline arguments " << argc << std::endl
              << std::endl
              << "Usage: " << argv[0] << " PID [(start|stop) [allocations|deallocations]]" << std::endl
              << std::endl;
    return 1;
  }


  std::stringstream ss;
  ss << argv[1];

  pid_t pid;
  ss >> pid;

  ProductionMemcheckConfig* config = ProductionMemcheckConfig::connect(pid);
  if (config == nullptr)
  {
    std::cerr << std::endl
              << "ERROR: production_memcheck shmem config for pid " << pid << " does not exist yet." << std::endl
              << std::endl;
    return 1;
  }

  std::cout << "Production memcheck actual config: " << *config << std::endl;

  if (argc == 2)
  {
    return 0;
  }


  std::string action = argv[2];
  std::string type   = argc == 4 ? argv[3] : "";
  if (action == "start")
  {
    if (type == "allocations")
    {
      config->process_allocations = 1;
    }
    else if (type == "deallocations")
    {
      config->process_deallocations = 1;
    }
    else if (type.empty())
    {
      config->process_allocations   = 1;
      config->process_deallocations = 1;
    }
    else
    {
      std::cerr << std::endl
                << "ERROR: unknown type '" << type << "'" << std::endl
                << std::endl
                << "Usage: " << argv[0] << " (start|stop) [allocations|deallocations]" << std::endl
                << std::endl;
    }
  }
  else if (action == "stop")
  {
    if (type == "allocations")
    {
      config->process_allocations = 0;
    }
    else if (type == "deallocations")
    {
      config->process_deallocations = 0;
    }
    else if (type.empty())
    {
      config->process_allocations   = 0;
      config->process_deallocations = 0;
    }
    else
    {
      std::cerr << std::endl
                << "ERROR: unknown type '" << type << "'" << std::endl
                << std::endl
                << "Usage: " << argv[0] << " (start|stop) [allocations|deallocations]" << std::endl
                << std::endl;
    }
  }
  else
  {
    std::cerr << std::endl
              << "ERROR: unknown action '" << action << "'" << std::endl
              << std::endl
              << "Usage: " << argv[0] << " (start|stop) [allocations|deallocations]" << std::endl
              << std::endl;
    return 1;
  }

  std::cout << "Production memcheck new config: " << *config << std::endl;

  return 0;
}
