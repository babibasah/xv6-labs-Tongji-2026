#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int main(int argc, char *argv[]) {
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char*) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2; 
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void memdump(char *fmt, char *data) {
  if (fmt == 0 || data == 0L || *fmt == '\0') {
    fprintf(2, "both fmt and data must have values");
    exit(1);
  }

  while (*fmt != '\0') {
    switch (*fmt) {
      case 'i': {
        int *output = (int*) data;
        printf("%d\n", *output);
        data += 4;
        break;
      }
      case 'p': {
        int val = *(int*) data;
        printf("%x\n", val);
        data += 8;
        break;
      }
      case 'h': {
        short *output = (short*) data;
        printf("%d\n", *output);
        data += 2;
        break;
      }
      case 'c': {
        printf("%c\n", *data);
        data++;
        break;
      }
      case 's': {
        char* target_data = *(char**) data;
        printf("%s\n", target_data);
        data += 8;
        break;
      }
      case 'S': {
        printf("%s\n", data);
        data += strlen(data) + 1;
        break;
      }
    }
    fmt++;
  }
}