#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int eval(char *);

int main(int argc, char **argv) {
    if(argc != 2) {
      printf("ERROR: must call the target with exactly one string command line argument.\n");
      return 1;
    }
    
    return eval(argv[1]);
}

int eval(char * input) {
  // 62 possible characters
  if(input[0] == 'S')
    if(input[1] == '0')
      if(input[2] == 's')
	__builtin_trap();//*((int *)0) = 15;

  if(input[0] == 'M')
    if(input[1] == 'i')
      if(input[2] == '5')
	__builtin_trap();//*((int *)0) = 15;

  if(input[0] == '0')
    if(input[1] == '0')
      if(input[2] == '7')
	while(1);

  return 0;
}
