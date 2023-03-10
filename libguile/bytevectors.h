#ifndef SCM_BYTEVECTORS_H
#define SCM_BYTEVECTORS_H

/* Copyright 2009, 2011, 2018, 2023
     Free Software Foundation, Inc.

   This file is part of Guile.

   Guile is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Guile is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Guile.  If not, see
   <https://www.gnu.org/licenses/>.  */



#include <libguile/error.h>
#include "libguile/gc.h"

#include "libguile/uniform.h"


/* R6RS bytevectors.  */

/* The size in words of the bytevector header (type tag and flags, length,
   and pointer to the underlying buffer).  */
#define SCM_BYTEVECTOR_HEADER_SIZE   4U

#define SCM_BYTEVECTOR_LENGTH(_bv)		\
  ((size_t) SCM_CELL_WORD_1 (_bv))
#define SCM_BYTEVECTOR_CONTENTS(_bv)		\
  ((signed char *) SCM_CELL_WORD_2 (_bv))
#define SCM_BYTEVECTOR_PARENT(_bv)		\
  (SCM_CELL_OBJECT_3 (_bv))


SCM_API SCM scm_endianness_big;
SCM_API SCM scm_endianness_little;

SCM_API SCM scm_c_make_bytevector (size_t);
SCM_API int scm_is_bytevector (SCM);
SCM_API size_t scm_c_bytevector_length (SCM);
SCM_API uint8_t scm_c_bytevector_ref (SCM, size_t);
SCM_API void scm_c_bytevector_set_x (SCM, size_t, uint8_t);

SCM_API SCM scm_make_bytevector (SCM, SCM);
SCM_API SCM scm_bytevector_slice (SCM, SCM, SCM);
SCM_API SCM scm_native_endianness (void);
SCM_API SCM scm_bytevector_p (SCM);
SCM_API SCM scm_bytevector_length (SCM);
SCM_API SCM scm_bytevector_eq_p (SCM, SCM);
SCM_API SCM scm_bytevector_fill_x (SCM, SCM);
SCM_API SCM scm_bytevector_copy_x (SCM, SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_copy (SCM);

SCM_API SCM scm_uniform_array_to_bytevector (SCM);

SCM_API SCM scm_bytevector_to_u8_list (SCM);
SCM_API SCM scm_u8_list_to_bytevector (SCM);
SCM_API SCM scm_uint_list_to_bytevector (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_to_uint_list (SCM, SCM, SCM);
SCM_API SCM scm_sint_list_to_bytevector (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_to_sint_list (SCM, SCM, SCM);

SCM_API SCM scm_bytevector_u16_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_s16_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_u32_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_s32_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_u64_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_s64_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_u8_ref (SCM, SCM);
SCM_API SCM scm_bytevector_s8_ref (SCM, SCM);
SCM_API SCM scm_bytevector_uint_ref (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_sint_ref (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u16_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s16_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u32_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s32_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u64_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s64_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u16_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s16_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u32_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s32_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u64_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s64_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u8_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s8_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_uint_set_x (SCM, SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_sint_set_x (SCM, SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u16_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s16_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u32_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s32_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_u64_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_s64_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_single_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_single_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_ieee_single_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_single_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_double_ref (SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_double_native_ref (SCM, SCM);
SCM_API SCM scm_bytevector_ieee_double_set_x (SCM, SCM, SCM, SCM);
SCM_API SCM scm_bytevector_ieee_double_native_set_x (SCM, SCM, SCM);
SCM_API SCM scm_string_to_utf8 (SCM);
SCM_API SCM scm_string_to_utf16 (SCM, SCM);
SCM_API SCM scm_string_to_utf32 (SCM, SCM);
SCM_API SCM scm_utf8_to_string (SCM);
SCM_API SCM scm_utf16_to_string (SCM, SCM);
SCM_API SCM scm_utf32_to_string (SCM, SCM);



/* Internal API.  */

#define SCM_BYTEVECTOR_P(x)				\
  (SCM_HAS_TYP7 (x, scm_tc7_bytevector))
#define SCM_BYTEVECTOR_FLAGS(_bv)		\
  (SCM_CELL_TYPE (_bv) >> 7UL)
#define SCM_SET_BYTEVECTOR_FLAGS(_bv, _f)				\
  SCM_SET_CELL_TYPE ((_bv),						\
		     scm_tc7_bytevector | ((scm_t_bits)(_f) << 7UL))

#define SCM_F_BYTEVECTOR_CONTIGUOUS 0x100UL
#define SCM_F_BYTEVECTOR_IMMUTABLE 0x200UL

#define SCM_MUTABLE_BYTEVECTOR_P(x)                                     \
  (SCM_NIMP (x) &&                                                      \
   ((SCM_CELL_TYPE (x) & (0x7fUL | (SCM_F_BYTEVECTOR_IMMUTABLE << 7UL)))  \
    == scm_tc7_bytevector))

#define SCM_BYTEVECTOR_ELEMENT_TYPE(_bv)	\
  (SCM_BYTEVECTOR_FLAGS (_bv) & 0xffUL)
#define SCM_BYTEVECTOR_CONTIGUOUS_P(_bv)	\
  (SCM_BYTEVECTOR_FLAGS (_bv) & SCM_F_BYTEVECTOR_CONTIGUOUS)

#define SCM_BYTEVECTOR_TYPE_SIZE(var)                           \
  (scm_i_array_element_type_sizes[SCM_BYTEVECTOR_ELEMENT_TYPE (var)]/8)
#define SCM_BYTEVECTOR_TYPED_LENGTH(var)                        \
  (SCM_BYTEVECTOR_LENGTH (var) / SCM_BYTEVECTOR_TYPE_SIZE (var))

/* Hint that is passed to `scm_gc_malloc ()' and friends.  */
#define SCM_GC_BYTEVECTOR "bytevector"

#define SCM_VALIDATE_BYTEVECTOR(_pos, _obj)			\
  SCM_ASSERT_TYPE (SCM_BYTEVECTOR_P (_obj), (_obj), (_pos),	\
		   FUNC_NAME, "bytevector")

SCM_INTERNAL SCM scm_i_make_typed_bytevector (size_t, scm_t_array_element_type);
SCM_INTERNAL SCM scm_c_take_typed_bytevector (signed char *, size_t,
                                              scm_t_array_element_type, SCM);

SCM_INTERNAL void scm_bootstrap_bytevectors (void);
SCM_INTERNAL void scm_init_bytevectors (void);

SCM_INTERNAL SCM scm_i_native_endianness;
SCM_INTERNAL SCM scm_c_take_gc_bytevector (signed char *, size_t, SCM);

SCM_INTERNAL int scm_i_print_bytevector (SCM, SCM, scm_print_state *);

SCM_INTERNAL SCM scm_c_shrink_bytevector (SCM, size_t);
SCM_INTERNAL void scm_i_bytevector_generalized_set_x (SCM, size_t, SCM);
SCM_INTERNAL SCM scm_null_bytevector;

#endif /* SCM_BYTEVECTORS_H */
