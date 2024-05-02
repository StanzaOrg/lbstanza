#include<sys/wait.h>
#include<sys/mman.h>
#include<stdint.h>
#include<stdbool.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/time.h>
#include<errno.h>
#include<fcntl.h>
#include<signal.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<dirent.h>
#include<pthread.h>

static void exit_with_error_line_and_func (const char* file, int line){
  fprintf(stderr, "[%s:%d] %s\n", file, line, strerror(errno));
  exit(-1);
}

#define exit_with_error() exit_with_error_line_and_func(__FILE__, __LINE__)



#include "process.h"
#include "process-posix.c"

stz_byte* file;
stz_byte** argvs;

int main () {
  // setup
  install_autoreaping_sigchld_handler();
  // consts
  file = strdup("true");
  argvs = &file;
  stz_byte* wdir = NULL;
  stz_byte** evars = NULL;
  // launch a bunch of processes
  for(int i = 0; i < 40; i++) {
    Process* proc = (Process*)malloc(sizeof(Process));
    int p = launch_process(
      file,
      argvs,
      STANDARD_IN,
      STANDARD_OUT,
      STANDARD_ERR,
      wdir,
      evars,
      proc
    );
    if(p < 0) {
      exit_with_error();
    }
    else {
      printf("%d : launched %lld\n", i, proc->pid);
    }
  }
}