#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define WAIT  6

#define MAXHISTORY 100
#define MAXCMDLEN 100
#define MAXARG 32

int from_file = 0;
char history[MAXHISTORY][MAXCMDLEN];
int history_count = 0;

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARG];
  char *eargv[MAXARG];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

struct waitcmd {
  int type;
};

int fork1(void);
void panic(char*);
struct cmd *parsecmd(char*);

// Simple strncmp implementation for xv6
int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

// Execute cmd. Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;

  case WAIT:
    wait(0);
    break;

  default:
    panic("runcmd");
  }
  exit(0);
}

// Check if file descriptor is a terminal
// In xv6, we can check if it's the console (fd 0, 1, or 2)
int
isatty(int fd)
{
  // In xv6, file descriptors 0, 1, 2 are the console
  // Other file descriptors are files or pipes
  return (fd == 0 || fd == 1 || fd == 2);
}

int
getcmd(char *buf, int nbuf)
{
  // Only show prompt if we're reading from terminal (not file)
  if (!from_file) {
    fprintf(2, "$ ");
  }
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

void
add_to_history(char *cmd)
{
  // Don't add empty commands
  if (cmd[0] == '\n' || cmd[0] == 0) return;
  
  // Remove trailing newline for clean storage
  int len = strlen(cmd);
  if (len > 0 && cmd[len-1] == '\n') {
    cmd[len-1] = 0;
  }
  
  // Don't add if empty after removing newline
  if (cmd[0] == 0) return;
  
  // Don't add duplicate consecutive commands
  if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0) {
    // Restore newline for execution
    cmd[len-1] = '\n';
    cmd[len] = 0;
    return;
  }
  
  if (history_count < MAXHISTORY) {
    strcpy(history[history_count], cmd);
    history_count++;
  } else {
    // Shift history down
    for (int i = 0; i < MAXHISTORY - 1; i++) {
      strcpy(history[i], history[i + 1]);
    }
    strcpy(history[MAXHISTORY - 1], cmd);
  }
  
  // Restore newline for execution
  cmd[len-1] = '\n';
  cmd[len] = 0;
}

void
print_history()
{
  for (int i = 0; i < history_count; i++) {
    printf("%d: %s\n", i + 1, history[i]);
  }
}

// Check if string starts with prefix
int
startswith(char *s, char *prefix)
{
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

int
main(void)
{
  static char buf[100];
  
  // Detect if we're reading from a file instead of terminal
  // In xv6, we can check if stdin (fd 0) is the console
  // A simple heuristic: if we're not fd 0,1,2, we're probably from a file
  // For now, we'll set a simple flag - in real implementation you'd check file type
  from_file = 0; // Default to interactive mode
  
  // Check command line arguments to see if we're reading from file
  // If sh is called with a script file as argument, we're in file mode
  // For now, we'll keep it simple and assume interactive mode
  
  // Read and run commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    // Handle empty command
    if(buf[0] == '\n') continue;
    
    // Store original command for history
    char original_buf[100];
    strcpy(original_buf, buf);
    
    // Remove trailing newline for processing
    int len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
      buf[len-1] = 0;
    }
    
    // Handle built-in history command
    if (strcmp(buf, "history") == 0) {
      print_history();
      // Restore newline and add to history
      buf[len-1] = '\n';
      buf[len] = 0;
      add_to_history(original_buf);
      continue;
    }
    
    // Handle !! command - execute last command
    if (strcmp(buf, "!!") == 0) {
      if (history_count > 0) {
        // Get the last command (not including "!!")
        strcpy(buf, history[history_count - 1]);
        len = strlen(buf);
        buf[len] = '\n';
        buf[len+1] = 0;
        printf("%s", buf);
      } else {
        fprintf(2, "No previous command\n");
        // Restore newline and add to history
        buf[len-1] = '\n';
        buf[len] = 0;
        add_to_history(original_buf);
        continue;
      }
    }
    
    // Check for wait command before forking
    if (strcmp(buf, "wait") == 0) {
      wait(0);
      // Restore newline and add to history
      buf[len-1] = '\n';
      buf[len] = 0;
      add_to_history(original_buf);
      continue;
    }
    
    // Handle cd command
    if(startswith(buf, "cd ")) {
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      // Restore newline and add to history
      buf[len-1] = '\n';
      buf[len] = 0;
      add_to_history(original_buf);
      continue;
    }
    
    // Check for background command before restoring newline
    int background = 0;
    if (len > 1 && buf[len-1] == 0 && buf[len-2] == '&') {
      background = 1;
      buf[len-2] = 0; // Remove the &
    }
    
    // Restore newline for command parsing
    buf[len-1] = '\n';
    buf[len] = 0;
    
    // Add to history after all processing
    add_to_history(original_buf);
    
    int pid = fork1();
    if(pid == 0) {
      runcmd(parsecmd(buf));
    } else {
      if(!background) {
        wait(0);
      }
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;
  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

// [Rest of the code remains the same - all the constructors and parsing functions]
// Constructors (unchanged)
struct cmd* execcmd(void) {
  struct execcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd) {
  struct redircmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right) {
  struct pipecmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* listcmd(struct cmd *left, struct cmd *right) {
  struct listcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* backcmd(struct cmd *subcmd) {
  struct backcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}

struct cmd* waitcmd(void) {
  struct waitcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = WAIT;
  return (struct cmd*)cmd;
}

// Parsing functions (unchanged - using original xv6 parsing)
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s;
  int ret;
  s = *ps;
  while(s < es && strchr(whitespace, *s)) s++;
  if(q) *q = s;
  ret = *s;
  switch(*s){
  case 0: break;
  case '|': case '(': case ')': case ';': case '&': case '<':
    s++; break;
  case '>':
    s++;
    if(*s == '>'){ ret = '+'; s++; }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
    break;
  }
  if(eq) *eq = s;
  while(s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks) {
  char *s;
  s = *ps;
  while(s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd* parsecmd(char *s) {
  char *es;
  struct cmd *cmd;
  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd* parseline(char **ps, char *es) {
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd* parsepipe(char **ps, char *es) {
  struct cmd *cmd;
  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es) {
  int tok;
  char *q, *eq;
  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') panic("missing file for redirection");
    switch(tok){
    case '<': cmd = redircmd(cmd, q, eq, O_RDONLY, 0); break;
    case '>': cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1); break;
    case '+': cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1); break;
    }
  }
  return cmd;
}

struct cmd* parseblock(char **ps, char *es) {
  struct cmd *cmd;
  if(!peek(ps, es, "(")) panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")")) panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd* parseexec(char **ps, char *es) {
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  // Check for wait command
  if(peek(ps, es, "wait")){
    gettoken(ps, es, &q, &eq);
    if(peek(ps, es, "")) return waitcmd();
  }

  ret = execcmd();
  cmd = (struct execcmd*)ret;
  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0) break;
    if(tok != 'a') panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARG) panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd* nulterminate(struct cmd *cmd) {
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0) return 0;
  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++) *ecmd->eargv[i] = 0;
    break;
  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;
  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;
  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;
  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  case WAIT:
    break;
  }
  return cmd;
} 
