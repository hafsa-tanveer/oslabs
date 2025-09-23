#include "kernel/types.h"
#include "user/user.h" 

int main(int argc, char *argv[]) {
  char buffer[512];
  int n;

  while ((n = read(0, buffer, sizeof(buffer))) > 0) {
    int i = 0;
    while (i < n) {
      
      while (i < n && (buffer[i] < '0' || buffer[i] > '9')) {
        i++;
      }

      
      if (i >= n) {
        break;
      }

      char num_str[64];
      int j = 0;
      while (i < n && buffer[i] >= '0' && buffer[i] <= '9') {
        if (j < sizeof(num_str) - 1) {
          num_str[j++] = buffer[i];
        }
        i++;
      }
      num_str[j] = '\0';

      if (j > 0) {
        int val = atoi(num_str);
        if (val % 5 == 0 || val % 6 == 0) {
          printf("%d\n", val);
        }
      }
    }
  }

  exit(0);
}
