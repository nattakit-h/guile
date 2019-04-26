#include "test.h"

static void
run_test(jit_state_t *j, uint8_t *arena_base, size_t arena_size)
{
  jit_begin(j, arena_base, arena_size);
  jit_load_args_1(j, jit_operand_gpr (JIT_OPERAND_ABI_WORD, JIT_R0));

  jit_extr_f(j, JIT_F0, JIT_R0);
  jit_retr_f(j, JIT_F0);

  float (*f)(jit_word_t) = jit_end(j, NULL);

  ASSERT(f(0) == 0.0f);
  ASSERT(f(1) == 1.0f);
  ASSERT(f(-100) == -100.0f);
}

int
main (int argc, char *argv[])
{
  return main_helper(argc, argv, run_test);
}
