#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {          //所有命令的基本结构，其中包含一个 type 字段，用于指示命令的类型
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {      //用于表示要执行的命令，包含命令参数数组
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {     //用于表示重定向命令，包含文件重定向信息和指向要运行的命令的指针
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {      //用于表示管道命令，包含指向左侧和右侧命令的指针
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

//用于创建子进程的辅助函数，如果 fork 失败，它会打印错误信息并返回
int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.这个函数用于执行命令，根据命令类型分别处理不同的情况
void runcmd(struct cmd *cmd)
{
  int p[2], r;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    exit(-1);
//普通的执行命令，它会打印 "exec not implemented" 消息
  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);
    fprintf(stderr, "exec not implemented\n");
    // Your code here ...
    if(execvp(ecmd->argv[0],ecmd->argv) < 0) {
      fprintf(stderr,"Command not found: %s\n",ecmd->argv[0]);
    }
    break;
//重定向命令，它会递归调用 runcmd 来处理子命令
  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    fprintf(stderr, "redir not implemented\n");
    // Your code here ...
    //輸出
    int fd_out = open(rcmd->file, rcmd->mode, 0666);
    if (fd_out < 0) {
      fprintf(stderr, "Failed to open %s for writing\n", rcmd->file);
      exit(-1);
    }
    if (dup2(fd_out, rcmd->fd) < 0) {
      fprintf(stderr, "Failed to redirect output\n");
      exit(-1);
  }
    close(fd_out);
  //輸入
    int fd_in = open(rcmd->file, rcmd->mode);
    if (fd_in < 0) {
      fprintf(stderr, "Failed to open %s for reading\n", rcmd->file);
      exit(-1);
    }
    if (dup2(fd_in, rcmd->fd) < 0) {
      fprintf(stderr, "Failed to redirect input\n");
      exit(-1);
    }
    close(fd_in);
    runcmd(rcmd->cmd);
    break;
//管道命令，它会打印 "pipe not implemented" 消息
  case '|':
    pcmd = (struct pipecmd*)cmd;
    fprintf(stderr, "pipe not implemented\n");
    // Your code here ...
    int pipefd[2];
    if (pipe(pipefd) < 0) {
      fprintf(stderr, "Failed to create pipe\n");
      exit(-1);
    }
    int pid1 = fork();
    if (pid1 == 0) {
      close(pipefd[0]); // Close the read end
      dup2(pipefd[1], 1); // Redirect stdout to the pipe
      close(pipefd[1]);
      runcmd(pcmd->left);
      exit(0);
    } else if (pid1 < 0) {
      fprintf(stderr, "Failed to fork\n");
      exit(-1);
    }
    int pid2 = fork();
    if (pid2 == 0) {
      close(pipefd[1]); // Close the write end
      dup2(pipefd[0], 0); // Redirect stdin to the pipe
      close(pipefd[0]);
      runcmd(pcmd->right);
      exit(0);
    } else if (pid2 < 0) {
      fprintf(stderr, "Failed to fork\n");
      exit(-1);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    break;
  }    
  exit(0);
}
//用于获取用户输入的命令行。如果标准输入是终端，它会打印命令提示符
//使用 fgets 从标准输入读取用户输入的命令，并将其存储在 buf 中
int getcmd(char *buf, int nbuf)
{
  
  if (isatty(fileno(stdin)))
    fprintf(stdout, "2440078$ ");
  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

//循环调用 getcmd 来获取用户输入的命令，然后根据命令类型执行相应的操作
int main(void)
{
  static char buf[100];
  int fd, r;

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait(&r);
  }
  exit(0);
}

//用于创建子进程的辅助函数，如果 fork 失败，它会打印错误信息并返回
int fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

//创建普通命令结构
struct cmd* execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

//创建重定向命令结构
struct cmd* redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

//创建管道命令结构
struct cmd* pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

//用于从输入中获取标记（如命令、文件名等）
int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

//用于查看下一个字符是否属于指定的字符集   主要用于跳过空白字符
int peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
//将输入缓冲区中的一段字符复制到一个新的字符串中，并在末尾添加null终止符，使其成为一个新的字符串
char  *mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

//是解析命令的入口点，它接受一个输入字符串并返回相应的命令结构。
struct cmd* parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

//解析普通命令部分
struct cmd* parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  return cmd;
}

//解析管道部分
struct cmd* parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

//用于解析命令中的重定向部分
struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

//解析执行命令部分，包括命令参数以及I/O重定向。它通过不断调用parseredirs来处理重定向，并构建execcmd结构，表示执行命令
struct cmd* parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
