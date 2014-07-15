#include<stdlib.h>
#include <errno.h>
#include <string.h>
/*This program is used to test function system*/


main()
{
 system("./test.sh");
  printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
}