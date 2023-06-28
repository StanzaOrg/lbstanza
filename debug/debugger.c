#include <signal.h>
#include "safepoints.h"

SafepointTable* app_safepoint_table;
enum { RUN, STEP, NEXT };
uint8_t run_mode;

static bool all_safepoints_enabled;
static void Safepoints_write(const uint8_t inst) {
  if (app_safepoint_table)
    SafepointTable_write(app_safepoint_table, inst);
}

void Safepoints_enable(void) {
  if (!all_safepoints_enabled) {
    all_safepoints_enabled = true;
    Safepoints_write(INT3);
  }
}
int Safepoints_disable(void) {
  bool enabled = all_safepoints_enabled;
  if (enabled) {
    all_safepoints_enabled = false;
    Safepoints_write(NOP);
  }
  return enabled;
}
void write_breakpoint(SafepointEntry* entry, const uint8_t inst) {
  if (!all_safepoints_enabled)
    SafepointEntry_write(entry, inst);
}

static void handler(int signal) {
  run_mode = STEP;
  Safepoints_enable();
}
void set_SIGINT_handler(void) {
  //Create signal action
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handler;
  sa.sa_flags = SA_ONSTACK | SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL)) {
    perror("SIGINT handler");
    exit(-1);
  }
}
