#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>


int main(int  argc, char *  argv[])
{
  for (int i = 0; i < 2; ++i)
  {
    char* string = (char*) malloc(2048);
    std::strncpy(string, "Hello Malloc!", 2048);

    std::cout << "malloced string: '" << string << "'" << std::endl;

    if (i)
    {
      free(string);
    }

    std::string* cxxstring = new std::string(4096, 'Z');
    assert(cxxstring != nullptr);
    *cxxstring = "Hello New!";

    std::cout << "new string: '" << *cxxstring << "'" << std::endl;

    if (i)
    {
      delete cxxstring;
    }
  }

  return EXIT_SUCCESS;
}
