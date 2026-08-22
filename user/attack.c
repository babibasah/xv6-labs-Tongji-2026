#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define DATASIZE (32 * 4096)

int main(int argc, char *argv[]) {
  char *p = sbrk(DATASIZE);
  if (p == (char*)-1) {
    exit(1);
  }

  char *indicator = "This may help.";
  int length = strlen(indicator);

  for (int i = 0; i <= DATASIZE - 32; i++) {
    if (memcmp(p + i, indicator, length) == 0) {
      char *secret = (p + i) + 16;
      
      if (strcmp(secret, "(null)") == 0) {
        continue;
      }

      printf("%s\n", secret);
      exit(0);
    }
  }

  exit(1);
}