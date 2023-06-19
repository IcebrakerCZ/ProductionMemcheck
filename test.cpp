#include <cstdlib>
#include <cstring>
#include <iostream>


int main(int  argc, char *  argv[])
{
  for (int i = 0; i < 2; ++i)
  {
    char* string = (char*) malloc(1024);
    std::strncpy(string, "Hello World!", 1024);

    std::cout << "malloced string: '" << string << "'" << std::endl;

    if (i)
    {
      free(string);
    }
  }

  return EXIT_SUCCESS;
}
