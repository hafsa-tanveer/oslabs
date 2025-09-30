#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define BUF_SIZE 512 
//programs runs slow with a smaller buffer size, crashes w a bigger one

int is_sep(char c) {
  char *seps = " -\r\t\n./,";
  for (int i = 0; seps[i]; i++) {
    if (c == seps[i])
      return 1; //checks for separators
  }
  return 0;
}

void sixfive(int fd) {
  char buf[BUF_SIZE];
  int n, i;
  int num = 0;
  int has_num = 0;

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (i = 0; i < n; i++) {
      if (buf[i] >= '0' && buf[i] <= '9') {
        num = num * 10 + (buf[i] - '0');
        has_num = 1;   //checks number
      } else if (is_sep(buf[i])) {
        if (has_num) {
          if (num % 5 == 0 || num % 6 == 0) //checks multiple
            printf("%d\n", num);
          num = 0;
          has_num = 0;
        }
      } else {
        
        num = 0;
        has_num = 0;
      }
    }
  }

  
  if (has_num && (num % 5 == 0 || num % 6 == 0)) {
    printf("%d\n", num);
  }
}

int main(int argc, char *argv[]) {
  int fd;

  if (argc <= 1) {
    fprintf(2, "Usage: sixfive files...\n");
    exit(1);  //added to read file; didnt work otherwise 
  }

  for (int i = 1; i < argc; i++) {
    fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      exit(1);
    }
    sixfive(fd);
    close(fd);
  }

  exit(0);
}

