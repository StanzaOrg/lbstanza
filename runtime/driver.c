#ifdef PLATFORM_WINDOWS
  //This define is necessary as a workaround for accessing CreateSymbolicLink
  //function. This #define is added automatically by the MSVC compiler, but
  //must be added manually when using gcc.
  //Must be defined before including windows.h
  #define _WIN32_WINNT 0x600

  #include<windows.h>
#else
  #include<sys/wait.h>
  #include<sys/mman.h>
#endif
#include<spawn.h>
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

#include <stanza/platform.h>
#include <stanza/types.h>

#include "process.h"

//       Forward Declarations
//       ====================

void* stz_malloc (stz_long size);
void stz_free (void* ptr);

#ifdef PLATFORM_WINDOWS
char* get_windows_api_error() {
  char* lpMsgBuf;
  char* ret;

  FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (char*)&lpMsgBuf,
      0, NULL );

  ret = strdup(lpMsgBuf);
  LocalFree(lpMsgBuf);

  return ret;
}
#endif

#ifdef PLATFORM_WINDOWS
static void exit_with_error_line_and_func (const char* file, int line) {
  fprintf(stderr, "[%s:%d] %s", file, line, get_windows_api_error());
  exit(-1);
}
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_OS_X)
static void exit_with_error_line_and_func (const char* file, int line){
  fprintf(stderr, "[%s:%d] %s\n", file, line, strerror(errno));
  exit(-1);
}
#endif

#define exit_with_error() exit_with_error_line_and_func(__FILE__, __LINE__)

//     Stanza Defined Entities
//     =======================
typedef struct{
  stz_long returnpc;
  stz_long liveness_map;
  stz_long slots[];
} StackFrame;

typedef struct Stack{
  stz_long size;
  StackFrame* frames;
  StackFrame* stack_pointer;
  stz_long pc;
  struct Stack* tail;
} Stack;

typedef struct{
  stz_long current_stack;
  stz_long system_stack;
  stz_byte* heap_top;
  stz_byte* heap_limit;
  stz_byte* heap_start;
  stz_byte* heap_old_objects_end;
  stz_byte* heap_bitset;
  stz_byte* heap_bitset_base;
  stz_long heap_size;
  stz_long heap_size_limit;
  stz_long heap_max_size;
  Stack* stacks;
  void* trackers;
  stz_byte* marking_stack_start;
  stz_byte* marking_stack_bottom;
  stz_byte* marking_stack_top;
} VMInit;

//     Macro Readers
//     =============
FILE* get_stdout () {return stdout;}
FILE* get_stderr () {return stderr;}
FILE* get_stdin () {return stdin;}
stz_int get_eof () {return (stz_int)EOF;}
stz_int get_errno () {return (stz_int)errno;}

//     Time of Day
//     ===========
stz_long current_time_us (void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (stz_long)tv.tv_sec * 1000 * 1000 + (stz_long)tv.tv_usec;
}

stz_long current_time_ms (void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (stz_long)tv.tv_sec * 1000 + (stz_long)tv.tv_usec / 1000;
}

//     Random Access Files
//     ===================
stz_long get_file_size (FILE* f) {
  int64_t cur_pos = ftell(f);
  fseek(f, 0, SEEK_END);
  stz_long size = (stz_long)ftell(f);
  fseek(f, cur_pos, SEEK_SET);
  return size;
}

stz_int file_seek (FILE* f, stz_long pos) {
  return (stz_int)fseek(f, pos, SEEK_SET);
}

stz_int file_skip (FILE* f, stz_long num) {
  return (stz_int)fseek(f, num, SEEK_CUR);
}

stz_int file_set_length (FILE* f, stz_long size) {
  return (stz_int)ftruncate(fileno(f), size);
}

stz_long file_read_block (FILE* f, char* data, stz_long len) {
  return (stz_long)fread(data, 1, len, f);
}

stz_long file_write_block (FILE* f, char* data, stz_long len) {
  return (stz_long)fwrite(data, 1, len, f);
}


//     Path Resolution
//     ===============
#if defined(PLATFORM_LINUX) || defined(PLATFORM_OS_X)
  stz_byte* resolve_path (const stz_byte* filename){
    //Call the Linux realpath function.
    return STZ_STR(realpath(C_CSTR(filename), 0));
  }
#endif

#if defined(PLATFORM_WINDOWS)
  // Return a bitmask that represents which of the 26 letters correspond
  // to valid drive letters.
  stz_int windows_logical_drives_bitmask (){
    return GetLogicalDrives();
  }

  // Resolve a given file path to its fully-resolved ("final") path name.
  // This function tries to return an absolute path with symbolic links
  // resolved. Sometimes it returns an UNC path, which is not usable.
  stz_byte* windows_final_path_name (stz_byte* path){
    // First, open the file (to get a handle to it)
    HANDLE hFile = CreateFile(
        /* lpFileName            */ (LPCSTR)path,
        /* dwDesiredAccess       */ 0,
        /* dwShareMode           */ FILE_SHARE_READ | FILE_SHARE_WRITE,
        /* lpSecurityAttributes  */ NULL,
        /* dwCreationDisposition */ OPEN_EXISTING,
                                 // necessary to open directories
        /* dwFlagsAndAttributes  */ FILE_FLAG_BACKUP_SEMANTICS,
        /* hTemplateFile         */ NULL);

    // Return -1 if a handle cannot be created.
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    // Then resolve it into its fully-resolved ("final") path name
    LPSTR ret = stz_malloc(sizeof(CHAR) * MAX_PATH);
    int numchars = GetFinalPathNameByHandle(hFile, ret, MAX_PATH, FILE_NAME_OPENED);

    // Close handle now that we no longer need it (important to do so!)
    CloseHandle(hFile);

    // Return null if GetFinalPath fails.
    if(numchars == 0){
      stz_free(ret);
      return NULL;
    }

    // Return the path.
    return STZ_STR(ret);
  }

  // Resolve a given file path using its "full" path name.
  // This function tries to return an absolute path. Symbolic
  // links are not resolved.
  stz_byte* windows_full_path_name (stz_byte* filename){
    char* fileext;
    char* path = (char*)stz_malloc(2048);
    int numchars = GetFullPathName((LPCSTR)filename, 2048, path, &fileext);

    // Return null if GetFullPath fails.
    if(numchars == 0){
      stz_free(path);
      return NULL;
    }

    //Return the path
    return path;
  }
#endif

#ifdef PLATFORM_WINDOWS
stz_int symlink(const stz_byte* target, const stz_byte* linkpath) {
  DWORD attributes, flags;

  attributes = GetFileAttributes((LPCSTR)target);
  flags = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ?
    SYMBOLIC_LINK_FLAG_DIRECTORY : 0;

  if (!CreateSymbolicLink((LPCSTR)linkpath, (LPCSTR)target, flags)) {
    return -1;
  }

  return 0;
}

//This function does not follow symbolic links. If we need
//to follow symbolic links, the caller should call this
//call this function with the result of resolve-path.
stz_int get_file_type (const stz_byte* filename0) {
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  LPCSTR filename = C_CSTR(filename0);
  bool is_directory = false,
       is_symlink   = false;

  // First grab the file's attributes
  if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &attributes)) {
    return -1; // Non-existent or inaccessible file
  }

  // Check if it's a directory
  if (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    is_directory = true;
  }

  // Check for possible symlink (reparse point *may* be a symlink)
  if (attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    // To know for sure, find the file and check its reparse tags
    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(filename, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
      return -1;
    }

    if (// Mount point a.k.a Junction (should be treated as a symlink)
        find_data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT ||
        // Actual symlinks (like those created by symlink())
        find_data.dwReserved0 == IO_REPARSE_TAG_SYMLINK) {
      is_symlink = true;
    }

    FindClose(find_handle);
  }

  // Now we can determine what kind of file it is
  if (!is_directory && !is_symlink) {
    return 0; // Regular file
  }
  else if (is_directory && !is_symlink) {
    return 1; // Directory (non-symlink)
  }
  else if (is_symlink) {
    return 2; // Symlink
  }
  else {
    return 3; // Unknown (other)
  }
}
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_OS_X)
stz_int get_file_type (const stz_byte* filename, stz_int follow_sym_links) {
  struct stat filestat;
  int result;
  if(follow_sym_links) result = stat(C_CSTR(filename), &filestat);
  else result = lstat(C_CSTR(filename), &filestat);

  if(result == 0){
    if(S_ISREG(filestat.st_mode))
      return 0;
    else if(S_ISDIR(filestat.st_mode))
      return 1;
    else if(S_ISLNK(filestat.st_mode))
      return 2;
    else
      return 3;
  }
  else{
    return -1;
  }
}

#endif

//     Environment Variable Setting
//     ============================
#ifdef PLATFORM_WINDOWS

  //Retrieve all environment variables as a list.
  extern char** _environ;
  char** get_env_vars (){
    return _environ;
  }

  stz_int setenv (const stz_byte* name, const stz_byte* value, stz_int overwrite) {
    //If we don't want to overwrite previous value, then check whether it exists.
    //If it does, then just return 0.
    if(!overwrite){
      if(getenv(C_CSTR(name)) == 0)
        return 0;
    }
    //(Over)write the environment variable.
    char* buffer = (char*)stz_malloc(strlen(C_CSTR(name)) + strlen(C_CSTR(value)) + 10);
    sprintf(buffer, "%s=%s", C_CSTR(name), C_CSTR(value));
    int r = _putenv(buffer);
    stz_free(buffer);
    return (stz_int)r;
  }

  stz_int unsetenv (const stz_byte* name){
    char* buffer = (char*)stz_malloc(strlen(C_CSTR(name)) + 10);
    sprintf(buffer, "%s=", C_CSTR(name));
    int r = _putenv(buffer);
    stz_free(buffer);
    return (stz_int)r;
  }
#else

  //Retrieve all environment variables as a list.
  extern char** environ;
  char** get_env_vars (){
    return environ;
  }

#endif

//             Time Modified
//             =============

stz_long file_time_modified (const stz_byte* filename){
  struct stat attrib;
  if(stat(C_CSTR(filename), &attrib) == 0)
    return (stz_long)attrib.st_mtime;
  return 0;
}

//============================================================
//===================== String List ==========================
//============================================================

typedef struct {
  stz_int n;
  stz_int capacity;
  stz_byte** strings;
} StringList;

StringList* make_stringlist (stz_int capacity){
  StringList* list = (StringList*)malloc(sizeof(StringList));
  list->n = 0;
  list->capacity = capacity;
  list->strings = (stz_byte**)malloc(capacity * sizeof(stz_byte*));
  return list;
}

static void ensure_stringlist_capacity (StringList* list, stz_int c) {
  if(list->capacity < c){
    stz_int new_capacity = list->capacity;
    while(new_capacity < c) new_capacity *= 2;
    stz_byte** new_strings = (stz_byte**)malloc(new_capacity * sizeof(stz_byte*));
    memcpy(new_strings, list->strings, list->n * sizeof(stz_byte*));
    list->capacity = new_capacity;
    free(list->strings);
    list->strings = new_strings;
  }
}

void free_stringlist (StringList* list){
  for(int i=0; i<list->n; i++)
    free(list->strings[i]);
  free(list->strings);
  free(list);
}

void stringlist_add (StringList* list, const stz_byte* string){
  ensure_stringlist_capacity(list, list->n + 1);
  char* copy = malloc(strlen(C_CSTR(string)) + 1);
  strcpy(copy, C_CSTR(string));
  list->strings[list->n] = STZ_STR(copy);
  list->n++;
}

//============================================================
//================== Directory Handling ======================
//============================================================

StringList* list_dir (const stz_byte* filename){
  //Open directory
  DIR* dir = opendir(C_CSTR(filename));
  if(dir == NULL) return 0;

  //Allocate memory for strings
  StringList* list = make_stringlist(10);
  //Loop through directory entries
  while(1){
    //Read next entry
    struct dirent* entry = readdir(dir);
    if(entry == NULL){
      closedir(dir);
      return list;
    }
    //Notify
    stringlist_add(list, STZ_CSTR(entry->d_name));
  }

  free(list);
  return 0;
}

//============================================================
//===================== Sleeping =============================
//============================================================

stz_int sleep_us (stz_long us){
  struct timespec t1, t2;
  t1.tv_sec = us / 1000000L;
  t1.tv_nsec = (us % 1000000L) * 1000L;
  return (stz_int)nanosleep(&t1, &t2);
}

stz_int sleep_ms (stz_long ms){
  struct timespec t1, t2;
  t1.tv_sec = ms / 1000L;
  t1.tv_nsec = (ms % 1000L) * 1000000L;
  return (stz_int)nanosleep(&t1, &t2);
}

//============================================================
//================= Stanza Memory Allocator ==================
//============================================================

void* stz_malloc (stz_long size){
  return malloc(size);
}

void stz_free (void* ptr){
  free(ptr);
}

//============================================================
//============= Stanza Memory Mapping on POSIX ===============
//============================================================
#if defined(PLATFORM_LINUX) | defined(PLATFORM_OS_X)

//Set protection bits on address range p (inclusive) to p + size (exclusive).
//Fatal error if size > 0 and mprotect fails.
static void protect(void* p, stz_long size, stz_int prot) {
  if (size && mprotect(p, (size_t)size, prot)) exit_with_error();
}

//Allocates a segment of memory that is min_size allocated, and can be
//resized up to max_size.
//This function is called from within Stanza, and min_size and max_size
//are assumed to be multiples of the system page size.
void* stz_memory_map (stz_long min_size, stz_long max_size) {
  void* p = mmap(NULL, (size_t)max_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) exit_with_error();

  protect(p, min_size, PROT_READ | PROT_WRITE | PROT_EXEC);
  return p;
}

//Unmaps the region of memory.
//This function is called from within Stanza, and size is
//assumed to be a multiple of the system page size.
void stz_memory_unmap (void* p, stz_long size) {
  if (p && munmap(p, (size_t)size)) exit_with_error();
}

//Resizes the given segment.
//old_size is assumed to be the size that is already allocated.
//new_size is the size that we desired to be allocated, and
//must be a multiple of the system page size.
void stz_memory_resize (void* p, stz_long old_size, stz_long new_size) {
  stz_long min_size = old_size;
  stz_long max_size = new_size;
  int prot = PROT_READ | PROT_WRITE | PROT_EXEC;

  if (min_size > max_size) {
    min_size = new_size;
    max_size = old_size;
    prot = PROT_NONE;
  }

  protect((char*)p + min_size, max_size - min_size, prot);
}

#endif

//============================================================
//============= Stanza Memory Mapping on Windows =============
//============================================================
#ifdef PLATFORM_WINDOWS

//Allocates a segment of memory that is min_size allocated, and can be
//resized up to max_size.
//This function is called from within Stanza, and min_size and max_size
//are assumed to be multiples of the system page size.
void* stz_memory_map (stz_long min_size, stz_long max_size) {
  // Reserve the max size with no access
  void* p = VirtualAlloc(NULL, (SIZE_T)max_size, MEM_RESERVE, PAGE_NOACCESS);
  if (p == NULL) exit_with_error();

  // Commit the min size with RWX access.
  p = VirtualAlloc(p, (SIZE_T)min_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (p == NULL) exit_with_error();

  // Return the reserved and committed pointer.
  return p;
}

//Unmaps given segment of memory.
//This function is called from within Stanza, and size is
//assumed to be a multiple of the system page size.
void stz_memory_unmap (void* p, stz_long size) {
  // End doing nothing if p is null.
  if (p == NULL) return;

  // Release the memory and fatal if it fails.
  if (!VirtualFree(p, 0, MEM_RELEASE))
    exit_with_error();
}

//Resizes the given segment.
//old_size is assumed to be the size that is already allocated.
//new_size is the size that we desired to be allocated, and
//must be a multiple of the system page size.
void stz_memory_resize (void* p, stz_long old_size, stz_long new_size) {
  //Case: if growing the allocated size.
  if (new_size > old_size) {
    // Growing the allocation: commit all memory pages from the old limit to the new limit.
    if (!VirtualAlloc((char*)p + old_size, (SIZE_T)(new_size - old_size), MEM_COMMIT, PAGE_EXECUTE_READWRITE))
      exit_with_error();
  }
  //Case: if shrinking the allocated size.
  else if(new_size < old_size) {
    // Shrinking the allocation: decommit all memory pages from the new limit to the old limit.
    if (!VirtualFree((char*)p + new_size, (SIZE_T)(old_size - new_size), MEM_DECOMMIT))
      exit_with_error();
  }
}

#endif

//============================================================
//================= Process Runtime ==========================
//============================================================
#if defined(PLATFORM_OS_X) || defined(PLATFORM_LINUX)

//------------------------------------------------------------
//------------------- Structures -----------------------------
//------------------------------------------------------------

//- working_dir: If not null, the working directory of the
//  new process.
//- env_vars: If not null, the environment variables to set in the
//  child process. Each string has format "MYVAR=MYVALUE" and is null-terminated.
//- argvs: The string arguments that are passed to the C main function.
typedef struct {
  stz_byte* pipe;
  stz_byte* in_pipe;
  stz_byte* out_pipe;
  stz_byte* err_pipe;
  stz_byte* file;
  stz_byte* working_dir;
  stz_byte** env_vars;
  stz_byte** argvs;
} EvalArg;

#define RETURN_NEG(x) {int r=(x); if(r < 0) return -1;}

//------------------------------------------------------------
//-------------------- Utilities -----------------------------
//------------------------------------------------------------

static int count_non_null (void** xs){
  int n=0;
  while(xs[n] != NULL)
    n++;
  return n;
}

static char* string_join (const char* a, const char* b){
  int len = strlen(a) + strlen(b);
  char* buffer = (char*)stz_malloc(len + 1);
  sprintf(buffer, "%s%s", a, b);
  return buffer;
}

//Opening a named pipe
static int open_pipe (const char* prefix, const char* suffix, int options){
  char* name = string_join(prefix, suffix);
  int fd = open(name, options);
  stz_free(name);
  return fd;
}

//Creating a named pipe
// TODO: free name
static int make_pipe (char* prefix, char* suffix){
  char* name = string_join(prefix, suffix);
  return mkfifo(name, S_IRUSR|S_IWUSR);
}
#endif

//------------------------------------------------------------
//----------------------- Serialization ----------------------
//------------------------------------------------------------
#if defined(PLATFORM_LINUX) | defined(PLATFORM_OS_X)

// ===== Serialization =====
static void write_int (FILE* f, stz_int x){
  fwrite(&x, sizeof(stz_int), 1, f);
}
static void write_long (FILE* f, stz_long x){
  fwrite(&x, sizeof(stz_long), 1, f);
}
static void write_string (FILE* f, stz_byte* s){
  if(s == NULL)
    write_int(f, -1);
  else{
    size_t n = strlen(C_CSTR(s));
    write_int(f, (stz_int)n);
    fwrite(s, 1, n, f);
  }
}
static void write_strings (FILE* f, stz_byte** s){
  int n = count_non_null((void**)s);
  write_int(f, (stz_int)n);
  for(int i=0; i<n; i++)
    write_string(f, s[i]);
}

//Write a list of strings. The list may be optional, and is
//represented using NULL.
static void write_optional_strings (FILE* f, stz_byte** s){
  if(s == NULL){
    write_int(f, 0);
  }else{
    write_int(f, 1);
    write_strings(f, s);
  }     
}

static void write_process_state (FILE* f, ProcessState* s){
  write_int(f, s->state);
  write_int(f, s->code);
}

// ===== Deserialization =====
static void bread (void* xs0, int size, int n0, FILE* f){
  char* xs = xs0;
  int n = n0;
  while(n > 0){
    int c = fread(xs, size, n, f);
    if(c < n){
      if(ferror(f)) exit_with_error();
      if(feof(f)) return;
    }
    n = n - c;
    xs = xs + size*c;
  }
}
static stz_int read_int (FILE* f){
  stz_int n;
  bread(&n, sizeof(stz_int), 1, f);
  return n;
}
static stz_long read_long (FILE* f){
  stz_long n;
  bread(&n, sizeof(stz_long), 1, f);
  return n;
}
static stz_byte* read_string (FILE* f){
  stz_int n = read_int(f);
  if(n < 0)
    return NULL;
  else{
    stz_byte* s = (stz_byte*)stz_malloc(n + 1);
    bread(s, 1, (int)n, f);
    s[n] = '\0';
    return s;
  }
}
static stz_byte** read_strings (FILE* f){
  stz_int n = read_int(f);
  stz_byte** xs = (stz_byte**)stz_malloc(sizeof(stz_byte*)*(n + 1));
  for(int i=0; i<n; i++)
    xs[i] = read_string(f);
  xs[n] = NULL;
  return xs;
}

static stz_byte** read_optional_strings (FILE* f){
  stz_int flag = read_int(f);
  if(flag == 0){
    return NULL;
  }else{
    return read_strings(f);
  }
}

static EvalArg* read_earg (FILE* f){
  EvalArg* earg = (EvalArg*)stz_malloc(sizeof(EvalArg));
  earg->pipe = read_string(f);
  earg->in_pipe = read_string(f);
  earg->out_pipe = read_string(f);
  earg->err_pipe = read_string(f);
  earg->file = read_string(f);
  earg->working_dir = read_string(f);
  earg->env_vars = read_optional_strings(f);
  earg->argvs = read_strings(f);
  return earg;
}
static void read_process_state (FILE* f, ProcessState* s){
  s->state = read_int(f);
  s->code = read_int(f);
}

//===== Free =====
static void free_strings (stz_byte** ss){
  for(int i=0; ss[i] != NULL; i++)
    stz_free(ss[i]);
  stz_free(ss);
}

static void free_earg (EvalArg* arg){
  stz_free(arg->pipe);
  if(arg->in_pipe != NULL) stz_free(arg->in_pipe);
  if(arg->out_pipe != NULL) stz_free(arg->out_pipe);
  if(arg->err_pipe != NULL) stz_free(arg->err_pipe);
  stz_free(arg->file);
  if(arg->working_dir != NULL) stz_free(arg->working_dir);
  if(arg->env_vars != NULL) free_strings(arg->env_vars);
  free_strings(arg->argvs);
  stz_free(arg);
}

//------------------------------------------------------------
//-------------------- Process Queries -----------------------
//------------------------------------------------------------

static void get_process_state (stz_long pid, ProcessState* s, int wait_for_termination){
  int status;
  int ret = waitpid((pid_t)pid, &status, wait_for_termination? 0 : WNOHANG);

  if(ret == 0)
    *s = (ProcessState){PROCESS_RUNNING, 0};
  else if(WIFEXITED(status))
    *s = (ProcessState){PROCESS_DONE, WEXITSTATUS(status)};
  else if(WIFSIGNALED(status))
    *s = (ProcessState){PROCESS_TERMINATED, WTERMSIG(status)};
  else if(WIFSTOPPED(status))
    *s = (ProcessState){PROCESS_STOPPED, WSTOPSIG(status)};
  else
    *s = (ProcessState){PROCESS_RUNNING, 0};
}

//------------------------------------------------------------
//----------------------- Execvpe ----------------------------
//------------------------------------------------------------
//The 'execvpe' frontend to 'execvp' does not exist in OS-X.
//So implement a quick version of it.

#ifdef PLATFORM_OS_X
extern char **environ;
int execvpe(const char *program, char **argv, char **envp){
  char **saved = environ;
  environ = envp;
  int rc = execvp(program, argv);
  environ = saved;
  return rc;
}
#endif

//------------------------------------------------------------
//---------------------- Launcher Main -----------------------
//------------------------------------------------------------

#define LAUNCH_COMMAND 0
#define STATE_COMMAND 1
#define WAIT_COMMAND 2

static void write_error_and_exit (int fd){
  int code = errno;
  write(fd, &code, sizeof(int));
  close(fd);
  exit(-1);
}

static void make_pipe_name (char* pipe_name, int pipeid) {
  sprintf(pipe_name, "/tmp/stanza_exec_pipe_%ld_%ld", (long)getpid(), (long)pipeid);
}

static int delete_process_pipe (FILE* fd, char* pipe_name, char* suffix) {
  if (fd != NULL) {
    int close_res = fclose(fd);
    if (close_res == EOF) return -1;
    char my_pipe_name[80];
    sprintf(my_pipe_name, "%s%s", pipe_name, suffix);
    int rm_res = remove(my_pipe_name);
    if (rm_res < 0) return -1;
  }
  return 0;
}

stz_int delete_process_pipes (FILE* input, FILE* output, FILE* error, stz_int pipeid) {
  char pipe_name[80];
  make_pipe_name(pipe_name, (int)pipeid);
  //if (delete_process_pipe(input,  pipe_name, "_in") < 0)
  //  return -1;
  //if (delete_process_pipe(output, pipe_name, "_out") < 0)
  //  return -1;
  //if (delete_process_pipe(error,  pipe_name, "_err") < 0)
  //  return -1;
  return 0;
}

#ifdef PLATFORM_OS_X
stz_int launch_process(stz_byte* file, stz_byte** argvs, stz_int input,
                       stz_int output, stz_int error, stz_int pipeid,
                       stz_byte* working_dir, stz_byte** env_vars, Process* process) {
  //Compute pipe sources:
  int pipe_sources[NUM_STREAM_SPECS];
  for(int i=0; i<NUM_STREAM_SPECS; i++)
    pipe_sources[i] = -1;
  pipe_sources[input] = 0;
  pipe_sources[output] = 1;
  pipe_sources[error] = 2;

  //Generate array of pipes per-input-source
  //TODO: close these at some point
  int pipes[NUM_STREAM_SPECS][2];
  for(int i=0; i<NUM_STREAM_SPECS; i++) {
    if(pipe(pipes[i])) return -1;
  }

  //Setup file actions
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);

  int posix_ret;
  // TODO: close all pipes that get dup2'ed
  //Setup input pipe if used
  if(pipe_sources[PROCESS_IN] > 0) {
    // Close write end
    if((posix_ret = posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_IN][1])))
      exit_with_error();
    // dup read to STDIN
    if((posix_ret = posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_IN][0], STDIN_FILENO)))
      exit_with_error();
  }
  //Setup output pipe if used
  if(pipe_sources[PROCESS_OUT] > 0) {
    // Close read end
    if((posix_ret = posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_OUT][0])))
      exit_with_error();
    // dup write to STDOUT
    if((posix_ret = posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_OUT][1], STDOUT_FILENO)))
      exit_with_error();
  }
  //Setup error pipe if used
  if(pipe_sources[PROCESS_ERR] > 0) {
    // Close read end
    if((posix_ret = posix_spawn_file_actions_addclose(&actions, pipes[PROCESS_ERR][0])))
      exit_with_error();
    // dup write to STDERR
    if((posix_ret = posix_spawn_file_actions_adddup2(&actions, pipes[PROCESS_ERR][1], STDERR_FILENO)))
      exit_with_error();
  }
  //Setup working directory
  if(working_dir) {
    if((posix_ret = posix_spawn_file_actions_addchdir_np(&actions, working_dir)))
      exit_with_error();
  }

  // Spawn process
  pid_t pid = -1;
  if(posix_spawn(&pid, file, &actions, NULL, argvs, env_vars) == 0) {
    printf("Child pid: %d\n", pid);
  } else {
    exit_with_error();
  }

  // Cleanup
  posix_spawn_file_actions_destroy(&actions);

  //Parent:
  //Close pipes, setup files
  FILE* fin = NULL;
  if(pipe_sources[PROCESS_IN] > 0) {
    close(pipes[PROCESS_IN][0]);
    fin = fdopen(pipes[PROCESS_IN][1], "w");
    if(fin == NULL) return -1;
  }
  FILE* fout = NULL;
  if(pipe_sources[PROCESS_OUT] > 0) {
    close(pipes[PROCESS_OUT][1]);
    fout = fdopen(pipes[PROCESS_OUT][0], "r");
    if(fout == NULL) return -1;
  }
  FILE* ferr = NULL;
  if(pipe_sources[PROCESS_ERR] > 0) {
    close(pipes[PROCESS_ERR][1]);
    ferr = fdopen(pipes[PROCESS_ERR][0], "r");
    if(ferr == NULL) return -1;
  }

  process->pid = pid;
  process->in = fin;
  process->out = fout;
  process->err = ferr;
  return 0;
}
#endif

//
//
//#ifdef PLATFORM_LINUX
//stz_int launch_process(stz_byte* file, stz_byte** argvs, stz_int input,
//                       stz_int output, stz_int error, stz_int pipeid,
//                       stz_byte* working_dir, stz_byte** env_vars, Process* process) {
//}
//#endif
 
// Old version
//stz_int launch_process(stz_byte* file, stz_byte** argvs, stz_int input,
//                       stz_int output, stz_int error, stz_int pipeid,
//                       stz_byte* working_dir, stz_byte** env_vars, Process* process) {
//  //Figure out unique pipe name
//  char pipe_name[80];
//  make_pipe_name(pipe_name, (int)pipeid);
//
//  //Compute pipe sources
//  int pipe_sources[NUM_STREAM_SPECS];
//  for(int i=0; i<NUM_STREAM_SPECS; i++)
//    pipe_sources[i] = -1;
//  pipe_sources[input] = 0;
//  pipe_sources[output] = 1;
//  pipe_sources[error] = 2;
//
//  //Create pipes to child
//  if(pipe_sources[PROCESS_IN] >= 0)
//    RETURN_NEG(make_pipe(pipe_name, "_in"))
//  if(pipe_sources[PROCESS_OUT] >= 0)
//    RETURN_NEG(make_pipe(pipe_name, "_out"))
//  if(pipe_sources[PROCESS_ERR] >= 0)
//    RETURN_NEG(make_pipe(pipe_name, "_err"))
//
//  stz_byte* in_pipe = NULL;
//  stz_byte* out_pipe = NULL;
//  stz_byte* err_pipe = NULL;
//  
//  int infd  = -1;
//  int outfd = -1;
//  int errfd = -1;
//
//  //Open pipes to child process
//  FILE* fin = NULL;
//  if(pipe_sources[PROCESS_IN] >= 0){
//    infd = open_pipe(pipe_name, "_in", O_RDWR);
//    RETURN_NEG(infd)
//    fin = fdopen(infd, "w");
//    if(fin == NULL) return -1;
//  }
//  FILE* fout = NULL;
//  if(pipe_sources[PROCESS_OUT] >= 0){
//    outfd = open_pipe(pipe_name, "_out", O_RDWR);
//    int fd2 = open_pipe(pipe_name, "_out", O_RDONLY | O_NONBLOCK);
//    printf("outfd = %d, fd2 = %d\n", outfd, fd2);
//    RETURN_NEG(outfd)
//    printf("fout = fdopen(%d) (%s)\n", outfd, string_join(pipe_name, "_out"));
//    fout = fdopen(outfd, "r");
//    if(fout == NULL) return -1;
//  }
//  FILE* ferr = NULL;
//  if(pipe_sources[PROCESS_ERR] >= 0){
//    int errfd = open_pipe(pipe_name, "_err", O_RDWR);
//    RETURN_NEG(errfd)
//    printf("ferr = fdopen(%d) (%s)\n", errfd, string_join(pipe_name, "_err"));
//    ferr = fdopen(errfd, "r");
//    if(ferr == NULL) return -1;
//  }
//
//  // Compute final file descriptors based on pipe sources
//  int final_outfd = outfd;
//  int final_errfd = errfd;
//
//  if(input == PROCESS_IN) in_pipe = STZ_STR("_in");
//  if(output == PROCESS_OUT) {
//    out_pipe = STZ_STR("_out");
//    final_outfd = outfd;
//  }
//  if(output == PROCESS_ERR) {
//    out_pipe = STZ_STR("_err");
//    final_outfd = errfd;
//  }
//  if(error == PROCESS_OUT) {
//    err_pipe = STZ_STR("_out");
//    final_errfd = outfd;
//  }
//  if(error == PROCESS_ERR) {
//    err_pipe = STZ_STR("_err");
//    final_errfd = errfd;
//  }
//
//
//
//  pid_t pid = -1;
//
//  // OS X : use posix_spawn to initialize child process
//  #ifdef PLATFORM_OS_X
//    // Setup
//    posix_spawn_file_actions_t actions;
//    posix_spawn_file_actions_init(&actions);
//    char* child_in_pipe = in_pipe ? string_join(pipe_name, in_pipe) : NULL;
//    char* child_out_pipe = out_pipe ? string_join(pipe_name, out_pipe) : NULL;
//    char* child_err_pipe = err_pipe ? string_join(pipe_name, err_pipe) : NULL;
//    // File actions
//    // TODO: fix all the warnings
//    int posix_ret;
//    if (in_pipe) {
//      if ((posix_spawn_file_actions_addopen(&actions, 0, child_in_pipe, O_RDONLY, 0)))
//        exit_with_error();
//      if ((posix_spawn_file_actions_adddup2(&actions, infd, 0)))
//        exit_with_error();
//    }
//    if (out_pipe) {
//      if ((posix_ret = posix_spawn_file_actions_addopen(&actions, 1, child_out_pipe, O_RDWR, 0666)))
//        exit_with_error();
//      printf("Opened %s to fd 1\n", child_out_pipe);
//      printf("out: dup2(%d, 1), file = %s\n", 4, child_out_pipe);
//      //if ((posix_ret = posix_spawn_file_actions_adddup2(&actions, 4, 1)))
//      //if ((posix_ret = posix_spawn_file_actions_adddup2(&actions, 1, final_outfd)))
//      //if ((posix_ret = posix_spawn_file_actions_adddup2(&actions, final_outfd, 1)))
//      //  exit_with_error();
//    }
//    if (err_pipe) {
//      //if ((posix_ret = posix_spawn_file_actions_addopen(&actions, 5, child_err_pipe, O_RDWR, 0666)))
//      //  exit_with_error();
//      //printf("err: dup2(%d, 2), file = %s\n", 5, child_err_pipe);
//      if ((posix_ret = posix_spawn_file_actions_adddup2(&actions, 2, 1)))
//        exit_with_error();
//      printf("err: dup2(2, 1)\n");
//    }
//    if (working_dir) {
//      printf("working dir: %s\n", working_dir);
//      if ((posix_ret = posix_spawn_file_actions_addchdir_np(&actions, working_dir)))
//        exit_with_error();
//    }
//    // Call spawn
//    if(posix_spawn(&pid, file, &actions, NULL, argvs, env_vars) == 0) {
//      // Parent process
//      printf("Success: child process = %d\n", pid);
//      //int exec_r;
//      //waitpid(pid, &exec_r, 0);
//      //printf("Child process finished\n");
//    } else {
//      exit_with_error();
//    }
//
//    // Clean up spawn resources
//    posix_spawn_file_actions_destroy(&actions);
//    stz_free(child_in_pipe);
//    stz_free(child_out_pipe);
//    stz_free(child_err_pipe);
//  #endif
//
//  // TODO: Linux: use vfork to initialize child process
//  #ifdef PLATFORM_LINUX
//  #endif
//
//  // TODO:
//  //Read back process id, and set errno if failed
//  //stz_long pid = read_long(launcher_out);
//  //if(pid < 0){
//  //  errno = (int)(- pid);
//  //  return -1;
//  //}
//
//  //Return process structure
//  process->pid = pid;
//  process->in = fin;
//  process->out = fout;
//  process->err = ferr;
//  printf("Returning\n");
//  return 0;
//}

int retrieve_process_state (Process* process, ProcessState* s, stz_int wait_for_termination){

  int status;
  int ret = waitpid((pid_t)(process->pid), &status, wait_for_termination? 0 : WNOHANG);
  if(ret == 0)
    *s = (ProcessState){PROCESS_RUNNING, 0};
  else if(WIFEXITED(status))
    *s = (ProcessState){PROCESS_DONE, WEXITSTATUS(status)};
  else if(WIFSIGNALED(status))
    *s = (ProcessState){PROCESS_TERMINATED, WTERMSIG(status)};
  else if(WIFSTOPPED(status))
    *s = (ProcessState){PROCESS_STOPPED, WSTOPSIG(status)};
  else
    *s = (ProcessState){PROCESS_RUNNING, 0};

  return 0;
}
//  stz_int pid = process->pid;
  
//  //Check whether launcher has been initialized
//  if(launcher_pid < 0){
//    fprintf(stderr, "Launcher not initialized.\n");
//    exit(-1);
//  }

//  //Send command
//  int r = fputc(wait_for_termination? WAIT_COMMAND : STATE_COMMAND, launcher_in);
//  if(r == EOF) exit_with_error();
//  write_long(launcher_in, pid);
//  fflush(launcher_in);

//  //Read back process state
//  read_process_state(launcher_out, s);
//  return 0;
//}
#else
#include "process-win32.c"
//============================================================
//============== End Process Runtime =========================
//============================================================
#endif

#define STACK_TYPE 6

stz_long stanza_entry (VMInit* init);

//     Command line arguments
//     ======================
stz_int input_argc;
stz_byte** input_argv;
stz_int input_argv_needs_free;

//     Main Driver
//     ===========
static void* alloc (VMInit* init, long tag, long size){
  void* ptr = init->heap_top + 8;
  *(long*)(init->heap_top) = tag;
  init->heap_top += 8 + size;
  return ptr;
}

static Stack* alloc_stack (VMInit* init){
  Stack* stack = alloc(init, STACK_TYPE, sizeof(Stack));
  stz_long initial_stack_size = 8 * 1024;
  StackFrame* frames = (StackFrame*)stz_malloc(initial_stack_size);
  stack->size = initial_stack_size;
  stack->frames = frames;
  stack->stack_pointer = NULL;
  stack->tail = NULL;
  return stack;
}

//Given a pointer to a struct allocated on the heap,
//add the tag bits to the pointer.
uint64_t tag_as_ref (void* p){
  return (uint64_t)p - 8 + 1;
}

enum {
  LOG_BITS_IN_BYTE = 3,
  LOG_BYTES_IN_LONG = 3,
  LOG_BITS_IN_LONG = LOG_BYTES_IN_LONG + LOG_BITS_IN_BYTE,
  BYTES_IN_LONG = 1 << LOG_BYTES_IN_LONG,
  BITS_IN_LONG = 1 << LOG_BITS_IN_LONG
};

#define SYSTEM_PAGE_SIZE 4096ULL
#define ROUND_UP_TO_WHOLE_PAGES(x) (((x) + (SYSTEM_PAGE_SIZE - 1)) & ~(SYSTEM_PAGE_SIZE - 1))
#define ROUND_UP_TO_WHOLE_LONGS(x) (((x) + (sizeof(stz_long) - 1)) & ~(sizeof(stz_long) - 1))

static stz_long bitset_size (stz_long heap_size) {
  uint64_t heap_size_in_longs = (heap_size + (BYTES_IN_LONG - 1)) >> LOG_BYTES_IN_LONG;
  uint64_t bitset_size_in_longs = (heap_size_in_longs + (BITS_IN_LONG - 1)) >> LOG_BITS_IN_LONG;
  return ROUND_UP_TO_WHOLE_PAGES(bitset_size_in_longs << LOG_BYTES_IN_LONG);
}

//Use 'main' as the standard name for the C main function unless
//RENAME_STANZA_MAIN is passed as a flag. If it is, then rename 'main'
//to 'stanza_main'.
#ifdef RENAME_STANZA_MAIN
  #define MAIN_FUNC stanza_main
#else
  #define MAIN_FUNC main
#endif

STANZA_API_FUNC int MAIN_FUNC (int argc, char* argv[]) {
  input_argc = (stz_int)argc;
  input_argv = (stz_byte **)argv;
  input_argv_needs_free = 0;
  VMInit init;

  //Allocate heap
  const stz_long min_heap_size = ROUND_UP_TO_WHOLE_PAGES(8 * 1024 * 1024);
  const stz_long max_heap_size = ROUND_UP_TO_WHOLE_PAGES(STZ_LONG(8) * 1024 * 1024 * 1024);
  init.heap_start = (stz_byte*)stz_memory_map(min_heap_size, max_heap_size);
  init.heap_max_size = max_heap_size;
  init.heap_size_limit = max_heap_size;
  init.heap_size = min_heap_size;

  //Setup the nursery
  const stz_long nursery_fraction = 8; // Must match the value in core.stanza
  const stz_long nursery_size = ROUND_UP_TO_WHOLE_LONGS(min_heap_size / nursery_fraction / 2);
  init.heap_old_objects_end = init.heap_start;
  init.heap_top = init.heap_old_objects_end + nursery_size;
  init.heap_limit = init.heap_top + nursery_size;

  //Allocate bitset for heap
  const stz_long min_bitset_size = bitset_size(min_heap_size);
  const stz_long max_bitset_size = bitset_size(max_heap_size);
  init.heap_bitset = (stz_byte*)stz_memory_map(min_bitset_size, max_bitset_size);
  init.heap_bitset_base = init.heap_bitset - ((uint64_t)init.heap_start >> 6);
  memset(init.heap_bitset, 0, min_bitset_size);

  //For bitset_base computation to work: bitset must be aligned to 512-bytes boundary.
  if((uint64_t)init.heap_bitset % 512 != 0){
    fprintf(stderr, "Unaligned bitset: %p.\n", init.heap_bitset);
    exit(-1);
  }

  //Allocate marking stack for heap
  const stz_long marking_stack_size = ROUND_UP_TO_WHOLE_PAGES((1024 * 1024L) << LOG_BYTES_IN_LONG);
  init.marking_stack_start = stz_memory_map(marking_stack_size, marking_stack_size);
  init.marking_stack_bottom = init.marking_stack_start + marking_stack_size;
  init.marking_stack_top = init.marking_stack_bottom;

  //Allocate stacks
  Stack* entry_stack = alloc_stack(&init);
  Stack* entry_system_stack = alloc_stack(&init);
  entry_stack->tail = entry_system_stack;
  init.current_stack = tag_as_ref(entry_stack);
  init.system_stack = tag_as_ref(entry_system_stack);
  init.stacks = entry_stack;

  //Initialize trackers to empty list.
  init.trackers = NULL;

  //Call Stanza entry
  stanza_entry(&init);

  //Heap and freespace are disposed by OS at process termination
  return 0;
}
