#include "test.h"

static uint8_t data[] = { 0xff, 0x00, 0x42 };

static void
run_test(jit_state_t *j, uint8_t *arena_base, size_t arena_size)
{
  jit_begin(j, arena_base, arena_size);

  jit_ldi_c(j, JIT_R0, &data[0]);
  jit_retr(j, JIT_R0);

  jit_uword_t (*f)(void) = jit_end(j, NULL);

  ASSERT(f() == -1);
}

int
main (int argc, char *argv[])
{
  return main_helper(argc, argv, run_test);
}
