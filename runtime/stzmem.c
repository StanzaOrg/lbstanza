#include "stdmem.h"

void* stz_malloc (stz_long size){
  void* result = malloc(size);
  if(!result){
    fprintf(stderr, "FATAL ERROR: Out of memory.");
    exit(-1);
  }
  return result;
}

void stz_free (void* ptr){
  free(ptr);
}
