#ifndef RUNTIME_PROCESS_H
#define RUNTIME_PROCESS_H
#include <stdio.h>
#include <stanza/types.h>

//Struct for holding the status code of a terminated/killed/stopped process.
//- code_set: Initialized to 0 to indicate that the process was just launched
//  and status_code is not yet known. Once status_code is known, this field is set to 1.
//- status_code: Holds the POSIX status code of the process as communicated by
//  the SIGCHILD signal. Initialized to -1.
//- referenced_from_stanza: Initialized to 1 to indicate that the Stanza
//  Process wrapper is still alive, and hence the low-level structures cannot
//  be freed yet. When the Stanza Process wrapper is garbage-collected this
//  flag will be set to 0.
typedef struct ProcessStatus {
  stz_int code_set;
  stz_int status_code;
  stz_int referenced_from_stanza; 
} ProcessStatus;

//Represents the Process, and the channels for
//communicating with it.
//- pid: The id of the process. 
//- handle: The Windows handle to the process. Not used by other platforms.
//- in: The standard input stream of the Process.
//- out: The standard output stream of the Process.
//- err: The standard error stream of the Process.
//- status: A pointer to the status code of the Process. Not used on Windows.
typedef struct {
  stz_long pid;
  void* handle;
  FILE* in;
  FILE* out;
  FILE* err;
  ProcessStatus* status;
} Process;

//Represents the state of the process.
//- state: PROCESS_RUNNING | PROCESS_DONE | PROCESS_TERMINATED | PROCESS_STOPPED
//- code: If state is PROCESS_RUNNING, then code is 0, otherwise, state is the
//  exit code of the process.
typedef struct {
  stz_int state;
  stz_int code;
} ProcessState;

#define PROCESS_RUNNING 0
#define PROCESS_DONE 1
#define PROCESS_TERMINATED 2
#define PROCESS_STOPPED 3

#define STANDARD_IN 0
#define STANDARD_OUT 1
#define PROCESS_IN 2
#define PROCESS_OUT 3
#define STANDARD_ERR 4
#define PROCESS_ERR 5
#define NUM_STREAM_SPECS 6

#endif
