#include <stdio.h>
#include <sys/types.h>
#include <stanza/types.h>
#include <spawn.h>
#include "process.h"

//============================================================
//================= ChildProcess Registration ================
//============================================================

// Holds all metadata for a spawned child process.
// - pid: The process id of the child process itself. 
// - fin/fout/ferr: The stdin/stdout/stderr streams for communicating with the child.
//   NULL if child uses stdin/stdout/stderr directly.
// - status: Pointer to location to write status code when child is terminated.
//   Used by signal handler.
typedef struct ChildProcess {
  pid_t pid;
  FILE* fin;
  FILE* fout;
  FILE* ferr;
  ProcessStatus* pstatus;
} ChildProcess;

// Represents a linked list of ChildProcess.
// - proc: The pointer to the ChildProcess itself.
// - next: The next link in the list. NULL if end of list.
// NOTE: volatile declarations on 'next' forced by compiler.
typedef struct ChildProcessList {
  ChildProcess* proc;
  volatile struct ChildProcessList * next;
} ChildProcessList;

// Linked list of live child processes.
// Will be read from by signal handler.
volatile ChildProcessList * child_processes = NULL;

// Add a new ChildProcess to the global 'child_processes' list.
// Precondition: SIGCHLD is blocked
void add_child_process (ChildProcess* child) {
  ChildProcessList * new_node = (ChildProcessList*)malloc(sizeof(ChildProcessList));
  new_node->proc = child;
  new_node->next = child_processes;
  child_processes = new_node;
}

// Free the ChildProcessList* node, and accompanying structures.
void free_child_process (ChildProcessList* node){
  free(node->proc->pstatus);
  free(node->proc);
  free(node);
}

// Return the ChildProcess with the given process id.
// Returns NULL if there is none.
static ChildProcess* get_child_process (pid_t pid){
  volatile ChildProcessList * curr = child_processes;
  while(curr != NULL && curr->proc->pid != pid)
    curr = curr->next;
  if(curr == NULL) return NULL;
  else return curr->proc;
}

// After a child process is spawned, this function creates a
// ChildProcess struct for recording the child's metadata
// and stores it in the 'child_processes' list.
// Precondition: Assumes that SIGCHLD is blocked.
// - status (output): A newly initialized ProcessStatus
//   is written to the provided pointer.
static void register_child_process (
              pid_t pid,
              FILE* fin,
              FILE* fout,
              FILE* ferr,
              ProcessStatus** status) {

  // Initialize and save ProcessStatus struct.
  ProcessStatus* st = (ProcessStatus*)malloc(sizeof(ProcessStatus));
  st->code_set = 0;
  st->status_code = -1;
  st->referenced_from_stanza = 1;
  *status = st;

  // Initialize ChildProcess struct.
  ChildProcess* child = (ChildProcess*)malloc(sizeof(ChildProcess));
  child->pid = pid;
  child->fin = fin;
  child->fout = fout;
  child->ferr = ferr;
  child->pstatus = st;

  // Store child in ChildProcessList
  add_child_process(child);
}

//============================================================
//================= ChildProcess Operations ==================
//============================================================

// Helper: Return true if the given status code
// represents a process that was terminated, either
// normally (WIFEXITED) or abruptly (WIFSIGNALED).
static bool is_dead_status (stz_int status_code) {
  return WIFSIGNALED(status_code) || WIFEXITED(status_code);
}

// Helper: Return true if the given process is dead.
// If status value is unknown, process is still running
// Otherwise, inspect value.
static bool is_process_dead (ProcessStatus* pstatus) {
  if(pstatus->code_set)
    return is_dead_status(pstatus->status_code);
  else return false;
}

// Helper: Return true if the given process is safe to be
// freed.
static bool is_process_safe_to_free (ProcessStatus* pstatus) {
  return !pstatus->referenced_from_stanza && is_process_dead(pstatus);
}

// Update the status code for the registered child process
// with the given process id.
// Precondition: SIGCHLD is blocked
static void set_child_status (pid_t pid, stz_int status_code) {
  ChildProcess* proc = get_child_process(pid);
  if(proc != NULL) {
    proc->pstatus->code_set = 1;
    proc->pstatus->status_code = status_code;
  }
}

// Update the current status of the given registered child process.
// If the status has changed, then record it in its ChildProcess
// metadata struct.
// Precondition: SIGCHLD is blocked
static void update_child_status (pid_t pid) {
  //Retrieve the status of the given process.
  int status;
  int ret_pid = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

  //waitpid returns a positive integer if the process status has changed.
  if(ret_pid > 0)
    set_child_status(pid, status);
}

// Update the current status of all registered child processes.
// Precondition: SIGCHLD is blocked
static void update_all_child_statuses () {
  volatile ChildProcessList * curr = child_processes;
  while(curr != NULL) {
    update_child_status(curr->proc->pid);
    curr = curr->next;
  }
}

// Remove all ChildProcess nodes that have been terminated from the list.
// Precondition: assumes SIGCHLD is blocked
static void remove_dead_child_processes () {
  volatile ChildProcessList * curr = child_processes;
  volatile ChildProcessList * prev = NULL;
  while(curr != NULL) {
    // Remove curr if its process has died
    if(is_process_safe_to_free(curr->proc->pstatus)) {
      if(prev == NULL) {
        child_processes = curr->next;
        free_child_process((ChildProcessList*) curr);
        curr = child_processes;
      } else {
        prev->next = curr->next;
        free_child_process((ChildProcessList*) curr);
        curr = prev->next;
      }
    } else {
      prev = curr;
      curr = curr->next;
    }
  }
}

//============================================================
//==================== Autoreap Handler ======================
//============================================================

// When the new sigchild handler is installed,
// the existing handler is stored here.
struct sigaction old_sigchild_action;

// This handler automatically uses waitpid to reap registered child processes
// when they terminate.
// Note: This SIGCHLD handler is not re-entrant. It cannot
// be executed if it is already in the middle of executing. SIGCHILD
// must be blocked.
void autoreaping_sigchld_handler(int sig){
  //Update the statuses of all registered child processes.
  update_all_child_statuses();

  //Test whether we are able to forward the signal
  //to the previous signal handler. It must:
  //1. Not be using the sigaction system. (SA_SIGINFO)
  //2. Not be the default signal handler. (SIG_DFL)
  //3. Not be explicitly ignored. (SIG_IGN)
  if(!(old_sigchild_action.sa_flags & SA_SIGINFO) &&
     old_sigchild_action.sa_handler != SIG_DFL &&
     old_sigchild_action.sa_handler != SIG_IGN){
    old_sigchild_action.sa_handler(sig);
  }
}

static char* sighandler_stack;

// This installs the autoreaping signal handler.
void install_autoreaping_sigchld_handler () {
  //Create signal mask containing on SIGCHLD.
  sigset_t sigchld_mask;
  sigemptyset(&sigchld_mask);
  sigaddset(&sigchld_mask, SIGCHLD);

  //Allocate stack and initialize struct for handler
  sighandler_stack = (char*)malloc(SIGSTKSZ * sizeof(char));

  stack_t ss = {
    .ss_size = SIGSTKSZ,
    .ss_sp = sighandler_stack 
  };

  //Setup SIGCHLD handler  .
  //SIGCHLD is blocked during execution of the handler.
  struct sigaction sa = {
    .sa_handler = autoreaping_sigchld_handler,
    .sa_mask = sigchld_mask,
    .sa_flags = SA_RESTART | SA_ONSTACK
  };
  sigaltstack(&ss, 0);
  sigaction(SIGCHLD, &sa, &old_sigchild_action);
}

//============================================================
//================= Signal Handler Utilities =================
//============================================================

//Blocks the SIGCHILD signal by updating the signal mask.
//Returns the previous signal mask.
static sigset_t block_sigchild () {
  //Create signal mask containing only SIGCHLD.
  sigset_t sigchld_mask;
  sigemptyset(&sigchld_mask);
  sigaddset(&sigchld_mask, SIGCHLD);

  //Install the new mask. Exit with error if unsuccessful.
  sigset_t old_mask;
  if(sigprocmask(SIG_BLOCK, &sigchld_mask, &old_mask))
    exit_with_error();

  //Return the previous signal mask.
  return old_mask;
}

//Suspends the current thread until a SIGCHLD
//signal is encountered.
static void suspend_until_sigchild () {
  //Create a signal mask that blocks everything
  //except SIGCHLD.
  sigset_t allow_sigchld;
  sigfillset(&allow_sigchld);
  sigdelset(&allow_sigchld, SIGCHLD);

  //Call sigsuspend, which always returns -1,
  //and which wakes up with errno == EINTR on
  //normal wakeup.
  sigsuspend(&allow_sigchld);
  if(errno != EINTR) exit_with_error();
}

//Restore the signal mask to the given mask.
//Exits program with error if unsuccessful.
static void restore_signal_mask (sigset_t* old_mask)  {
  if(sigprocmask(SIG_SETMASK, old_mask, NULL))
    exit_with_error();
}

//============================================================
//================== Retrieve Process State ==================
//============================================================

//Create a Stanza ProcessState struct from a POSIX status code.
ProcessState make_process_state (ProcessStatus* pstatus){
  if(pstatus->code_set) {
    if(WIFEXITED(pstatus->status_code))
      return (ProcessState){PROCESS_DONE, WEXITSTATUS(pstatus->status_code)};
    else if(WIFSIGNALED(pstatus->status_code))
      return (ProcessState){PROCESS_TERMINATED, WTERMSIG(pstatus->status_code)};
    else if(WIFSTOPPED(pstatus->status_code))
      return (ProcessState){PROCESS_STOPPED, WSTOPSIG(pstatus->status_code)};
    else
      return (ProcessState){PROCESS_RUNNING, 0};
  } else {
    return (ProcessState){PROCESS_RUNNING, 0};
  }
}

// Retrieve the state of the given process.
// - process: The process to retrieve the state of.
// - s: Write out the process state here.
// - wait_for_termination: True if this function should block
//   until the process is terminated.
stz_int retrieve_process_state (Process* process,
                                ProcessState* s,
                                stz_int wait_for_termination){

  //Block SIGCHLD while we're reading the process state.
  sigset_t old_signal_mask = block_sigchild();

  //If we need to wait for termination, then
  //call suspend_until_sigchild repeatedly until
  //process is dead.
  if(wait_for_termination){
    while(!is_process_dead(process->status)){
      suspend_until_sigchild();
    }
  }

  //Store the status code as a Stanza ProcessState.
  *s = make_process_state(process->status);

  //End Block SIGCHLD.
  restore_signal_mask(&old_signal_mask);

  //Return 0 to indicate success.
  //TODO: In this implementation failures halt the program
  //instead of being communicated to caller.
  return 0;
}

//============================================================
//================= OS-X Process Launching ===================
//============================================================
#ifdef PLATFORM_OS_X

//Useful for testing linux implementation on OSX
//extern char **environ;
//int execvpe(const char *program, char **argv, char **envp){
//  char **saved = environ;
//  environ = envp;
//  int rc = execvp(program, argv);
//  environ = saved;
//  return rc;
//}

stz_int launch_process(stz_byte* file, stz_byte** argvs,
                       stz_int input, stz_int output, stz_int error,
                       stz_byte* working_dir, stz_byte** env_vars, Process* process) {
  //Block sigchld.
  sigset_t old_signal_mask = block_sigchild();
  
  //Cleanup any unneeded process metadata.
  remove_dead_child_processes();

  //Compute which pipes to create for the process.
  //has_pipes[PROCESS_IN] = 1, indicates that a process input pipe
  //needs to be created.
  int has_pipes[NUM_STREAM_SPECS];
  for(int i=0; i<NUM_STREAM_SPECS; i++)
    has_pipes[i] = 0;
  has_pipes[input] = 1;
  has_pipes[output] = 1;
  has_pipes[error] = 1;
  has_pipes[STANDARD_IN] = 0;
  has_pipes[STANDARD_OUT] = 0;
  has_pipes[STANDARD_ERR] = 0;

  //Setup file actions.
  //Will be deleted on clean up.
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);

  //Generate pipes for PROCESS_IN, PROCESS_OUT, PROCESS_ERR.
  int pipes[NUM_STREAM_SPECS][2];
  for(int i=0; i<NUM_STREAM_SPECS; i++)
    if(has_pipes[i])
      if(pipe(pipes[i])) goto return_error;

  //Connect process input pipe if necessary.
  if(has_pipes[PROCESS_IN]){
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_IN][1]))
      goto return_error;
    if(posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_IN][0], STDIN_FILENO))
      goto return_error;
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_IN][0]))
      goto return_error;
  }
  //Connect process output pipe if necessary.
  if(has_pipes[PROCESS_OUT]){
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_OUT][0]))
      goto return_error;
    if(output == PROCESS_OUT)
      if(posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_OUT][1], STDOUT_FILENO))
        goto return_error;
    if(error == PROCESS_OUT)
      if(posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_OUT][1], STDERR_FILENO))
        goto return_error;
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_OUT][1]))
      goto return_error;
  }
  //Connect process error pipe if necessary.
  if(has_pipes[PROCESS_ERR]){
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_ERR][0]))
      goto return_error;
    if(error == PROCESS_ERR)
      if(posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_ERR][1], STDERR_FILENO))
        goto return_error;
    if(output == PROCESS_ERR)
      if(posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_ERR][1], STDOUT_FILENO))
        goto return_error;
    if(posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_ERR][1]))
      goto return_error;
  }

  //Setup working directory
  if(working_dir) {
    if(posix_spawn_file_actions_addchdir_np(&actions, C_CSTR(working_dir)))
      goto return_error;
  }

  //Spawn the child process.
  pid_t pid = -1;
  int spawn_ret = posix_spawnp(&pid, C_CSTR(file), &actions, NULL, (char**)argvs, (char**)env_vars);
  if(spawn_ret){
    errno = spawn_ret;
    goto return_error;
  }

  //Set up the pipes in the parent process.
  FILE* fin = NULL;
  if(has_pipes[PROCESS_IN]) {
    close(pipes[PROCESS_IN][0]);
    fin = fdopen(pipes[PROCESS_IN][1], "w");
    if(fin == NULL) goto return_error;
  }
  FILE* fout = NULL;
  if(has_pipes[PROCESS_OUT]) {
    close(pipes[PROCESS_OUT][1]);
    fout = fdopen(pipes[PROCESS_OUT][0], "r");
    if(fout == NULL) goto return_error;
  }
  FILE* ferr = NULL;
  if(has_pipes[PROCESS_ERR]) {
    close(pipes[PROCESS_ERR][1]);
    ferr = fdopen(pipes[PROCESS_ERR][0], "r");
    if(ferr == NULL) goto return_error;
  }

  //Store the process details in the process structure.
  process->pid = pid;
  process->in = fin;
  process->out = fout;
  process->err = ferr;

  //Register the child process so that it is automatically
  //reaped by the autoreap handler.
  register_child_process(pid, fin, fout, ferr, &(process->status));

  //Process spawn was successful.
  goto return_success;

  // Perform cleanup and return -1 to indicate error.
  return_error: {
    posix_spawn_file_actions_destroy(&actions);
    restore_signal_mask(&old_signal_mask);
    return -1;
  }

  // Perform cleanup and return 0 to indicate success.
  return_success: {
    posix_spawn_file_actions_destroy(&actions);
    restore_signal_mask(&old_signal_mask);
    return 0;
  }
}

#endif

//============================================================
//================= Linux Process Launching ==================
//============================================================
#ifdef PLATFORM_LINUX

stz_int launch_process(stz_byte* file, stz_byte** argvs, stz_int input,
                       stz_int output, stz_int error,
                       stz_byte* working_dir, stz_byte** env_vars, Process* process) {

  //Compute which pipes to create for the process.
  //has_pipes[PROCESS_IN] = 1, indicates that a process input pipe
  //needs to be created.
  int has_pipes[NUM_STREAM_SPECS];
  for(int i=0; i<NUM_STREAM_SPECS; i++)
    has_pipes[i] = 0;
  has_pipes[input] = 1;
  has_pipes[output] = 1;
  has_pipes[error] = 1;
  has_pipes[STANDARD_IN] = 0;
  has_pipes[STANDARD_OUT] = 0;
  has_pipes[STANDARD_ERR] = 0;

  //Generate pipes for PROCESS_IN, PROCESS_OUT, PROCESS_ERR.
  int pipes[NUM_STREAM_SPECS][2];
  for(int i=0; i<NUM_STREAM_SPECS; i++)
    if(has_pipes[i])
      if(pipe(pipes[i])) return -1;

  //Create error pipe for receiving status code from
  //child's call to exec.
  int errpipe[2];
  if(pipe(errpipe) < 0) goto return_error;

  //Block SIGCHLD until setup is finished.
  sigset_t old_signal_mask = block_sigchild();

  //Fork child process.
  stz_long pid = (stz_long)vfork();
  if(pid < 0) return -1;

  // Parent: if exec succeeded, open files, register with signal handler
  if(pid > 0) {

    //Read return value of child's call to exec from error pipe.
    //Either child sends back one integer containing the result,
    //or child closes the pipe.
    //Call to read is blocking until either an integer is read, or
    //child closes the pipe.
    close(errpipe[1]);
    int exec_ret = 0;
    int bytes_read = read(errpipe[0], &exec_ret, sizeof(int));
    if(bytes_read > 0) {
      errno = exec_ret;
      goto return_error;
    }

    //Cleanup any unneeded process metadata.
    remove_dead_child_processes();

    //Set up the pipes in the parent process.
    FILE* fin = NULL;
    if(has_pipes[PROCESS_IN]) {
      close(pipes[PROCESS_IN][0]);
      fin = fdopen(pipes[PROCESS_IN][1], "w");
      if(fin == NULL) goto return_error;
    }
    FILE* fout = NULL;
    if(has_pipes[PROCESS_OUT]) {
      close(pipes[PROCESS_OUT][1]);
      fout = fdopen(pipes[PROCESS_OUT][0], "r");
      if(fout == NULL) goto return_error;
    }
    FILE* ferr = NULL;
    if(has_pipes[PROCESS_ERR]) {
      close(pipes[PROCESS_ERR][1]);
      ferr = fdopen(pipes[PROCESS_ERR][0], "r");
      if(ferr == NULL) goto return_error;
    }

    //Store the process details in the process structure.
    process->pid = pid;
    process->in = fin;
    process->out = fout;
    process->err = ferr;

    //Register the child process so that it is automatically
    //repeated by the autoreap handler.
    register_child_process(pid, fin, fout, ferr, &(process->status));

    //Child process successfully launched and registered.
    goto return_success;

    //Perform cleanup and return -1 to indicate error.
    return_error: {
      restore_signal_mask(&old_signal_mask);
      return -1;
    }

    //Perform cleanup and return 0 to indicate success.
    return_success: {
      restore_signal_mask(&old_signal_mask);
      return 0;
    }
  }

  // Child: setup pipes, exec
  else {

    //Setup error pipe. Close the read end, and set close-on-exec so
    //that the pipe is automatically closed if exec completes
    //successfully.
    close(errpipe[0]);
    if(fcntl(errpipe[1], F_SETFD, FD_CLOEXEC) == -1) exit(-1);

    //Connect process input pipe if necessary.
    if(has_pipes[PROCESS_IN]){
      if(close(pipes[PROCESS_IN][1]) < 0) exit(-1);
      if(dup2(pipes[PROCESS_IN][0], STDIN_FILENO) < 0) exit(-1);
      if(close(pipes[PROCESS_IN][0]) < 0) exit(-1);
    }
    //Connect process output pipe if necessary.
    if(has_pipes[PROCESS_OUT]){
      if(close(pipes[PROCESS_OUT][0]) < 0) exit(-1);
      if(output == PROCESS_OUT)
        if(dup2(pipes[PROCESS_OUT][1], STDOUT_FILENO) < 0) exit(-1);
      if(error == PROCESS_OUT)
        if(dup2(pipes[PROCESS_OUT][1], STDERR_FILENO) < 0) exit(-1);
      if(close(pipes[PROCESS_OUT][1]) < 0) exit(-1);
    }
    //Connect process error pipe if necessary.
    if(has_pipes[PROCESS_ERR]){
      if(close(pipes[PROCESS_ERR][0]) < 0) exit(-1);
      if(output == PROCESS_ERR)
        if(dup2(pipes[PROCESS_ERR][1], STDOUT_FILENO) < 0) exit(-1);
      if(error == PROCESS_ERR)
        if(dup2(pipes[PROCESS_ERR][1], STDERR_FILENO) < 0) exit(-1);
      if(close(pipes[PROCESS_ERR][1]) < 0) exit(-1);
    }

    //Setup working directory
    if(working_dir) {
      if(chdir(C_CSTR(working_dir)) < 0) exit(-1);
    }

    //Launch child process.
    //If an environment is supplied then call execvpe, otherwise call execvp.
    int exec_ret;
    if(env_vars == NULL)
      exec_ret = execvp(C_CSTR(file), (char**)argvs);
    else
      exec_ret = execvpe(C_CSTR(file), (char**)argvs, (char**)env_vars);

    //Write the result of exec to the error pipe to indicate that
    //execvp did not work.
    write(errpipe[1], &exec_ret, sizeof(int));

    //Hard stop.
    exit(-1);
  }
}

#endif
