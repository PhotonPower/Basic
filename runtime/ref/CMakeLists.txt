# C reference runtime (docs/runtime.md): defines the semantics, used by the PC target and for
# constant folding; the assembler ports are tested against it.
add_library(basic_rt_ref STATIC
  fp_round_pack.c
  fp_add.c
  fp_mul.c
  fp_div.c
  fp_sqrt.c
  fp_cmp.c
  fp_approx_eq.c
  fp_canon.c
  fp_from_i32.c
  fp_to_int.c
  fp_round_int.c
  fp_round.c
  ix_bn.c
  rt_fmt_flt.c
  rt_val.c
  rt_rnd.c)
target_include_directories(basic_rt_ref PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(basic_rt_ref PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
basic_set_warnings(basic_rt_ref)
