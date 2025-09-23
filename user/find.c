#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

char* fmtname(char *path) {
  static char buf[DIRSIZ+1];
  char *p;

  for(p=path+strlen(path); p >= path && *p != '/'; p--);
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

void run_exec(char *cmd[], char *file) {
  char *argv[MAXARG];
  int i = 0;

  
  while(cmd[i] && i < MAXARG-1) {
    argv[i] = cmd[i];
    i++;
  }

  argv[i++] = file;
  argv[i] = 0;

  if(fork() == 0) {
    exec(argv[0], argv);
    fprintf(2, "exec %s failed\n", argv[0]);
    exit(1);
  } else {
    wait(0);
  }
}

void find(char *path, char *filename, char *cmd[]) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(fmtname(path), filename) == 0){
      if(cmd) {
        run_exec(cmd, path);
      } else {
        printf("%s\n", path);
      }
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, filename, cmd);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if(argc < 3){
    fprintf(2, "Usage: find <path> <filename> [-exec cmd ...]\n");
    exit(1);
  }

  char **cmd = 0;

  for(int i = 3; i < argc; i++) {
    if(strcmp(argv[i], "-exec") == 0) {
      if(i+1 >= argc) {
        fprintf(2, "find: -exec needs a command\n");
        exit(1);
      }
      cmd = &argv[i+1];
      break;
    }
  }

  find(argv[1], argv[2], cmd);
  exit(0);
}

