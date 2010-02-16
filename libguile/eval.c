/* Copyright (C) 1995,1996,1997,1998,1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010
 * Free Software Foundation, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */



#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <alloca.h>

#include "libguile/__scm.h"

#include "libguile/_scm.h"
#include "libguile/alist.h"
#include "libguile/async.h"
#include "libguile/continuations.h"
#include "libguile/debug.h"
#include "libguile/deprecation.h"
#include "libguile/dynwind.h"
#include "libguile/eq.h"
#include "libguile/feature.h"
#include "libguile/fluids.h"
#include "libguile/goops.h"
#include "libguile/hash.h"
#include "libguile/hashtab.h"
#include "libguile/lang.h"
#include "libguile/list.h"
#include "libguile/macros.h"
#include "libguile/memoize.h"
#include "libguile/modules.h"
#include "libguile/ports.h"
#include "libguile/print.h"
#include "libguile/procprop.h"
#include "libguile/programs.h"
#include "libguile/root.h"
#include "libguile/smob.h"
#include "libguile/srcprop.h"
#include "libguile/stackchk.h"
#include "libguile/strings.h"
#include "libguile/threads.h"
#include "libguile/throw.h"
#include "libguile/validate.h"
#include "libguile/values.h"
#include "libguile/vectors.h"
#include "libguile/vm.h"

#include "libguile/eval.h"
#include "libguile/private-options.h"




/* We have three levels of EVAL here:

   - eval (exp, env)

     evaluates EXP in environment ENV.  ENV is a lexical environment
     structure as used by the actual tree code evaluator.  When ENV is
     a top-level environment, then changes to the current module are
     tracked by updating ENV so that it continues to be in sync with
     the current module.

   - scm_primitive_eval (exp)

     evaluates EXP in the top-level environment as determined by the
     current module.  This is done by constructing a suitable
     environment and calling eval.  Thus, changes to the
     top-level module are tracked normally.

   - scm_eval (exp, mod)

     evaluates EXP while MOD is the current module. This is done
     by setting the current module to MOD_OR_STATE, invoking
     scm_primitive_eval on EXP, and then restoring the current module
     to the value it had previously.  That is, while EXP is evaluated,
     changes to the current module (or dynamic state) are tracked,
     but these changes do not persist when scm_eval returns.

*/


/* Boot closures. We only see these when compiling eval.scm, because once
   eval.scm is in the house, closures are standard VM closures.
 */

static scm_t_bits scm_tc16_boot_closure;
#define RETURN_BOOT_CLOSURE(code, env) SCM_RETURN_NEWSMOB2 (scm_tc16_boot_closure, (code), (env))
#define BOOT_CLOSURE_P(obj) SCM_TYP16_PREDICATE (scm_tc16_boot_closure, (obj))
#define BOOT_CLOSURE_CODE(x) SCM_SMOB_OBJECT (x)
#define BOOT_CLOSURE_ENV(x) SCM_SMOB_OBJECT_2 (x)
#define BOOT_CLOSURE_NUM_REQUIRED_ARGS(x) SCM_I_INUM (CAR (BOOT_CLOSURE_CODE (x)))
#define BOOT_CLOSURE_HAS_REST_ARGS(x) scm_is_true (CADR (BOOT_CLOSURE_CODE (x)))
#define BOOT_CLOSURE_BODY(x) CDDR (BOOT_CLOSURE_CODE (x))



#if 0
#define CAR(x)   SCM_CAR(x)
#define CDR(x)   SCM_CDR(x)
#define CAAR(x)  SCM_CAAR(x)
#define CADR(x)  SCM_CADR(x)
#define CDAR(x)  SCM_CDAR(x)
#define CDDR(x)  SCM_CDDR(x)
#define CADDR(x) SCM_CADDR(x)
#define CDDDR(x) SCM_CDDDR(x)
#else
#define CAR(x)   scm_car(x)
#define CDR(x)   scm_cdr(x)
#define CAAR(x)  scm_caar(x)
#define CADR(x)  scm_cadr(x)
#define CDAR(x)  scm_cdar(x)
#define CDDR(x)  scm_cddr(x)
#define CADDR(x) scm_caddr(x)
#define CDDDR(x) scm_cdddr(x)
#endif


SCM_SYMBOL (scm_unbound_variable_key, "unbound-variable");

static void error_used_before_defined (void)
{
  scm_error (scm_unbound_variable_key, NULL,
             "Variable used before given a value", SCM_EOL, SCM_BOOL_F);
}

int
scm_badargsp (SCM formals, SCM args)
{
  while (!scm_is_null (formals))
    {
      if (!scm_is_pair (formals)) 
        return 0;
      if (scm_is_null (args)) 
        return 1;
      formals = CDR (formals);
      args = CDR (args);
    }
  return !scm_is_null (args) ? 1 : 0;
}

/* the environment:
   (VAL ... . MOD)
   If MOD is #f, it means the environment was captured before modules were
   booted.
   If MOD is the literal value '(), we are evaluating at the top level, and so
   should track changes to the current module. You have to be careful in this
   case, because further lexical contours should capture the current module.
*/
#define CAPTURE_ENV(env)                                        \
  ((env == SCM_EOL) ? scm_current_module () :                   \
   ((env == SCM_BOOL_F) ? scm_the_root_module () : env))

static SCM
eval (SCM x, SCM env)
{
  SCM mx;
  SCM proc = SCM_UNDEFINED, args = SCM_EOL;

 loop:
  SCM_TICK;
  if (!SCM_MEMOIZED_P (x))
    abort ();
  
  mx = SCM_MEMOIZED_ARGS (x);
  switch (SCM_MEMOIZED_TAG (x))
    {
    case SCM_M_BEGIN:
      for (; !scm_is_null (CDR (mx)); mx = CDR (mx))
        eval (CAR (mx), env);
      x = CAR (mx);
      goto loop;

    case SCM_M_IF:
      if (scm_is_true (eval (CAR (mx), env)))
        x = CADR (mx);
      else
        x = CDDR (mx);
      goto loop;

    case SCM_M_LET:
      {
        SCM inits = CAR (mx);
        SCM new_env = CAPTURE_ENV (env);
        for (; scm_is_pair (inits); inits = CDR (inits))
          new_env = scm_cons (eval (CAR (inits), env), new_env);
        env = new_env;
        x = CDR (mx);
        goto loop;
      }
          
    case SCM_M_LAMBDA:
      RETURN_BOOT_CLOSURE (mx, CAPTURE_ENV (env));

    case SCM_M_QUOTE:
      return mx;

    case SCM_M_DEFINE:
      scm_define (CAR (mx), eval (CDR (mx), env));
      return SCM_UNSPECIFIED;

    case SCM_M_DYNWIND:
      {
        SCM in, out, res, old_winds;
        in = eval (CAR (mx), env);
        out = eval (CDDR (mx), env);
        scm_call_0 (in);
        old_winds = scm_i_dynwinds ();
        scm_i_set_dynwinds (scm_acons (in, out, old_winds));
        res = eval (CADR (mx), env);
        scm_i_set_dynwinds (old_winds);
        scm_call_0 (out);
        return res;
      }

    case SCM_M_APPLY:
      /* Evaluate the procedure to be applied.  */
      proc = eval (CAR (mx), env);
      /* Evaluate the argument holding the list of arguments */
      args = eval (CADR (mx), env);
          
    apply_proc:
      /* Go here to tail-apply a procedure.  PROC is the procedure and
       * ARGS is the list of arguments. */
      if (BOOT_CLOSURE_P (proc))
        {
          int nreq = BOOT_CLOSURE_NUM_REQUIRED_ARGS (proc);
          SCM new_env = BOOT_CLOSURE_ENV (proc);
          if (BOOT_CLOSURE_HAS_REST_ARGS (proc))
            {
              if (SCM_UNLIKELY (scm_ilength (args) < nreq))
                scm_wrong_num_args (proc);
              for (; nreq; nreq--, args = CDR (args))
                new_env = scm_cons (CAR (args), new_env);
              new_env = scm_cons (args, new_env);
            }
          else
            {
              if (SCM_UNLIKELY (scm_ilength (args) != nreq))
                scm_wrong_num_args (proc);
              for (; scm_is_pair (args); args = CDR (args))
                new_env = scm_cons (CAR (args), new_env);
            }
          x = BOOT_CLOSURE_BODY (proc);
          env = new_env;
          goto loop;
        }
      else
        return scm_vm_apply (scm_the_vm (), proc, args);

    case SCM_M_CALL:
      /* Evaluate the procedure to be applied.  */
      proc = eval (CAR (mx), env);
      /* int nargs = CADR (mx); */
      mx = CDDR (mx);

      if (BOOT_CLOSURE_P (proc))
        {
          int nreq = BOOT_CLOSURE_NUM_REQUIRED_ARGS (proc);
          SCM new_env = BOOT_CLOSURE_ENV (proc);
          if (BOOT_CLOSURE_HAS_REST_ARGS (proc))
            {
              if (SCM_UNLIKELY (scm_ilength (mx) < nreq))
                scm_wrong_num_args (proc);
              for (; nreq; nreq--, mx = CDR (mx))
                new_env = scm_cons (eval (CAR (mx), env), new_env);
              {
                SCM rest = SCM_EOL;
                for (; scm_is_pair (mx); mx = CDR (mx))
                  rest = scm_cons (eval (CAR (mx), env), rest);
                new_env = scm_cons (scm_reverse (rest),
                                    new_env);
              }
            }
          else
            {
              for (; scm_is_pair (mx); mx = CDR (mx), nreq--)
                new_env = scm_cons (eval (CAR (mx), env), new_env);
              if (SCM_UNLIKELY (nreq != 0))
                scm_wrong_num_args (proc);
            }
          x = BOOT_CLOSURE_BODY (proc);
          env = new_env;
          goto loop;
        }
      else
        {
          SCM rest = SCM_EOL;
          /* FIXME: use alloca */
          for (; scm_is_pair (mx); mx = CDR (mx))
            rest = scm_cons (eval (CAR (mx), env), rest);
          return scm_vm_apply (scm_the_vm (), proc, scm_reverse (rest));
        }
          
    case SCM_M_CONT:
      return scm_i_call_with_current_continuation (eval (mx, env));

    case SCM_M_CALL_WITH_VALUES:
      {
        SCM producer;
        SCM v;

        producer = eval (CAR (mx), env);
        proc = eval (CDR (mx), env);  /* proc is the consumer. */
        v = scm_vm_apply (scm_the_vm (), producer, SCM_EOL);
        if (SCM_VALUESP (v))
          args = scm_struct_ref (v, SCM_INUM0);
        else
          args = scm_list_1 (v);
        goto apply_proc;
      }

    case SCM_M_LEXICAL_REF:
      {
        int n;
        SCM ret;
        for (n = SCM_I_INUM (mx); n; n--)
          env = CDR (env);
        ret = CAR (env);
        if (SCM_UNLIKELY (SCM_UNBNDP (ret)))
          /* we don't know what variable, though, because we don't have its
             name */
          error_used_before_defined ();
        return ret;
      }

    case SCM_M_LEXICAL_SET:
      {
        int n;
        SCM val = eval (CDR (mx), env);
        for (n = SCM_I_INUM (CAR (mx)); n; n--)
          env = CDR (env);
        SCM_SETCAR (env, val);
        return SCM_UNSPECIFIED;
      }

    case SCM_M_TOPLEVEL_REF:
      if (SCM_VARIABLEP (mx))
        return SCM_VARIABLE_REF (mx);
      else
        {
          while (scm_is_pair (env))
            env = scm_cdr (env);
          return SCM_VARIABLE_REF
            (scm_memoize_variable_access_x (x, CAPTURE_ENV (env)));
        }

    case SCM_M_TOPLEVEL_SET:
      {
        SCM var = CAR (mx);
        SCM val = eval (CDR (mx), env);
        if (SCM_VARIABLEP (var))
          {
            SCM_VARIABLE_SET (var, val);
            return SCM_UNSPECIFIED;
          }
        else
          {
            while (scm_is_pair (env))
              env = scm_cdr (env);
            SCM_VARIABLE_SET
              (scm_memoize_variable_access_x (x, CAPTURE_ENV (env)),
               val);
            return SCM_UNSPECIFIED;
          }
      }

    case SCM_M_MODULE_REF:
      if (SCM_VARIABLEP (mx))
        return SCM_VARIABLE_REF (mx);
      else
        return SCM_VARIABLE_REF
          (scm_memoize_variable_access_x (x, SCM_BOOL_F));

    case SCM_M_MODULE_SET:
      if (SCM_VARIABLEP (CDR (mx)))
        {
          SCM_VARIABLE_SET (CDR (mx), eval (CAR (mx), env));
          return SCM_UNSPECIFIED;
        }
      else
        {
          SCM_VARIABLE_SET
            (scm_memoize_variable_access_x (x, SCM_BOOL_F),
             eval (CAR (mx), env));
          return SCM_UNSPECIFIED;
        }

    default:
      abort ();
    }
}

scm_t_option scm_eval_opts[] = {
  { SCM_OPTION_INTEGER, "stack", 22000, "Size of thread stacks (in machine words)." },
  { 0 }
};

scm_t_option scm_debug_opts[] = {
  { SCM_OPTION_BOOLEAN, "cheap", 1,
    "*This option is now obsolete.  Setting it has no effect." },
  { SCM_OPTION_BOOLEAN, "breakpoints", 0, "*Check for breakpoints." },
  { SCM_OPTION_BOOLEAN, "trace", 0, "*Trace mode." },
  { SCM_OPTION_BOOLEAN, "procnames", 1,
    "Record procedure names at definition." },
  { SCM_OPTION_BOOLEAN, "backwards", 0,
    "Display backtrace in anti-chronological order." },
  { SCM_OPTION_INTEGER, "width", 79, "Maximal width of backtrace." },
  { SCM_OPTION_INTEGER, "indent", 10, "Maximal indentation in backtrace." },
  { SCM_OPTION_INTEGER, "frames", 3,
    "Maximum number of tail-recursive frames in backtrace." },
  { SCM_OPTION_INTEGER, "maxdepth", 1000,
    "Maximal number of stored backtrace frames." },
  { SCM_OPTION_INTEGER, "depth", 20, "Maximal length of printed backtrace." },
  { SCM_OPTION_BOOLEAN, "backtrace", 0, "Show backtrace on error." },
  { SCM_OPTION_BOOLEAN, "debug", 0, "Use the debugging evaluator." },
  /* This default stack limit will be overridden by debug.c:init_stack_limit(),
     if we have getrlimit() and the stack limit is not INFINITY. But it is still
     important, as some systems have both the soft and the hard limits set to
     INFINITY; in that case we fall back to this value.

     The situation is aggravated by certain compilers, which can consume
     "beaucoup de stack", as they say in France.

     See http://thread.gmane.org/gmane.lisp.guile.devel/8599/focus=8662 for
     more discussion. This setting is 640 KB on 32-bit arches (should be enough
     for anyone!) or a whoppin' 1280 KB on 64-bit arches.
  */
  { SCM_OPTION_INTEGER, "stack", 160000, "Stack size limit (measured in words; 0 = no check)." },
  { SCM_OPTION_SCM, "show-file-name", (unsigned long)SCM_BOOL_T,
    "Show file names and line numbers "
    "in backtraces when not `#f'.  A value of `base' "
    "displays only base names, while `#t' displays full names."},
  { SCM_OPTION_BOOLEAN, "warn-deprecated", 0,
    "Warn when deprecated features are used." },
  { 0 }, 
};


/*
 * this ordering is awkward and illogical, but we maintain it for
 * compatibility. --hwn
 */
scm_t_option scm_evaluator_trap_table[] = {
  { SCM_OPTION_BOOLEAN, "traps", 0, "Enable evaluator traps." },
  { SCM_OPTION_BOOLEAN, "enter-frame", 0, "Trap when eval enters new frame." },
  { SCM_OPTION_BOOLEAN, "apply-frame", 0, "Trap when entering apply." },
  { SCM_OPTION_BOOLEAN, "exit-frame", 0, "Trap when exiting eval or apply." },
  { SCM_OPTION_SCM, "enter-frame-handler", (unsigned long)SCM_BOOL_F, "Handler for enter-frame traps." },
  { SCM_OPTION_SCM, "apply-frame-handler", (unsigned long)SCM_BOOL_F, "Handler for apply-frame traps." },
  { SCM_OPTION_SCM, "exit-frame-handler", (unsigned long)SCM_BOOL_F, "Handler for exit-frame traps." },
  { SCM_OPTION_BOOLEAN, "memoize-symbol", 0, "Trap when memoizing a symbol." },
  { SCM_OPTION_SCM, "memoize-symbol-handler", (unsigned long)SCM_BOOL_F, "The handler for memoization." },
  { 0 }
};


SCM_DEFINE (scm_eval_options_interface, "eval-options-interface", 0, 1, 0, 
            (SCM setting),
	    "Option interface for the evaluation options. Instead of using\n"
	    "this procedure directly, use the procedures @code{eval-enable},\n"
	    "@code{eval-disable}, @code{eval-set!} and @code{eval-options}.")
#define FUNC_NAME s_scm_eval_options_interface
{
  SCM ans;
  
  scm_dynwind_begin (0);
  scm_dynwind_critical_section (SCM_BOOL_F);
  ans = scm_options (setting,
		     scm_eval_opts,
		     FUNC_NAME);
  scm_dynwind_end ();

  return ans;
}
#undef FUNC_NAME


SCM_DEFINE (scm_evaluator_traps, "evaluator-traps-interface", 0, 1, 0, 
            (SCM setting),
	    "Option interface for the evaluator trap options.")
#define FUNC_NAME s_scm_evaluator_traps
{
  SCM ans;

  
  scm_options_try (setting,
		   scm_evaluator_trap_table,
		   FUNC_NAME, 1);
  SCM_CRITICAL_SECTION_START;
  ans = scm_options (setting,
		     scm_evaluator_trap_table,
		     FUNC_NAME);

  /* njrev: same again. */
  SCM_CRITICAL_SECTION_END;
  return ans;
}
#undef FUNC_NAME





/* Simple procedure calls
 */

SCM
scm_call_0 (SCM proc)
{
  return scm_c_vm_run (scm_the_vm (), proc, NULL, 0);
}

SCM
scm_call_1 (SCM proc, SCM arg1)
{
  return scm_c_vm_run (scm_the_vm (), proc, &arg1, 1);
}

SCM
scm_call_2 (SCM proc, SCM arg1, SCM arg2)
{
  SCM args[] = { arg1, arg2 };
  return scm_c_vm_run (scm_the_vm (), proc, args, 2);
}

SCM
scm_call_3 (SCM proc, SCM arg1, SCM arg2, SCM arg3)
{
  SCM args[] = { arg1, arg2, arg3 };
  return scm_c_vm_run (scm_the_vm (), proc, args, 3);
}

SCM
scm_call_4 (SCM proc, SCM arg1, SCM arg2, SCM arg3, SCM arg4)
{
  SCM args[] = { arg1, arg2, arg3, arg4 };
  return scm_c_vm_run (scm_the_vm (), proc, args, 4);
}

SCM
scm_call_n (SCM proc, SCM *argv, size_t nargs)
{
  return scm_c_vm_run (scm_the_vm (), proc, argv, nargs);
}

/* Simple procedure applies
 */

SCM
scm_apply_0 (SCM proc, SCM args)
{
  return scm_apply (proc, args, SCM_EOL);
}

SCM
scm_apply_1 (SCM proc, SCM arg1, SCM args)
{
  return scm_apply (proc, scm_cons (arg1, args), SCM_EOL);
}

SCM
scm_apply_2 (SCM proc, SCM arg1, SCM arg2, SCM args)
{
  return scm_apply (proc, scm_cons2 (arg1, arg2, args), SCM_EOL);
}

SCM
scm_apply_3 (SCM proc, SCM arg1, SCM arg2, SCM arg3, SCM args)
{
  return scm_apply (proc, scm_cons (arg1, scm_cons2 (arg2, arg3, args)),
		    SCM_EOL);
}

/* This code processes the arguments to apply:

   (apply PROC ARG1 ... ARGS)

   Given a list (ARG1 ... ARGS), this function conses the ARG1
   ... arguments onto the front of ARGS, and returns the resulting
   list.  Note that ARGS is a list; thus, the argument to this
   function is a list whose last element is a list.

   Apply calls this function, and applies PROC to the elements of the
   result.  apply:nconc2last takes care of building the list of
   arguments, given (ARG1 ... ARGS).

   Rather than do new consing, apply:nconc2last destroys its argument.
   On that topic, this code came into my care with the following
   beautifully cryptic comment on that topic: "This will only screw
   you if you do (scm_apply scm_apply '( ... ))"  If you know what
   they're referring to, send me a patch to this comment.  */

SCM_DEFINE (scm_nconc2last, "apply:nconc2last", 1, 0, 0, 
	    (SCM lst),
	    "Given a list (@var{arg1} @dots{} @var{args}), this function\n"
	    "conses the @var{arg1} @dots{} arguments onto the front of\n"
	    "@var{args}, and returns the resulting list. Note that\n"
	    "@var{args} is a list; thus, the argument to this function is\n"
	    "a list whose last element is a list.\n"
	    "Note: Rather than do new consing, @code{apply:nconc2last}\n"
	    "destroys its argument, so use with care.")
#define FUNC_NAME s_scm_nconc2last
{
  SCM *lloc;
  SCM_VALIDATE_NONEMPTYLIST (1, lst);
  lloc = &lst;
  while (!scm_is_null (SCM_CDR (*lloc))) /* Perhaps should be
                                          SCM_NULL_OR_NIL_P, but not
                                          needed in 99.99% of cases,
                                          and it could seriously hurt
                                          performance. - Neil */
    lloc = SCM_CDRLOC (*lloc);
  SCM_ASSERT (scm_ilength (SCM_CAR (*lloc)) >= 0, lst, SCM_ARG1, FUNC_NAME);
  *lloc = SCM_CAR (*lloc);
  return lst;
}
#undef FUNC_NAME



/* Typechecking for multi-argument MAP and FOR-EACH.

   Verify that each element of the vector ARGV, except for the first,
   is a proper list whose length is LEN.  Attribute errors to WHO,
   and claim that the i'th element of ARGV is WHO's i+2'th argument.  */
static inline void
check_map_args (SCM argv,
		long len,
		SCM gf,
		SCM proc,
		SCM args,
		const char *who)
{
  long i;

  for (i = SCM_SIMPLE_VECTOR_LENGTH (argv) - 1; i >= 1; i--)
    {
      SCM elt = SCM_SIMPLE_VECTOR_REF (argv, i);
      long elt_len = scm_ilength (elt);

      if (elt_len < 0)
	{
	  if (gf)
	    scm_apply_generic (gf, scm_cons (proc, args));
	  else
	    scm_wrong_type_arg (who, i + 2, elt);
	}

      if (elt_len != len)
	scm_out_of_range_pos (who, elt, scm_from_long (i + 2));
    }
}


SCM_GPROC (s_map, "map", 2, 0, 1, scm_map, g_map);

/* Note: Currently, scm_map applies PROC to the argument list(s)
   sequentially, starting with the first element(s).  This is used in
   evalext.c where the Scheme procedure `map-in-order', which guarantees
   sequential behaviour, is implemented using scm_map.  If the
   behaviour changes, we need to update `map-in-order'.
*/

SCM 
scm_map (SCM proc, SCM arg1, SCM args)
#define FUNC_NAME s_map
{
  long i, len;
  SCM res = SCM_EOL;
  SCM *pres = &res;

  len = scm_ilength (arg1);
  SCM_GASSERTn (len >= 0,
		g_map, scm_cons2 (proc, arg1, args), SCM_ARG2, s_map);
  SCM_VALIDATE_REST_ARGUMENT (args);
  if (scm_is_null (args))
    {
      SCM_GASSERT2 (scm_is_true (scm_procedure_p (proc)), g_map, proc, arg1, SCM_ARG1, s_map);
      while (SCM_NIMP (arg1))
	{
	  *pres = scm_list_1 (scm_call_1 (proc, SCM_CAR (arg1)));
	  pres = SCM_CDRLOC (*pres);
	  arg1 = SCM_CDR (arg1);
	}
      return res;
    }
  if (scm_is_null (SCM_CDR (args)))
    {
      SCM arg2 = SCM_CAR (args);
      int len2 = scm_ilength (arg2);
      SCM_GASSERTn (scm_is_true (scm_procedure_p (proc)), g_map,
                    scm_cons2 (proc, arg1, args), SCM_ARG1, s_map);
      SCM_GASSERTn (len2 >= 0,
		    g_map, scm_cons2 (proc, arg1, args), SCM_ARG3, s_map);
      if (len2 != len)
	SCM_OUT_OF_RANGE (3, arg2);
      while (SCM_NIMP (arg1))
	{
	  *pres = scm_list_1 (scm_call_2 (proc, SCM_CAR (arg1), SCM_CAR (arg2)));
	  pres = SCM_CDRLOC (*pres);
	  arg1 = SCM_CDR (arg1);
	  arg2 = SCM_CDR (arg2);
	}
      return res;
    }
  arg1 = scm_cons (arg1, args);
  args = scm_vector (arg1);
  check_map_args (args, len, g_map, proc, arg1, s_map);
  while (1)
    {
      arg1 = SCM_EOL;
      for (i = SCM_SIMPLE_VECTOR_LENGTH (args) - 1; i >= 0; i--)
	{
	  SCM elt = SCM_SIMPLE_VECTOR_REF (args, i);
	  if (SCM_IMP (elt)) 
	    return res;
	  arg1 = scm_cons (SCM_CAR (elt), arg1);
	  SCM_SIMPLE_VECTOR_SET (args, i, SCM_CDR (elt));
	}
      *pres = scm_list_1 (scm_apply (proc, arg1, SCM_EOL));
      pres = SCM_CDRLOC (*pres);
    }
}
#undef FUNC_NAME


SCM_GPROC (s_for_each, "for-each", 2, 0, 1, scm_for_each, g_for_each);

SCM 
scm_for_each (SCM proc, SCM arg1, SCM args)
#define FUNC_NAME s_for_each
{
  long i, len;
  len = scm_ilength (arg1);
  SCM_GASSERTn (len >= 0, g_for_each, scm_cons2 (proc, arg1, args),
		SCM_ARG2, s_for_each);
  SCM_VALIDATE_REST_ARGUMENT (args);
  if (scm_is_null (args))
    {
      SCM_GASSERT2 (scm_is_true (scm_procedure_p (proc)), g_for_each,
                    proc, arg1, SCM_ARG1, s_for_each);
      while (SCM_NIMP (arg1))
	{
	  scm_call_1 (proc, SCM_CAR (arg1));
	  arg1 = SCM_CDR (arg1);
	}
      return SCM_UNSPECIFIED;
    }
  if (scm_is_null (SCM_CDR (args)))
    {
      SCM arg2 = SCM_CAR (args);
      int len2 = scm_ilength (arg2);
      SCM_GASSERTn (scm_is_true (scm_procedure_p (proc)), g_for_each,
		    scm_cons2 (proc, arg1, args), SCM_ARG1, s_for_each);
      SCM_GASSERTn (len2 >= 0, g_for_each,
		    scm_cons2 (proc, arg1, args), SCM_ARG3, s_for_each);
      if (len2 != len)
	SCM_OUT_OF_RANGE (3, arg2);
      while (SCM_NIMP (arg1))
	{
	  scm_call_2 (proc, SCM_CAR (arg1), SCM_CAR (arg2));
	  arg1 = SCM_CDR (arg1);
	  arg2 = SCM_CDR (arg2);
	}
      return SCM_UNSPECIFIED;
    }
  arg1 = scm_cons (arg1, args);
  args = scm_vector (arg1);
  check_map_args (args, len, g_for_each, proc, arg1, s_for_each);
  while (1)
    {
      arg1 = SCM_EOL;
      for (i = SCM_SIMPLE_VECTOR_LENGTH (args) - 1; i >= 0; i--)
	{
	  SCM elt = SCM_SIMPLE_VECTOR_REF (args, i);
	  if (SCM_IMP (elt))
	    return SCM_UNSPECIFIED;
	  arg1 = scm_cons (SCM_CAR (elt), arg1);
	  SCM_SIMPLE_VECTOR_SET (args, i, SCM_CDR (elt));
	}
      scm_apply (proc, arg1, SCM_EOL);
    }
}
#undef FUNC_NAME


static SCM
scm_c_primitive_eval (SCM exp)
{
  SCM transformer = scm_current_module_transformer ();
  if (scm_is_true (transformer))
    exp = scm_call_1 (transformer, exp);
  exp = scm_memoize_expression (exp);
  return eval (exp, SCM_EOL);
}

static SCM var_primitive_eval;
SCM
scm_primitive_eval (SCM exp)
{
  return scm_c_vm_run (scm_the_vm (), scm_variable_ref (var_primitive_eval),
                       &exp, 1);
}


/* Eval does not take the second arg optionally.  This is intentional
 * in order to be R5RS compatible, and to prepare for the new module
 * system, where we would like to make the choice of evaluation
 * environment explicit.  */

SCM_DEFINE (scm_eval, "eval", 2, 0, 0, 
	    (SCM exp, SCM module_or_state),
	    "Evaluate @var{exp}, a list representing a Scheme expression,\n"
            "in the top-level environment specified by\n"
	    "@var{module_or_state}.\n"
            "While @var{exp} is evaluated (using @code{primitive-eval}),\n"
            "@var{module_or_state} is made the current module when\n"
	    "it is a module, or the current dynamic state when it is\n"
	    "a dynamic state."
	    "Example: (eval '(+ 1 2) (interaction-environment))")
#define FUNC_NAME s_scm_eval
{
  SCM res;

  scm_dynwind_begin (SCM_F_DYNWIND_REWINDABLE);
  if (scm_is_dynamic_state (module_or_state))
    scm_dynwind_current_dynamic_state (module_or_state);
  else if (scm_module_system_booted_p)
    {
      SCM_VALIDATE_MODULE (2, module_or_state);
      scm_dynwind_current_module (module_or_state);
    }
  /* otherwise if the module system isn't booted, ignore the module arg */

  res = scm_primitive_eval (exp);

  scm_dynwind_end ();
  return res;
}
#undef FUNC_NAME


static SCM f_apply;

/* Apply a function to a list of arguments.

   This function is exported to the Scheme level as taking two
   required arguments and a tail argument, as if it were:
	(lambda (proc arg1 . args) ...)
   Thus, if you just have a list of arguments to pass to a procedure,
   pass the list as ARG1, and '() for ARGS.  If you have some fixed
   args, pass the first as ARG1, then cons any remaining fixed args
   onto the front of your argument list, and pass that as ARGS.  */

SCM 
scm_apply (SCM proc, SCM arg1, SCM args)
{
  /* Fix things up so that args contains all args. */
  if (scm_is_null (args))
    args = arg1;
  else
    args = scm_cons_star (arg1, args);

  return scm_vm_apply (scm_the_vm (), proc, args);
}


static SCM
boot_closure_apply (SCM closure, SCM args)
{
  int nreq = BOOT_CLOSURE_NUM_REQUIRED_ARGS (closure);
  SCM new_env = BOOT_CLOSURE_ENV (closure);
  if (BOOT_CLOSURE_HAS_REST_ARGS (closure))
    {
      if (SCM_UNLIKELY (scm_ilength (args) < nreq))
        scm_wrong_num_args (closure);
      for (; nreq; nreq--, args = CDR (args))
        new_env = scm_cons (CAR (args), new_env);
      new_env = scm_cons (args, new_env);
    }
  else
    {
      if (SCM_UNLIKELY (scm_ilength (args) != nreq))
        scm_wrong_num_args (closure);
      for (; scm_is_pair (args); args = CDR (args))
        new_env = scm_cons (CAR (args), new_env);
    }
  return eval (BOOT_CLOSURE_BODY (closure), new_env);
}

static int
boot_closure_print (SCM closure, SCM port, scm_print_state *pstate)
{
  SCM args;
  scm_puts ("#<boot-closure ", port);
  scm_uintprint ((unsigned long)SCM2PTR (closure), 16, port);
  scm_putc (' ', port);
  args = scm_make_list (scm_from_int (BOOT_CLOSURE_NUM_REQUIRED_ARGS (closure)),
                        scm_from_locale_symbol ("_"));
  if (BOOT_CLOSURE_HAS_REST_ARGS (closure))
    args = scm_cons_star (scm_from_locale_symbol ("_"), args);
  scm_display (args, port);
  scm_putc ('>', port);
  return 1;
}

void 
scm_init_eval ()
{
  SCM primitive_eval;

  scm_init_opts (scm_evaluator_traps,
		 scm_evaluator_trap_table);
  scm_init_opts (scm_eval_options_interface,
		 scm_eval_opts);
  
  f_apply = scm_c_define_gsubr ("apply", 2, 0, 1, scm_apply);

  scm_tc16_boot_closure = scm_make_smob_type ("boot-closure", 0);
  scm_set_smob_apply (scm_tc16_boot_closure, boot_closure_apply, 0, 0, 1);
  scm_set_smob_print (scm_tc16_boot_closure, boot_closure_print);

  primitive_eval = scm_c_make_gsubr ("primitive-eval", 1, 0, 0,
                                     scm_c_primitive_eval);
  var_primitive_eval = scm_define (SCM_SUBR_NAME (primitive_eval),
                                   primitive_eval);

#include "libguile/eval.x"
}

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/

