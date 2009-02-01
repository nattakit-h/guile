/* Copyright (C) 2001 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 *
 * As a special exception, the Free Software Foundation gives permission
 * for additional uses of the text contained in its release of GUILE.
 *
 * The exception is that, if you link the GUILE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the GUILE library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by the
 * Free Software Foundation under the name GUILE.  If you copy
 * code from other Free Software Foundation releases into a copy of
 * GUILE, as the General Public License permits, the exception does
 * not apply to the code that you add in this way.  To avoid misleading
 * anyone as to the status of such modified files, you must delete
 * this exception notice from them.
 *
 * If you write modifications of your own for GUILE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.  */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include "vm-bootstrap.h"
#include "instructions.h"
#include "modules.h"
#include "programs.h"
#include "vm.h"


scm_t_bits scm_tc16_program;

static SCM write_program = SCM_BOOL_F;

SCM_DEFINE (scm_make_program, "make-program", 1, 2, 0,
	    (SCM objcode, SCM objtable, SCM external),
	    "")
#define FUNC_NAME s_scm_make_program
{
  SCM_VALIDATE_OBJCODE (1, objcode);
  if (SCM_UNLIKELY (SCM_UNBNDP (objtable)))
    objtable = SCM_BOOL_F;
  else if (scm_is_true (objtable))
    SCM_VALIDATE_VECTOR (2, objtable);
  if (SCM_UNLIKELY (SCM_UNBNDP (external)))
    external = SCM_EOL;
  else
    SCM_VALIDATE_LIST (3, external);

  SCM_RETURN_NEWSMOB3 (scm_tc16_program, objcode, objtable, external);
}
#undef FUNC_NAME

static SCM
program_mark (SCM obj)
{
  if (scm_is_true (SCM_PROGRAM_OBJTABLE (obj)))
    scm_gc_mark (SCM_PROGRAM_OBJTABLE (obj));
  if (!scm_is_null (SCM_PROGRAM_EXTERNALS (obj)))
    scm_gc_mark (SCM_PROGRAM_EXTERNALS (obj));
  return SCM_PROGRAM_OBJCODE (obj);
}

static SCM
program_apply (SCM program, SCM args)
{
  return scm_vm_apply (scm_the_vm (), program, args);
}

static int
program_print (SCM program, SCM port, scm_print_state *pstate)
{
  static int print_error = 0;

  if (SCM_FALSEP (write_program) && scm_module_system_booted_p)
    write_program = scm_module_local_variable
      (scm_c_resolve_module ("system vm program"),
       scm_from_locale_symbol ("write-program"));
  
  if (SCM_FALSEP (write_program) || print_error)
    return scm_smob_print (program, port, pstate);

  print_error = 1;
  scm_call_2 (SCM_VARIABLE_REF (write_program), program, port);
  print_error = 0;
  return 1;
}


/*
 * Scheme interface
 */

SCM_DEFINE (scm_program_p, "program?", 1, 0, 0,
	    (SCM obj),
	    "")
#define FUNC_NAME s_scm_program_p
{
  return SCM_BOOL (SCM_PROGRAM_P (obj));
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_base, "program-base", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_base
{
  SCM_VALIDATE_PROGRAM (1, program);

  return scm_from_ulong ((unsigned long) SCM_PROGRAM_DATA (program)->base);
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_arity, "program-arity", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_arity
{
  struct scm_objcode *p;

  SCM_VALIDATE_PROGRAM (1, program);

  p = SCM_PROGRAM_DATA (program);
  return SCM_LIST4 (SCM_I_MAKINUM (p->nargs),
		    SCM_I_MAKINUM (p->nrest),
		    SCM_I_MAKINUM (p->nlocs),
		    SCM_I_MAKINUM (p->nexts));
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_objects, "program-objects", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_objects
{
  SCM_VALIDATE_PROGRAM (1, program);
  return SCM_PROGRAM_OBJTABLE (program);
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_module, "program-module", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_module
{
  SCM objs;
  SCM_VALIDATE_PROGRAM (1, program);
  objs = SCM_PROGRAM_OBJTABLE (program);
  return scm_is_true (objs) ? scm_c_vector_ref (objs, 0) : SCM_BOOL_F;
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_meta, "program-meta", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_meta
{
  SCM metaobj;
  
  SCM_VALIDATE_PROGRAM (1, program);

  metaobj = scm_objcode_meta (SCM_PROGRAM_OBJCODE (program));
  if (scm_is_true (metaobj))
    return scm_make_program (metaobj, SCM_BOOL_F, SCM_EOL);
  else
    return SCM_BOOL_F;
}
#undef FUNC_NAME

extern SCM
scm_c_program_source (SCM program, size_t ip)
{
  SCM meta, sources, source;

  meta = scm_program_meta (program);
  if (scm_is_false (meta))
    return SCM_BOOL_F;
  meta = scm_call_0 (meta);
  if (scm_is_false (meta))
    return SCM_BOOL_F;
  sources = scm_cadr (meta);
  source = scm_assv (scm_from_size_t (ip), sources);
  if (scm_is_false (source))
    return SCM_BOOL_F;

  return scm_cdr (source); /* a #(line column file) vector */
}

SCM_DEFINE (scm_program_external, "program-external", 1, 0, 0,
	    (SCM program),
	    "")
#define FUNC_NAME s_scm_program_external
{
  SCM_VALIDATE_PROGRAM (1, program);
  return SCM_PROGRAM_EXTERNALS (program);
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_external_set_x, "program-external-set!", 2, 0, 0,
	    (SCM program, SCM external),
	    "Modify the list of closure variables of @var{program} (for "
	    "debugging purposes).")
#define FUNC_NAME s_scm_program_external_set_x
{
  SCM_VALIDATE_PROGRAM (1, program);
  SCM_VALIDATE_LIST (2, external);
  SCM_PROGRAM_EXTERNALS (program) = external;
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE (scm_program_objcode, "program-objcode", 1, 0, 0,
	    (SCM program),
	    "Return a @var{program}'s object code.")
#define FUNC_NAME s_scm_program_objcode
{
  SCM_VALIDATE_PROGRAM (1, program);

  return SCM_PROGRAM_OBJCODE (program);
}
#undef FUNC_NAME



void
scm_bootstrap_programs (void)
{
  scm_tc16_program = scm_make_smob_type ("program", 0);
  scm_set_smob_mark (scm_tc16_program, program_mark);
  scm_set_smob_apply (scm_tc16_program, program_apply, 0, 0, 1);
  scm_set_smob_print (scm_tc16_program, program_print);
}

void
scm_init_programs (void)
{
  scm_bootstrap_vm ();
  
#ifndef SCM_MAGIC_SNARFER
#include "programs.x"
#endif
}

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
