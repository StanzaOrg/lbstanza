#ifndef SAFEPOINTS_H
#define SAFEPOINTS_H

#include <inttypes.h>
#include <stdlib.h>

enum {
  NOP = 0x90,
  INT3 = 0xCC
};

typedef const struct {
  uint8_t* const address;
  const uint64_t group;
} SafepointAddress;

typedef const struct {
  const uint64_t length;
  SafepointAddress addresses[];
} AddressList;
static inline void AddressList_write(AddressList* list, const uint8_t inst) {
  for (SafepointAddress *p = list->addresses, *lim = p + list->length; p < lim; p++)
    *p->address = inst;
}
static inline SafepointAddress* AddressList_find(AddressList* list, const void* pc) {
  for (SafepointAddress *p = list->addresses, *lim = p + list->length; p < lim; p++)
    if (p->address == pc)
      return p;
  return NULL;
}

typedef const struct {
  const uint64_t line;
  AddressList* const address_list;
} SafepointEntry;
static inline void SafepointEntry_write(SafepointEntry* entry, const uint8_t inst) {
  AddressList_write(entry->address_list, inst);
}
static inline SafepointAddress* SafepointEntry_find(SafepointEntry* entry, const void* pc) {
  return AddressList_find(entry->address_list, pc);
}

typedef const struct {
  const uint64_t num_entries;
  const char* const filename;
  SafepointEntry entries[];
} FileSafepoints;
static inline void FileSafepoints_write(FileSafepoints* file, const uint8_t inst) {
  for (SafepointEntry *entry = file->entries, *lim = entry + file->num_entries; entry < lim; entry++)
    SafepointEntry_write(entry, inst);
}
static inline SafepointEntry* FileSafepoints_find(FileSafepoints* file, uint64_t line) {
  if (file)
    for (SafepointEntry *entry = file->entries, *lim = entry + file->num_entries; entry < lim; entry++)
      if (entry->line >= line)
        return entry;
  return NULL;
}

typedef const struct {
  const uint64_t num_files;
  FileSafepoints* const files[];
} SafepointTable;
static inline void SafepointTable_write(SafepointTable* safepoints, const uint8_t inst) {
  if (safepoints)
    for (uint64_t i = 0, num_files = safepoints->num_files; i < num_files; i++)
      FileSafepoints_write(safepoints->files[i], inst);
}

#endif // SAFEPOINTS_H
