/* Expands front end tree to back end RTL for GCC
   Copyright (C) 1987, 1988, 1989, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* This file handles the generation of rtl code from tree structure
   above the level of expressions, using subroutines in exp*.c and emit-rtl.c.
   It also creates the rtl expressions for parameters and auto variables
   and has full responsibility for allocating stack slots.

   The functions whose names start with `expand_' are called by the
   parser to generate RTL instructions for various kinds of constructs.

   Some control and binding constructs require calling several such
   functions at different times.  For example, a simple if-then
   is expanded by calling `expand_start_cond' (with the condition-expression
   as argument) before parsing the then-clause and calling `expand_end_cond'
   after parsing the then-clause.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"

#include "rtl.h"
#include "tree.h"
#include "tm_p.h"
#include "flags.h"
#include "except.h"
#include "function.h"
#include "insn-config.h"
#include "expr.h"
#include "libfuncs.h"
#include "hard-reg-set.h"
#include "loop.h"
#include "recog.h"
#include "machmode.h"
#include "toplev.h"
#include "output.h"
#include "ggc.h"
#include "langhooks.h"
#include "predict.h"
#include "optabs.h"
#include "target.h"
#include "regs.h"

/* Functions and data structures for expanding case statements.  */

/* Case label structure, used to hold info on labels within case
   statements.  We handle "range" labels; for a single-value label
   as in C, the high and low limits are the same.

   We start with a vector of case nodes sorted in ascending order, and
   the default label as the last element in the vector.  Before expanding
   to RTL, we transform this vector into a list linked via the RIGHT
   fields in the case_node struct.  Nodes with higher case values are
   later in the list.

   Switch statements can be output in three forms.  A branch table is
   used if there are more than a few labels and the labels are dense
   within the range between the smallest and largest case value.  If a
   branch table is used, no further manipulations are done with the case
   node chain.

   The alternative to the use of a branch table is to generate a series
   of compare and jump insns.  When that is done, we use the LEFT, RIGHT,
   and PARENT fields to hold a binary tree.  Initially the tree is
   totally unbalanced, with everything on the right.  We balance the tree
   with nodes on the left having lower case values than the parent
   and nodes on the right having higher values.  We then output the tree
   in order.

   For very small, suitable switch statements, we can generate a series
   of simple bit test and branches instead.  */

struct case_node GTY(())
{
  struct case_node	*left;	/* Left son in binary tree */
  struct case_node	*right;	/* Right son in binary tree; also node chain */
  struct case_node	*parent; /* Parent of node in binary tree */
  tree			low;	/* Lowest index value for this label */
  tree			high;	/* Highest index value for this label */
  tree			code_label; /* Label to jump to when node matches */
};

typedef struct case_node case_node;
typedef struct case_node *case_node_ptr;

/* These are used by estimate_case_costs and balance_case_nodes.  */

/* This must be a signed type, and non-ANSI compilers lack signed char.  */
static short cost_table_[129];
static int use_cost_table;
static int cost_table_initialized;

/* Special care is needed because we allow -1, but TREE_INT_CST_LOW
   is unsigned.  */
#define COST_TABLE(I)  cost_table_[(unsigned HOST_WIDE_INT) ((I) + 1)]

/* Stack of control and binding constructs we are currently inside.

   These constructs begin when you call `expand_start_WHATEVER'
   and end when you call `expand_end_WHATEVER'.  This stack records
   info about how the construct began that tells the end-function
   what to do.  It also may provide information about the construct
   to alter the behavior of other constructs within the body.
   For example, they may affect the behavior of C `break' and `continue'.

   Each construct gets one `struct nesting' object.
   All of these objects are chained through the `all' field.
   `nesting_stack' points to the first object (innermost construct).
   The position of an entry on `nesting_stack' is in its `depth' field.

   Each type of construct has its own individual stack.
   For example, loops have `cond_stack'.  Each object points to the
   next object of the same type through the `next' field.

   Some constructs are visible to `break' exit-statements and others
   are not.  Which constructs are visible depends on the language.
   Therefore, the data structure allows each construct to be visible
   or not, according to the args given when the construct is started.
   The construct is visible if the `exit_label' field is non-null.
   In that case, the value should be a CODE_LABEL rtx.  */

struct nesting GTY(())
{
  struct nesting *all;
  struct nesting *next;
  int depth;
  rtx exit_label;
  enum nesting_desc {
    COND_NESTING,
    BLOCK_NESTING,
    CASE_NESTING
  } desc;
  union nesting_u
    {
      /* For conds (if-then and if-then-else statements).  */
      struct nesting_cond
	{
	  /* Label for the end of the if construct.
	     There is none if EXITFLAG was not set
	     and no `else' has been seen yet.  */
	  rtx endif_label;
	  /* Label for the end of this alternative.
	     This may be the end of the if or the next else/elseif.  */
	  rtx next_label;
	} GTY ((tag ("COND_NESTING"))) cond;
      /* For switch (C) or case (Pascal) statements.  */
      struct nesting_case
	{
	  /* The insn after which the case dispatch should finally
	     be emitted.  Zero for a dummy.  */
	  rtx start;
	  /* A list of case labels; it is first built as an AVL tree.
	     During expand_end_case, this is converted to a list, and may be
	     rearranged into a nearly balanced binary tree.  */
	  struct case_node *case_list;
	  /* Label to jump to if no case matches.  */
	  tree default_label;
	  /* The expression to be dispatched on.  */
	  tree index_expr;
	} GTY ((tag ("CASE_NESTING"))) case_stmt;
    } GTY ((desc ("%1.desc"))) data;
};

/* Allocate and return a new `struct nesting'.  */

#define ALLOC_NESTING() ggc_alloc (sizeof (struct nesting))

/* Pop the nesting stack element by element until we pop off
   the element which is at the top of STACK.
   Update all the other stacks, popping off elements from them
   as we pop them from nesting_stack.  */

#define POPSTACK(STACK)					\
do { struct nesting *target = STACK;			\
     struct nesting *this;				\
     do { this = nesting_stack;				\
	  if (cond_stack == this)			\
	    cond_stack = cond_stack->next;		\
	  if (case_stack == this)			\
	    case_stack = case_stack->next;		\
	  nesting_depth = nesting_stack->depth - 1;	\
	  nesting_stack = this->all; }			\
     while (this != target); } while (0)


struct stmt_status GTY(())
{
  /* If any new stacks are added here, add them to POPSTACKS too.  */

  /* Chain of all pending conditional statements.  */
  struct nesting * x_cond_stack;

  /* Chain of all pending case or switch statements.  */
  struct nesting * x_case_stack;

  /* Separate chain including all of the above,
     chained through the `all' field.  */
  struct nesting * x_nesting_stack;

  /* Number of entries on nesting_stack now.  */
  int x_nesting_depth;

  /* Location of last line-number note, whether we actually
     emitted it or not.  */
  location_t x_emit_locus;
};

#define cond_stack (cfun->stmt->x_cond_stack)
#define case_stack (cfun->stmt->x_case_stack)
#define nesting_stack (cfun->stmt->x_nesting_stack)
#define nesting_depth (cfun->stmt->x_nesting_depth)
#define emit_locus (cfun->stmt->x_emit_locus)

static int n_occurrences (int, const char *);
static bool decl_conflicts_with_clobbers_p (tree, const HARD_REG_SET);
static void expand_nl_goto_receiver (void);
static bool check_operand_nalternatives (tree, tree);
static bool check_unique_operand_names (tree, tree);
static char *resolve_operand_name_1 (char *, tree, tree);
static void expand_null_return_1 (void);
static rtx shift_return_value (rtx);
static void expand_value_return (rtx);
static void do_jump_if_equal (rtx, rtx, rtx, int);
static int estimate_case_costs (case_node_ptr);
static bool same_case_target_p (rtx, rtx);
static bool lshift_cheap_p (void);
static int case_bit_test_cmp (const void *, const void *);
static void emit_case_bit_tests (tree, tree, tree, tree, case_node_ptr, rtx);
static void balance_case_nodes (case_node_ptr *, case_node_ptr);
static int node_has_low_bound (case_node_ptr, tree);
static int node_has_high_bound (case_node_ptr, tree);
static int node_is_bounded (case_node_ptr, tree);
static void emit_case_nodes (rtx, case_node_ptr, rtx, tree);

void
init_stmt_for_function (void)
{
  cfun->stmt = ggc_alloc_cleared (sizeof (struct stmt_status));
}

/* Record the current file and line.  Called from emit_line_note.  */

void
set_file_and_line_for_stmt (location_t location)
{
  /* If we're outputting an inline function, and we add a line note,
     there may be no CFUN->STMT information.  So, there's no need to
     update it.  */
  if (cfun->stmt)
    emit_locus = location;
}

/* Emit a no-op instruction.  */

void
emit_nop (void)
{
  rtx last_insn;

  last_insn = get_last_insn ();
  if (!optimize
      && (LABEL_P (last_insn)
	  || (NOTE_P (last_insn)
	      && prev_real_insn (last_insn) == 0)))
    emit_insn (gen_nop ());
}

/* Return the rtx-label that corresponds to a LABEL_DECL,
   creating it if necessary.  */

rtx
label_rtx (tree label)
{
  if (TREE_CODE (label) != LABEL_DECL)
    abort ();

  if (!DECL_RTL_SET_P (label))
    {
      rtx r = gen_label_rtx ();
      SET_DECL_RTL (label, r);
      if (FORCED_LABEL (label) || DECL_NONLOCAL (label))
	LABEL_PRESERVE_P (r) = 1;
    }

  return DECL_RTL (label);
}

/* As above, but also put it on the forced-reference list of the
   function that contains it.  */
rtx
force_label_rtx (tree label)
{
  rtx ref = label_rtx (label);
  tree function = decl_function_context (label);
  struct function *p;

  if (!function)
    abort ();

  if (function != current_function_decl)
    p = find_function_data (function);
  else
    p = cfun;

  p->expr->x_forced_labels = gen_rtx_EXPR_LIST (VOIDmode, ref,
						p->expr->x_forced_labels);
  return ref;
}

/* Add an unconditional jump to LABEL as the next sequential instruction.  */

void
emit_jump (rtx label)
{
  do_pending_stack_adjust ();
  emit_jump_insn (gen_jump (label));
  emit_barrier ();
}

/* Emit code to jump to the address
   specified by the pointer expression EXP.  */

void
expand_computed_goto (tree exp)
{
  rtx x = expand_expr (exp, NULL_RTX, VOIDmode, 0);

  x = convert_memory_address (Pmode, x);

  do_pending_stack_adjust ();
  emit_indirect_jump (x);
}

/* Handle goto statements and the labels that they can go to.  */

/* Specify the location in the RTL code of a label LABEL,
   which is a LABEL_DECL tree node.

   This is used for the kind of label that the user can jump to with a
   goto statement, and for alternatives of a switch or case statement.
   RTL labels generated for loops and conditionals don't go through here;
   they are generated directly at the RTL level, by other functions below.

   Note that this has nothing to do with defining label *names*.
   Languages vary in how they do that and what that even means.  */

void
expand_label (tree label)
{
  rtx label_r = label_rtx (label);

  do_pending_stack_adjust ();
  emit_label (label_r);
  if (DECL_NAME (label))
    LABEL_NAME (DECL_RTL (label)) = IDENTIFIER_POINTER (DECL_NAME (label));

  if (DECL_NONLOCAL (label))
    {
      expand_nl_goto_receiver ();
      nonlocal_goto_handler_labels
	= gen_rtx_EXPR_LIST (VOIDmode, label_r,
			     nonlocal_goto_handler_labels);
    }

  if (FORCED_LABEL (label))
    forced_labels = gen_rtx_EXPR_LIST (VOIDmode, label_r, forced_labels);

  if (DECL_NONLOCAL (label) || FORCED_LABEL (label))
    maybe_set_first_label_num (label_r);
}

/* Generate RTL code for a `goto' statement with target label LABEL.
   LABEL should be a LABEL_DECL tree node that was or will later be
   defined with `expand_label'.  */

void
expand_goto (tree label)
{
#ifdef ENABLE_CHECKING
  /* Check for a nonlocal goto to a containing function.  Should have
     gotten translated to __builtin_nonlocal_goto.  */
  tree context = decl_function_context (label);
  if (context != 0 && context != current_function_decl)
    abort ();
#endif

  emit_jump (label_rtx (label));
}

/* Return the number of times character C occurs in string S.  */
static int
n_occurrences (int c, const char *s)
{
  int n = 0;
  while (*s)
    n += (*s++ == c);
  return n;
}

/* Generate RTL for an asm statement (explicit assembler code).
   STRING is a STRING_CST node containing the assembler code text,
   or an ADDR_EXPR containing a STRING_CST.  VOL nonzero means the
   insn is volatile; don't optimize it.  */

void
expand_asm (tree string, int vol)
{
  rtx body;

  if (TREE_CODE (string) == ADDR_EXPR)
    string = TREE_OPERAND (string, 0);

  body = gen_rtx_ASM_INPUT (VOIDmode, TREE_STRING_POINTER (string));

  MEM_VOLATILE_P (body) = vol;

  emit_insn (body);
}

/* Parse the output constraint pointed to by *CONSTRAINT_P.  It is the
   OPERAND_NUMth output operand, indexed from zero.  There are NINPUTS
   inputs and NOUTPUTS outputs to this extended-asm.  Upon return,
   *ALLOWS_MEM will be TRUE iff the constraint allows the use of a
   memory operand.  Similarly, *ALLOWS_REG will be TRUE iff the
   constraint allows the use of a register operand.  And, *IS_INOUT
   will be true if the operand is read-write, i.e., if it is used as
   an input as well as an output.  If *CONSTRAINT_P is not in
   canonical form, it will be made canonical.  (Note that `+' will be
   replaced with `=' as part of this process.)

   Returns TRUE if all went well; FALSE if an error occurred.  */

bool
parse_output_constraint (const char **constraint_p, int operand_num,
			 int ninputs, int noutputs, bool *allows_mem,
			 bool *allows_reg, bool *is_inout)
{
  const char *constraint = *constraint_p;
  const char *p;

  /* Assume the constraint doesn't allow the use of either a register
     or memory.  */
  *allows_mem = false;
  *allows_reg = false;

  /* Allow the `=' or `+' to not be at the beginning of the string,
     since it wasn't explicitly documented that way, and there is a
     large body of code that puts it last.  Swap the character to
     the front, so as not to uglify any place else.  */
  p = strchr (constraint, '=');
  if (!p)
    p = strchr (constraint, '+');

  /* If the string doesn't contain an `=', issue an error
     message.  */
  if (!p)
    {
      error ("output operand constraint lacks `='");
      return false;
    }

  /* If the constraint begins with `+', then the operand is both read
     from and written to.  */
  *is_inout = (*p == '+');

  /* Canonicalize the output constraint so that it begins with `='.  */
  if (p != constraint || is_inout)
    {
      char *buf;
      size_t c_len = strlen (constraint);

      if (p != constraint)
	warning ("output constraint `%c' for operand %d is not at the beginning",
		 *p, operand_num);

      /* Make a copy of the constraint.  */
      buf = alloca (c_len + 1);
      strcpy (buf, constraint);
      /* Swap the first character and the `=' or `+'.  */
      buf[p - constraint] = buf[0];
      /* Make sure the first character is an `='.  (Until we do this,
	 it might be a `+'.)  */
      buf[0] = '=';
      /* Replace the constraint with the canonicalized string.  */
      *constraint_p = ggc_alloc_string (buf, c_len);
      constraint = *constraint_p;
    }

  /* Loop through the constraint string.  */
  for (p = constraint + 1; *p; p += CONSTRAINT_LEN (*p, p))
    switch (*p)
      {
      case '+':
      case '=':
	error ("operand constraint contains incorrectly positioned '+' or '='");
	return false;

      case '%':
	if (operand_num + 1 == ninputs + noutputs)
	  {
	    error ("`%%' constraint used with last operand");
	    return false;
	  }
	break;

      case 'V':  case 'm':  case 'o':
	*allows_mem = true;
	break;

      case '?':  case '!':  case '*':  case '&':  case '#':
      case 'E':  case 'F':  case 'G':  case 'H':
      case 's':  case 'i':  case 'n':
      case 'I':  case 'J':  case 'K':  case 'L':  case 'M':
      case 'N':  case 'O':  case 'P':  case ',':
	break;

      case '0':  case '1':  case '2':  case '3':  case '4':
      case '5':  case '6':  case '7':  case '8':  case '9':
      case '[':
	error ("matching constraint not valid in output operand");
	return false;

      case '<':  case '>':
	/* ??? Before flow, auto inc/dec insns are not supposed to exist,
	   excepting those that expand_call created.  So match memory
	   and hope.  */
	*allows_mem = true;
	break;

      case 'g':  case 'X':
	*allows_reg = true;
	*allows_mem = true;
	break;

      case 'p': case 'r':
	*allows_reg = true;
	break;

      default:
	if (!ISALPHA (*p))
	  break;
	if (REG_CLASS_FROM_CONSTRAINT (*p, p) != NO_REGS)
	  *allows_reg = true;
#ifdef EXTRA_CONSTRAINT_STR
	else if (EXTRA_ADDRESS_CONSTRAINT (*p, p))
	  *allows_reg = true;
	else if (EXTRA_MEMORY_CONSTRAINT (*p, p))
	  *allows_mem = true;
	else
	  {
	    /* Otherwise we can't assume anything about the nature of
	       the constraint except that it isn't purely registers.
	       Treat it like "g" and hope for the best.  */
	    *allows_reg = true;
	    *allows_mem = true;
	  }
#endif
	break;
      }

  return true;
}

/* Similar, but for input constraints.  */

bool
parse_input_constraint (const char **constraint_p, int input_num,
			int ninputs, int noutputs, int ninout,
			const char * const * constraints,
			bool *allows_mem, bool *allows_reg)
{
  const char *constraint = *constraint_p;
  const char *orig_constraint = constraint;
  size_t c_len = strlen (constraint);
  size_t j;
  bool saw_match = false;

  /* Assume the constraint doesn't allow the use of either
     a register or memory.  */
  *allows_mem = false;
  *allows_reg = false;

  /* Make sure constraint has neither `=', `+', nor '&'.  */

  for (j = 0; j < c_len; j += CONSTRAINT_LEN (constraint[j], constraint+j))
    switch (constraint[j])
      {
      case '+':  case '=':  case '&':
	if (constraint == orig_constraint)
	  {
	    error ("input operand constraint contains `%c'", constraint[j]);
	    return false;
	  }
	break;

      case '%':
	if (constraint == orig_constraint
	    && input_num + 1 == ninputs - ninout)
	  {
	    error ("`%%' constraint used with last operand");
	    return false;
	  }
	break;

      case 'V':  case 'm':  case 'o':
	*allows_mem = true;
	break;

      case '<':  case '>':
      case '?':  case '!':  case '*':  case '#':
      case 'E':  case 'F':  case 'G':  case 'H':
      case 's':  case 'i':  case 'n':
      case 'I':  case 'J':  case 'K':  case 'L':  case 'M':
      case 'N':  case 'O':  case 'P':  case ',':
	break;

	/* Whether or not a numeric constraint allows a register is
	   decided by the matching constraint, and so there is no need
	   to do anything special with them.  We must handle them in
	   the default case, so that we don't unnecessarily force
	   operands to memory.  */
      case '0':  case '1':  case '2':  case '3':  case '4':
      case '5':  case '6':  case '7':  case '8':  case '9':
	{
	  char *end;
	  unsigned long match;

	  saw_match = true;

	  match = strtoul (constraint + j, &end, 10);
	  if (match >= (unsigned long) noutputs)
	    {
	      error ("matching constraint references invalid operand number");
	      return false;
	    }

	  /* Try and find the real constraint for this dup.  Only do this
	     if the matching constraint is the only alternative.  */
	  if (*end == '\0'
	      && (j == 0 || (j == 1 && constraint[0] == '%')))
	    {
	      constraint = constraints[match];
	      *constraint_p = constraint;
	      c_len = strlen (constraint);
	      j = 0;
	      /* ??? At the end of the loop, we will skip the first part of
		 the matched constraint.  This assumes not only that the
		 other constraint is an output constraint, but also that
		 the '=' or '+' come first.  */
	      break;
	    }
	  else
	    j = end - constraint;
	  /* Anticipate increment at end of loop.  */
	  j--;
	}
	/* Fall through.  */

      case 'p':  case 'r':
	*allows_reg = true;
	break;

      case 'g':  case 'X':
	*allows_reg = true;
	*allows_mem = true;
	break;

      default:
	if (! ISALPHA (constraint[j]))
	  {
	    error ("invalid punctuation `%c' in constraint", constraint[j]);
	    return false;
	  }
	if (REG_CLASS_FROM_CONSTRAINT (constraint[j], constraint + j)
	    != NO_REGS)
	  *allows_reg = true;
#ifdef EXTRA_CONSTRAINT_STR
	else if (EXTRA_ADDRESS_CONSTRAINT (constraint[j], constraint + j))
	  *allows_reg = true;
	else if (EXTRA_MEMORY_CONSTRAINT (constraint[j], constraint + j))
	  *allows_mem = true;
	else
	  {
	    /* Otherwise we can't assume anything about the nature of
	       the constraint except that it isn't purely registers.
	       Treat it like "g" and hope for the best.  */
	    *allows_reg = true;
	    *allows_mem = true;
	  }
#endif
	break;
      }

  if (saw_match && !*allows_reg)
    warning ("matching constraint does not allow a register");

  return true;
}

/* INPUT is one of the input operands from EXPR, an ASM_EXPR.  Returns true
   if it is an operand which must be passed in memory (i.e. an "m"
   constraint), false otherwise.  */

bool
asm_op_is_mem_input (tree input, tree expr)
{
  const char *constraint = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (input)));
  tree outputs = ASM_OUTPUTS (expr);
  int noutputs = list_length (outputs);
  const char **constraints
    = (const char **) alloca ((noutputs) * sizeof (const char *));
  int i = 0;
  bool allows_mem, allows_reg;
  tree t;

  /* Collect output constraints.  */
  for (t = outputs; t ; t = TREE_CHAIN (t), i++)
    constraints[i] = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));

  /* We pass 0 for input_num, ninputs and ninout; they are only used for
     error checking which will be done at expand time.  */
  parse_input_constraint (&constraint, 0, 0, noutputs, 0, constraints,
			  &allows_mem, &allows_reg);
  return (!allows_reg && allows_mem);
}

/* Check for overlap between registers marked in CLOBBERED_REGS and
   anything inappropriate in DECL.  Emit error and return TRUE for error,
   FALSE for ok.  */

static bool
decl_conflicts_with_clobbers_p (tree decl, const HARD_REG_SET clobbered_regs)
{
  /* Conflicts between asm-declared register variables and the clobber
     list are not allowed.  */
  if ((TREE_CODE (decl) == VAR_DECL || TREE_CODE (decl) == PARM_DECL)
      && DECL_REGISTER (decl)
      && REG_P (DECL_RTL (decl))
      && REGNO (DECL_RTL (decl)) < FIRST_PSEUDO_REGISTER)
    {
      rtx reg = DECL_RTL (decl);
      unsigned int regno;

      for (regno = REGNO (reg);
	   regno < (REGNO (reg)
		    + hard_regno_nregs[REGNO (reg)][GET_MODE (reg)]);
	   regno++)
	if (TEST_HARD_REG_BIT (clobbered_regs, regno))
	  {
	    error ("asm-specifier for variable `%s' conflicts with asm clobber list",
		   IDENTIFIER_POINTER (DECL_NAME (decl)));

	    /* Reset registerness to stop multiple errors emitted for a
	       single variable.  */
	    DECL_REGISTER (decl) = 0;
	    return true;
	  }
    }
  return false;
}

/* Generate RTL for an asm statement with arguments.
   STRING is the instruction template.
   OUTPUTS is a list of output arguments (lvalues); INPUTS a list of inputs.
   Each output or input has an expression in the TREE_VALUE and
   and a tree list in TREE_PURPOSE which in turn contains a constraint
   name in TREE_VALUE (or NULL_TREE) and a constraint string
   in TREE_PURPOSE.
   CLOBBERS is a list of STRING_CST nodes each naming a hard register
   that is clobbered by this insn.

   Not all kinds of lvalue that may appear in OUTPUTS can be stored directly.
   Some elements of OUTPUTS may be replaced with trees representing temporary
   values.  The caller should copy those temporary values to the originally
   specified lvalues.

   VOL nonzero means the insn is volatile; don't optimize it.  */

void
expand_asm_operands (tree string, tree outputs, tree inputs,
		     tree clobbers, int vol, location_t locus)
{
  rtvec argvec, constraintvec;
  rtx body;
  int ninputs = list_length (inputs);
  int noutputs = list_length (outputs);
  int ninout;
  int nclobbers;
  HARD_REG_SET clobbered_regs;
  int clobber_conflict_found = 0;
  tree tail;
  tree t;
  int i;
  /* Vector of RTX's of evaluated output operands.  */
  rtx *output_rtx = alloca (noutputs * sizeof (rtx));
  int *inout_opnum = alloca (noutputs * sizeof (int));
  rtx *real_output_rtx = alloca (noutputs * sizeof (rtx));
  enum machine_mode *inout_mode
    = alloca (noutputs * sizeof (enum machine_mode));
  const char **constraints
    = alloca ((noutputs + ninputs) * sizeof (const char *));
  int old_generating_concat_p = generating_concat_p;

  /* An ASM with no outputs needs to be treated as volatile, for now.  */
  if (noutputs == 0)
    vol = 1;

  if (! check_operand_nalternatives (outputs, inputs))
    return;

  string = resolve_asm_operand_names (string, outputs, inputs);

  /* Collect constraints.  */
  i = 0;
  for (t = outputs; t ; t = TREE_CHAIN (t), i++)
    constraints[i] = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));
  for (t = inputs; t ; t = TREE_CHAIN (t), i++)
    constraints[i] = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));

  /* Sometimes we wish to automatically clobber registers across an asm.
     Case in point is when the i386 backend moved from cc0 to a hard reg --
     maintaining source-level compatibility means automatically clobbering
     the flags register.  */
  clobbers = targetm.md_asm_clobbers (clobbers);

  /* Count the number of meaningful clobbered registers, ignoring what
     we would ignore later.  */
  nclobbers = 0;
  CLEAR_HARD_REG_SET (clobbered_regs);
  for (tail = clobbers; tail; tail = TREE_CHAIN (tail))
    {
      const char *regname = TREE_STRING_POINTER (TREE_VALUE (tail));

      i = decode_reg_name (regname);
      if (i >= 0 || i == -4)
	++nclobbers;
      else if (i == -2)
	error ("unknown register name `%s' in `asm'", regname);

      /* Mark clobbered registers.  */
      if (i >= 0)
        {
	  /* Clobbering the PIC register is an error */
	  if (i == (int) PIC_OFFSET_TABLE_REGNUM)
	    {
	      error ("PIC register `%s' clobbered in `asm'", regname);
	      return;
	    }

	  SET_HARD_REG_BIT (clobbered_regs, i);
	}
    }

  /* First pass over inputs and outputs checks validity and sets
     mark_addressable if needed.  */

  ninout = 0;
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    {
      tree val = TREE_VALUE (tail);
      tree type = TREE_TYPE (val);
      const char *constraint;
      bool is_inout;
      bool allows_reg;
      bool allows_mem;

      /* If there's an erroneous arg, emit no insn.  */
      if (type == error_mark_node)
	return;

      /* Try to parse the output constraint.  If that fails, there's
	 no point in going further.  */
      constraint = constraints[i];
      if (!parse_output_constraint (&constraint, i, ninputs, noutputs,
				    &allows_mem, &allows_reg, &is_inout))
	return;

      if (! allows_reg
	  && (allows_mem
	      || is_inout
	      || (DECL_P (val)
		  && REG_P (DECL_RTL (val))
		  && GET_MODE (DECL_RTL (val)) != TYPE_MODE (type))))
	lang_hooks.mark_addressable (val);

      if (is_inout)
	ninout++;
    }

  ninputs += ninout;
  if (ninputs + noutputs > MAX_RECOG_OPERANDS)
    {
      error ("more than %d operands in `asm'", MAX_RECOG_OPERANDS);
      return;
    }

  for (i = 0, tail = inputs; tail; i++, tail = TREE_CHAIN (tail))
    {
      bool allows_reg, allows_mem;
      const char *constraint;

      /* If there's an erroneous arg, emit no insn, because the ASM_INPUT
	 would get VOIDmode and that could cause a crash in reload.  */
      if (TREE_TYPE (TREE_VALUE (tail)) == error_mark_node)
	return;

      constraint = constraints[i + noutputs];
      if (! parse_input_constraint (&constraint, i, ninputs, noutputs, ninout,
				    constraints, &allows_mem, &allows_reg))
	return;

      if (! allows_reg && allows_mem)
	lang_hooks.mark_addressable (TREE_VALUE (tail));
    }

  /* Second pass evaluates arguments.  */

  ninout = 0;
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    {
      tree val = TREE_VALUE (tail);
      tree type = TREE_TYPE (val);
      bool is_inout;
      bool allows_reg;
      bool allows_mem;
      rtx op;

      if (!parse_output_constraint (&constraints[i], i, ninputs,
				    noutputs, &allows_mem, &allows_reg,
				    &is_inout))
	abort ();

      /* If an output operand is not a decl or indirect ref and our constraint
	 allows a register, make a temporary to act as an intermediate.
	 Make the asm insn write into that, then our caller will copy it to
	 the real output operand.  Likewise for promoted variables.  */

      generating_concat_p = 0;

      real_output_rtx[i] = NULL_RTX;
      if ((TREE_CODE (val) == INDIRECT_REF
	   && allows_mem)
	  || (DECL_P (val)
	      && (allows_mem || REG_P (DECL_RTL (val)))
	      && ! (REG_P (DECL_RTL (val))
		    && GET_MODE (DECL_RTL (val)) != TYPE_MODE (type)))
	  || ! allows_reg
	  || is_inout)
	{
	  op = expand_expr (val, NULL_RTX, VOIDmode, EXPAND_WRITE);
	  if (MEM_P (op))
	    op = validize_mem (op);

	  if (! allows_reg && !MEM_P (op))
	    error ("output number %d not directly addressable", i);
	  if ((! allows_mem && MEM_P (op))
	      || GET_CODE (op) == CONCAT)
	    {
	      real_output_rtx[i] = op;
	      op = gen_reg_rtx (GET_MODE (op));
	      if (is_inout)
		emit_move_insn (op, real_output_rtx[i]);
	    }
	}
      else
	{
	  op = assign_temp (type, 0, 0, 1);
	  op = validize_mem (op);
	  TREE_VALUE (tail) = make_tree (type, op);
	}
      output_rtx[i] = op;

      generating_concat_p = old_generating_concat_p;

      if (is_inout)
	{
	  inout_mode[ninout] = TYPE_MODE (type);
	  inout_opnum[ninout++] = i;
	}

      if (decl_conflicts_with_clobbers_p (val, clobbered_regs))
	clobber_conflict_found = 1;
    }

  /* Make vectors for the expression-rtx, constraint strings,
     and named operands.  */

  argvec = rtvec_alloc (ninputs);
  constraintvec = rtvec_alloc (ninputs);

  body = gen_rtx_ASM_OPERANDS ((noutputs == 0 ? VOIDmode
				: GET_MODE (output_rtx[0])),
			       TREE_STRING_POINTER (string),
			       empty_string, 0, argvec, constraintvec,
			       locus);

  MEM_VOLATILE_P (body) = vol;

  /* Eval the inputs and put them into ARGVEC.
     Put their constraints into ASM_INPUTs and store in CONSTRAINTS.  */

  for (i = 0, tail = inputs; tail; tail = TREE_CHAIN (tail), ++i)
    {
      bool allows_reg, allows_mem;
      const char *constraint;
      tree val, type;
      rtx op;

      constraint = constraints[i + noutputs];
      if (! parse_input_constraint (&constraint, i, ninputs, noutputs, ninout,
				    constraints, &allows_mem, &allows_reg))
	abort ();

      generating_concat_p = 0;

      val = TREE_VALUE (tail);
      type = TREE_TYPE (val);
      op = expand_expr (val, NULL_RTX, VOIDmode,
			(allows_mem && !allows_reg
			 ? EXPAND_MEMORY : EXPAND_NORMAL));

      /* Never pass a CONCAT to an ASM.  */
      if (GET_CODE (op) == CONCAT)
	op = force_reg (GET_MODE (op), op);
      else if (MEM_P (op))
	op = validize_mem (op);

      if (asm_operand_ok (op, constraint) <= 0)
	{
	  if (allows_reg)
	    op = force_reg (TYPE_MODE (type), op);
	  else if (!allows_mem)
	    warning ("asm operand %d probably doesn't match constraints",
		     i + noutputs);
	  else if (MEM_P (op))
	    {
	      /* We won't recognize either volatile memory or memory
		 with a queued address as available a memory_operand
		 at this point.  Ignore it: clearly this *is* a memory.  */
	    }
	  else
	    {
	      warning ("use of memory input without lvalue in "
		       "asm operand %d is deprecated", i + noutputs);

	      if (CONSTANT_P (op))
		{
		  rtx mem = force_const_mem (TYPE_MODE (type), op);
		  if (mem)
		    op = validize_mem (mem);
		  else
		    op = force_reg (TYPE_MODE (type), op);
		}
	      if (REG_P (op)
		  || GET_CODE (op) == SUBREG
		  || GET_CODE (op) == CONCAT)
		{
		  tree qual_type = build_qualified_type (type,
							 (TYPE_QUALS (type)
							  | TYPE_QUAL_CONST));
		  rtx memloc = assign_temp (qual_type, 1, 1, 1);
		  memloc = validize_mem (memloc);
		  emit_move_insn (memloc, op);
		  op = memloc;
		}
	    }
	}

      generating_concat_p = old_generating_concat_p;
      ASM_OPERANDS_INPUT (body, i) = op;

      ASM_OPERANDS_INPUT_CONSTRAINT_EXP (body, i)
	= gen_rtx_ASM_INPUT (TYPE_MODE (type), constraints[i + noutputs]);

      if (decl_conflicts_with_clobbers_p (val, clobbered_regs))
	clobber_conflict_found = 1;
    }

  /* Protect all the operands from the queue now that they have all been
     evaluated.  */

  generating_concat_p = 0;

  /* For in-out operands, copy output rtx to input rtx.  */
  for (i = 0; i < ninout; i++)
    {
      int j = inout_opnum[i];
      char buffer[16];

      ASM_OPERANDS_INPUT (body, ninputs - ninout + i)
	= output_rtx[j];

      sprintf (buffer, "%d", j);
      ASM_OPERANDS_INPUT_CONSTRAINT_EXP (body, ninputs - ninout + i)
	= gen_rtx_ASM_INPUT (inout_mode[i], ggc_strdup (buffer));
    }

  generating_concat_p = old_generating_concat_p;

  /* Now, for each output, construct an rtx
     (set OUTPUT (asm_operands INSN OUTPUTCONSTRAINT OUTPUTNUMBER
			       ARGVEC CONSTRAINTS OPNAMES))
     If there is more than one, put them inside a PARALLEL.  */

  if (noutputs == 1 && nclobbers == 0)
    {
      ASM_OPERANDS_OUTPUT_CONSTRAINT (body) = constraints[0];
      emit_insn (gen_rtx_SET (VOIDmode, output_rtx[0], body));
    }

  else if (noutputs == 0 && nclobbers == 0)
    {
      /* No output operands: put in a raw ASM_OPERANDS rtx.  */
      emit_insn (body);
    }

  else
    {
      rtx obody = body;
      int num = noutputs;

      if (num == 0)
	num = 1;

      body = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (num + nclobbers));

      /* For each output operand, store a SET.  */
      for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
	{
	  XVECEXP (body, 0, i)
	    = gen_rtx_SET (VOIDmode,
			   output_rtx[i],
			   gen_rtx_ASM_OPERANDS
			   (GET_MODE (output_rtx[i]),
			    TREE_STRING_POINTER (string),
			    constraints[i], i, argvec, constraintvec,
			    locus));

	  MEM_VOLATILE_P (SET_SRC (XVECEXP (body, 0, i))) = vol;
	}

      /* If there are no outputs (but there are some clobbers)
	 store the bare ASM_OPERANDS into the PARALLEL.  */

      if (i == 0)
	XVECEXP (body, 0, i++) = obody;

      /* Store (clobber REG) for each clobbered register specified.  */

      for (tail = clobbers; tail; tail = TREE_CHAIN (tail))
	{
	  const char *regname = TREE_STRING_POINTER (TREE_VALUE (tail));
	  int j = decode_reg_name (regname);
	  rtx clobbered_reg;

	  if (j < 0)
	    {
	      if (j == -3)	/* `cc', which is not a register */
		continue;

	      if (j == -4)	/* `memory', don't cache memory across asm */
		{
		  XVECEXP (body, 0, i++)
		    = gen_rtx_CLOBBER (VOIDmode,
				       gen_rtx_MEM
				       (BLKmode,
					gen_rtx_SCRATCH (VOIDmode)));
		  continue;
		}

	      /* Ignore unknown register, error already signaled.  */
	      continue;
	    }

	  /* Use QImode since that's guaranteed to clobber just one reg.  */
	  clobbered_reg = gen_rtx_REG (QImode, j);

	  /* Do sanity check for overlap between clobbers and respectively
	     input and outputs that hasn't been handled.  Such overlap
	     should have been detected and reported above.  */
	  if (!clobber_conflict_found)
	    {
	      int opno;

	      /* We test the old body (obody) contents to avoid tripping
		 over the under-construction body.  */
	      for (opno = 0; opno < noutputs; opno++)
		if (reg_overlap_mentioned_p (clobbered_reg, output_rtx[opno]))
		  internal_error ("asm clobber conflict with output operand");

	      for (opno = 0; opno < ninputs - ninout; opno++)
		if (reg_overlap_mentioned_p (clobbered_reg,
					     ASM_OPERANDS_INPUT (obody, opno)))
		  internal_error ("asm clobber conflict with input operand");
	    }

	  XVECEXP (body, 0, i++)
	    = gen_rtx_CLOBBER (VOIDmode, clobbered_reg);
	}

      emit_insn (body);
    }

  /* For any outputs that needed reloading into registers, spill them
     back to where they belong.  */
  for (i = 0; i < noutputs; ++i)
    if (real_output_rtx[i])
      emit_move_insn (real_output_rtx[i], output_rtx[i]);

  free_temp_slots ();
}

void
expand_asm_expr (tree exp)
{
  int noutputs, i;
  tree outputs, tail;
  tree *o;

  if (ASM_INPUT_P (exp))
    {
      expand_asm (ASM_STRING (exp), ASM_VOLATILE_P (exp));
      return;
    }

  outputs = ASM_OUTPUTS (exp);
  noutputs = list_length (outputs);
  /* o[I] is the place that output number I should be written.  */
  o = (tree *) alloca (noutputs * sizeof (tree));

  /* Record the contents of OUTPUTS before it is modified.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    o[i] = TREE_VALUE (tail);

  /* Generate the ASM_OPERANDS insn; store into the TREE_VALUEs of
     OUTPUTS some trees for where the values were actually stored.  */
  expand_asm_operands (ASM_STRING (exp), outputs, ASM_INPUTS (exp),
		       ASM_CLOBBERS (exp), ASM_VOLATILE_P (exp),
		       input_location);

  /* Copy all the intermediate outputs into the specified outputs.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    {
      if (o[i] != TREE_VALUE (tail))
	{
	  expand_assignment (o[i], TREE_VALUE (tail), 0);
	  free_temp_slots ();

	  /* Restore the original value so that it's correct the next
	     time we expand this function.  */
	  TREE_VALUE (tail) = o[i];
	}
    }
}

/* A subroutine of expand_asm_operands.  Check that all operands have
   the same number of alternatives.  Return true if so.  */

static bool
check_operand_nalternatives (tree outputs, tree inputs)
{
  if (outputs || inputs)
    {
      tree tmp = TREE_PURPOSE (outputs ? outputs : inputs);
      int nalternatives
	= n_occurrences (',', TREE_STRING_POINTER (TREE_VALUE (tmp)));
      tree next = inputs;

      if (nalternatives + 1 > MAX_RECOG_ALTERNATIVES)
	{
	  error ("too many alternatives in `asm'");
	  return false;
	}

      tmp = outputs;
      while (tmp)
	{
	  const char *constraint
	    = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (tmp)));

	  if (n_occurrences (',', constraint) != nalternatives)
	    {
	      error ("operand constraints for `asm' differ in number of alternatives");
	      return false;
	    }

	  if (TREE_CHAIN (tmp))
	    tmp = TREE_CHAIN (tmp);
	  else
	    tmp = next, next = 0;
	}
    }

  return true;
}

/* A subroutine of expand_asm_operands.  Check that all operand names
   are unique.  Return true if so.  We rely on the fact that these names
   are identifiers, and so have been canonicalized by get_identifier,
   so all we need are pointer comparisons.  */

static bool
check_unique_operand_names (tree outputs, tree inputs)
{
  tree i, j;

  for (i = outputs; i ; i = TREE_CHAIN (i))
    {
      tree i_name = TREE_PURPOSE (TREE_PURPOSE (i));
      if (! i_name)
	continue;

      for (j = TREE_CHAIN (i); j ; j = TREE_CHAIN (j))
	if (simple_cst_equal (i_name, TREE_PURPOSE (TREE_PURPOSE (j))))
	  goto failure;
    }

  for (i = inputs; i ; i = TREE_CHAIN (i))
    {
      tree i_name = TREE_PURPOSE (TREE_PURPOSE (i));
      if (! i_name)
	continue;

      for (j = TREE_CHAIN (i); j ; j = TREE_CHAIN (j))
	if (simple_cst_equal (i_name, TREE_PURPOSE (TREE_PURPOSE (j))))
	  goto failure;
      for (j = outputs; j ; j = TREE_CHAIN (j))
	if (simple_cst_equal (i_name, TREE_PURPOSE (TREE_PURPOSE (j))))
	  goto failure;
    }

  return true;

 failure:
  error ("duplicate asm operand name '%s'",
	 TREE_STRING_POINTER (TREE_PURPOSE (TREE_PURPOSE (i))));
  return false;
}

/* A subroutine of expand_asm_operands.  Resolve the names of the operands
   in *POUTPUTS and *PINPUTS to numbers, and replace the name expansions in
   STRING and in the constraints to those numbers.  */

tree
resolve_asm_operand_names (tree string, tree outputs, tree inputs)
{
  char *buffer;
  char *p;
  const char *c;
  tree t;

  check_unique_operand_names (outputs, inputs);

  /* Substitute [<name>] in input constraint strings.  There should be no
     named operands in output constraints.  */
  for (t = inputs; t ; t = TREE_CHAIN (t))
    {
      c = TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (t)));
      if (strchr (c, '[') != NULL)
	{
	  p = buffer = xstrdup (c);
	  while ((p = strchr (p, '[')) != NULL)
	    p = resolve_operand_name_1 (p, outputs, inputs);
	  TREE_VALUE (TREE_PURPOSE (t))
	    = build_string (strlen (buffer), buffer);
	  free (buffer);
	}
    }

  /* Now check for any needed substitutions in the template.  */
  c = TREE_STRING_POINTER (string);
  while ((c = strchr (c, '%')) != NULL)
    {
      if (c[1] == '[')
	break;
      else if (ISALPHA (c[1]) && c[2] == '[')
	break;
      else
	{
	  c += 1;
	  continue;
	}
    }

  if (c)
    {
      /* OK, we need to make a copy so we can perform the substitutions.
	 Assume that we will not need extra space--we get to remove '['
	 and ']', which means we cannot have a problem until we have more
	 than 999 operands.  */
      buffer = xstrdup (TREE_STRING_POINTER (string));
      p = buffer + (c - TREE_STRING_POINTER (string));

      while ((p = strchr (p, '%')) != NULL)
	{
	  if (p[1] == '[')
	    p += 1;
	  else if (ISALPHA (p[1]) && p[2] == '[')
	    p += 2;
	  else
	    {
	      p += 1;
	      continue;
	    }

	  p = resolve_operand_name_1 (p, outputs, inputs);
	}

      string = build_string (strlen (buffer), buffer);
      free (buffer);
    }

  return string;
}

/* A subroutine of resolve_operand_names.  P points to the '[' for a
   potential named operand of the form [<name>].  In place, replace
   the name and brackets with a number.  Return a pointer to the
   balance of the string after substitution.  */

static char *
resolve_operand_name_1 (char *p, tree outputs, tree inputs)
{
  char *q;
  int op;
  tree t;
  size_t len;

  /* Collect the operand name.  */
  q = strchr (p, ']');
  if (!q)
    {
      error ("missing close brace for named operand");
      return strchr (p, '\0');
    }
  len = q - p - 1;

  /* Resolve the name to a number.  */
  for (op = 0, t = outputs; t ; t = TREE_CHAIN (t), op++)
    {
      tree name = TREE_PURPOSE (TREE_PURPOSE (t));
      if (name)
	{
	  const char *c = TREE_STRING_POINTER (name);
	  if (strncmp (c, p + 1, len) == 0 && c[len] == '\0')
	    goto found;
	}
    }
  for (t = inputs; t ; t = TREE_CHAIN (t), op++)
    {
      tree name = TREE_PURPOSE (TREE_PURPOSE (t));
      if (name)
	{
	  const char *c = TREE_STRING_POINTER (name);
	  if (strncmp (c, p + 1, len) == 0 && c[len] == '\0')
	    goto found;
	}
    }

  *q = '\0';
  error ("undefined named operand '%s'", p + 1);
  op = 0;
 found:

  /* Replace the name with the number.  Unfortunately, not all libraries
     get the return value of sprintf correct, so search for the end of the
     generated string by hand.  */
  sprintf (p, "%d", op);
  p = strchr (p, '\0');

  /* Verify the no extra buffer space assumption.  */
  if (p > q)
    abort ();

  /* Shift the rest of the buffer down to fill the gap.  */
  memmove (p, q + 1, strlen (q + 1) + 1);

  return p;
}

/* Generate RTL to evaluate the expression EXP.  */

void
expand_expr_stmt (tree exp)
{
  rtx value;
  tree type;

  value = expand_expr (exp, const0_rtx, VOIDmode, 0);
  type = TREE_TYPE (exp);

  /* If all we do is reference a volatile value in memory,
     copy it to a register to be sure it is actually touched.  */
  if (value && MEM_P (value) && TREE_THIS_VOLATILE (exp))
    {
      if (TYPE_MODE (type) == VOIDmode)
	;
      else if (TYPE_MODE (type) != BLKmode)
	value = copy_to_reg (value);
      else
	{
	  rtx lab = gen_label_rtx ();

	  /* Compare the value with itself to reference it.  */
	  emit_cmp_and_jump_insns (value, value, EQ,
				   expand_expr (TYPE_SIZE (type),
						NULL_RTX, VOIDmode, 0),
				   BLKmode, 0, lab);
	  emit_label (lab);
	}
    }

  /* Free any temporaries used to evaluate this expression.  */
  free_temp_slots ();
}

/* Warn if EXP contains any computations whose results are not used.
   Return 1 if a warning is printed; 0 otherwise.  LOCUS is the
   (potential) location of the expression.  */

int
warn_if_unused_value (tree exp, location_t locus)
{
 restart:
  if (TREE_USED (exp))
    return 0;

  /* Don't warn about void constructs.  This includes casting to void,
     void function calls, and statement expressions with a final cast
     to void.  */
  if (VOID_TYPE_P (TREE_TYPE (exp)))
    return 0;

  if (EXPR_HAS_LOCATION (exp))
    locus = EXPR_LOCATION (exp);

  switch (TREE_CODE (exp))
    {
    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case MODIFY_EXPR:
    case INIT_EXPR:
    case TARGET_EXPR:
    case CALL_EXPR:
    case TRY_CATCH_EXPR:
    case WITH_CLEANUP_EXPR:
    case EXIT_EXPR:
      return 0;

    case BIND_EXPR:
      /* For a binding, warn if no side effect within it.  */
      exp = BIND_EXPR_BODY (exp);
      goto restart;

    case SAVE_EXPR:
      exp = TREE_OPERAND (exp, 0);
      goto restart;

    case TRUTH_ORIF_EXPR:
    case TRUTH_ANDIF_EXPR:
      /* In && or ||, warn if 2nd operand has no side effect.  */
      exp = TREE_OPERAND (exp, 1);
      goto restart;

    case COMPOUND_EXPR:
      if (TREE_NO_WARNING (exp))
	return 0;
      if (warn_if_unused_value (TREE_OPERAND (exp, 0), locus))
	return 1;
      /* Let people do `(foo (), 0)' without a warning.  */
      if (TREE_CONSTANT (TREE_OPERAND (exp, 1)))
	return 0;
      exp = TREE_OPERAND (exp, 1);
      goto restart;

    case NOP_EXPR:
    case CONVERT_EXPR:
    case NON_LVALUE_EXPR:
      /* Don't warn about conversions not explicit in the user's program.  */
      if (TREE_NO_WARNING (exp))
	return 0;
      /* Assignment to a cast usually results in a cast of a modify.
	 Don't complain about that.  There can be an arbitrary number of
	 casts before the modify, so we must loop until we find the first
	 non-cast expression and then test to see if that is a modify.  */
      {
	tree tem = TREE_OPERAND (exp, 0);

	while (TREE_CODE (tem) == CONVERT_EXPR || TREE_CODE (tem) == NOP_EXPR)
	  tem = TREE_OPERAND (tem, 0);

	if (TREE_CODE (tem) == MODIFY_EXPR || TREE_CODE (tem) == INIT_EXPR
	    || TREE_CODE (tem) == CALL_EXPR)
	  return 0;
      }
      goto maybe_warn;

    case INDIRECT_REF:
      /* Don't warn about automatic dereferencing of references, since
	 the user cannot control it.  */
      if (TREE_CODE (TREE_TYPE (TREE_OPERAND (exp, 0))) == REFERENCE_TYPE)
	{
	  exp = TREE_OPERAND (exp, 0);
	  goto restart;
	}
      /* Fall through.  */

    default:
      /* Referencing a volatile value is a side effect, so don't warn.  */
      if ((DECL_P (exp)
	   || TREE_CODE_CLASS (TREE_CODE (exp)) == 'r')
	  && TREE_THIS_VOLATILE (exp))
	return 0;

      /* If this is an expression which has no operands, there is no value
	 to be unused.  There are no such language-independent codes,
	 but front ends may define such.  */
      if (TREE_CODE_CLASS (TREE_CODE (exp)) == 'e'
	  && TREE_CODE_LENGTH (TREE_CODE (exp)) == 0)
	return 0;

    maybe_warn:
      /* If this is an expression with side effects, don't warn.  */
      if (TREE_SIDE_EFFECTS (exp))
	return 0;

      warning ("%Hvalue computed is not used", &locus);
      return 1;
    }
}

/* Generate RTL for the start of an if-then.  COND is the expression
   whose truth should be tested.

   If EXITFLAG is nonzero, this conditional is visible to
   `exit_something'.  */

void
expand_start_cond (tree cond, int exitflag)
{
  struct nesting *thiscond = ALLOC_NESTING ();

  /* Make an entry on cond_stack for the cond we are entering.  */

  thiscond->desc = COND_NESTING;
  thiscond->next = cond_stack;
  thiscond->all = nesting_stack;
  thiscond->depth = ++nesting_depth;
  thiscond->data.cond.next_label = gen_label_rtx ();
  /* Before we encounter an `else', we don't need a separate exit label
     unless there are supposed to be exit statements
     to exit this conditional.  */
  thiscond->exit_label = exitflag ? gen_label_rtx () : 0;
  thiscond->data.cond.endif_label = thiscond->exit_label;
  cond_stack = thiscond;
  nesting_stack = thiscond;

  do_jump (cond, thiscond->data.cond.next_label, NULL_RTX);
}

/* Generate RTL between then-clause and the elseif-clause
   of an if-then-elseif-....  */

void
expand_start_elseif (tree cond)
{
  if (cond_stack->data.cond.endif_label == 0)
    cond_stack->data.cond.endif_label = gen_label_rtx ();
  emit_jump (cond_stack->data.cond.endif_label);
  emit_label (cond_stack->data.cond.next_label);
  cond_stack->data.cond.next_label = gen_label_rtx ();
  do_jump (cond, cond_stack->data.cond.next_label, NULL_RTX);
}

/* Generate RTL between the then-clause and the else-clause
   of an if-then-else.  */

void
expand_start_else (void)
{
  if (cond_stack->data.cond.endif_label == 0)
    cond_stack->data.cond.endif_label = gen_label_rtx ();

  emit_jump (cond_stack->data.cond.endif_label);
  emit_label (cond_stack->data.cond.next_label);
  cond_stack->data.cond.next_label = 0;  /* No more _else or _elseif calls.  */
}

/* After calling expand_start_else, turn this "else" into an "else if"
   by providing another condition.  */

void
expand_elseif (tree cond)
{
  cond_stack->data.cond.next_label = gen_label_rtx ();
  do_jump (cond, cond_stack->data.cond.next_label, NULL_RTX);
}

/* Generate RTL for the end of an if-then.
   Pop the record for it off of cond_stack.  */

void
expand_end_cond (void)
{
  struct nesting *thiscond = cond_stack;

  do_pending_stack_adjust ();
  if (thiscond->data.cond.next_label)
    emit_label (thiscond->data.cond.next_label);
  if (thiscond->data.cond.endif_label)
    emit_label (thiscond->data.cond.endif_label);

  POPSTACK (cond_stack);
}

/* Return nonzero if we should preserve sub-expressions as separate
   pseudos.  We never do so if we aren't optimizing.  We always do so
   if -fexpensive-optimizations.  */

int
preserve_subexpressions_p (void)
{
  if (flag_expensive_optimizations)
    return 1;

  if (optimize == 0 || cfun == 0 || cfun->stmt == 0)
    return 0;

  return 1;
}


/* Generate RTL to return from the current function, with no value.
   (That is, we do not do anything about returning any value.)  */

void
expand_null_return (void)
{
  /* If this function was declared to return a value, but we
     didn't, clobber the return registers so that they are not
     propagated live to the rest of the function.  */
  clobber_return_register ();

  expand_null_return_1 ();
}

/* Generate RTL to return directly from the current function.
   (That is, we bypass any return value.)  */

void
expand_naked_return (void)
{
  rtx end_label;

  clear_pending_stack_adjust ();
  do_pending_stack_adjust ();

  end_label = naked_return_label;
  if (end_label == 0)
    end_label = naked_return_label = gen_label_rtx ();

  emit_jump (end_label);
}

/* If the current function returns values in the most significant part
   of a register, shift return value VAL appropriately.  The mode of
   the function's return type is known not to be BLKmode.  */

static rtx
shift_return_value (rtx val)
{
  tree type;

  type = TREE_TYPE (DECL_RESULT (current_function_decl));
  if (targetm.calls.return_in_msb (type))
    {
      rtx target;
      HOST_WIDE_INT shift;

      target = DECL_RTL (DECL_RESULT (current_function_decl));
      shift = (GET_MODE_BITSIZE (GET_MODE (target))
	       - BITS_PER_UNIT * int_size_in_bytes (type));
      if (shift > 0)
	val = expand_shift (LSHIFT_EXPR, GET_MODE (target),
			    gen_lowpart (GET_MODE (target), val),
			    build_int_2 (shift, 0), target, 1);
    }
  return val;
}


/* Generate RTL to return from the current function, with value VAL.  */

static void
expand_value_return (rtx val)
{
  /* Copy the value to the return location
     unless it's already there.  */

  rtx return_reg = DECL_RTL (DECL_RESULT (current_function_decl));
  if (return_reg != val)
    {
      tree type = TREE_TYPE (DECL_RESULT (current_function_decl));
      if (targetm.calls.promote_function_return (TREE_TYPE (current_function_decl)))
      {
	int unsignedp = TYPE_UNSIGNED (type);
	enum machine_mode old_mode
	  = DECL_MODE (DECL_RESULT (current_function_decl));
	enum machine_mode mode
	  = promote_mode (type, old_mode, &unsignedp, 1);

	if (mode != old_mode)
	  val = convert_modes (mode, old_mode, val, unsignedp);
      }
      if (GET_CODE (return_reg) == PARALLEL)
	emit_group_load (return_reg, val, type, int_size_in_bytes (type));
      else
	emit_move_insn (return_reg, val);
    }

  expand_null_return_1 ();
}

/* Output a return with no value.  */

static void
expand_null_return_1 (void)
{
  rtx end_label;

  clear_pending_stack_adjust ();
  do_pending_stack_adjust ();

  end_label = return_label;
  if (end_label == 0)
     end_label = return_label = gen_label_rtx ();
  emit_jump (end_label);
}

/* Generate RTL to evaluate the expression RETVAL and return it
   from the current function.  */

void
expand_return (tree retval)
{
  rtx result_rtl;
  rtx val = 0;
  tree retval_rhs;

  /* If function wants no value, give it none.  */
  if (TREE_CODE (TREE_TYPE (TREE_TYPE (current_function_decl))) == VOID_TYPE)
    {
      expand_expr (retval, NULL_RTX, VOIDmode, 0);
      expand_null_return ();
      return;
    }

  if (retval == error_mark_node)
    {
      /* Treat this like a return of no value from a function that
	 returns a value.  */
      expand_null_return ();
      return;
    }
  else if (TREE_CODE (retval) == RESULT_DECL)
    retval_rhs = retval;
  else if ((TREE_CODE (retval) == MODIFY_EXPR
	    || TREE_CODE (retval) == INIT_EXPR)
	   && TREE_CODE (TREE_OPERAND (retval, 0)) == RESULT_DECL)
    retval_rhs = TREE_OPERAND (retval, 1);
  else
    retval_rhs = retval;

  result_rtl = DECL_RTL (DECL_RESULT (current_function_decl));

  /* If the result is an aggregate that is being returned in one (or more)
     registers, load the registers here.  The compiler currently can't handle
     copying a BLKmode value into registers.  We could put this code in a
     more general area (for use by everyone instead of just function
     call/return), but until this feature is generally usable it is kept here
     (and in expand_call).  */

  if (retval_rhs != 0
      && TYPE_MODE (TREE_TYPE (retval_rhs)) == BLKmode
      && REG_P (result_rtl))
    {
      int i;
      unsigned HOST_WIDE_INT bitpos, xbitpos;
      unsigned HOST_WIDE_INT padding_correction = 0;
      unsigned HOST_WIDE_INT bytes
	= int_size_in_bytes (TREE_TYPE (retval_rhs));
      int n_regs = (bytes + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
      unsigned int bitsize
	= MIN (TYPE_ALIGN (TREE_TYPE (retval_rhs)), BITS_PER_WORD);
      rtx *result_pseudos = alloca (sizeof (rtx) * n_regs);
      rtx result_reg, src = NULL_RTX, dst = NULL_RTX;
      rtx result_val = expand_expr (retval_rhs, NULL_RTX, VOIDmode, 0);
      enum machine_mode tmpmode, result_reg_mode;

      if (bytes == 0)
	{
	  expand_null_return ();
	  return;
	}

      /* If the structure doesn't take up a whole number of words, see
	 whether the register value should be padded on the left or on
	 the right.  Set PADDING_CORRECTION to the number of padding
	 bits needed on the left side.

	 In most ABIs, the structure will be returned at the least end of
	 the register, which translates to right padding on little-endian
	 targets and left padding on big-endian targets.  The opposite
	 holds if the structure is returned at the most significant
	 end of the register.  */
      if (bytes % UNITS_PER_WORD != 0
	  && (targetm.calls.return_in_msb (TREE_TYPE (retval_rhs))
	      ? !BYTES_BIG_ENDIAN
	      : BYTES_BIG_ENDIAN))
	padding_correction = (BITS_PER_WORD - ((bytes % UNITS_PER_WORD)
					       * BITS_PER_UNIT));

      /* Copy the structure BITSIZE bits at a time.  */
      for (bitpos = 0, xbitpos = padding_correction;
	   bitpos < bytes * BITS_PER_UNIT;
	   bitpos += bitsize, xbitpos += bitsize)
	{
	  /* We need a new destination pseudo each time xbitpos is
	     on a word boundary and when xbitpos == padding_correction
	     (the first time through).  */
	  if (xbitpos % BITS_PER_WORD == 0
	      || xbitpos == padding_correction)
	    {
	      /* Generate an appropriate register.  */
	      dst = gen_reg_rtx (word_mode);
	      result_pseudos[xbitpos / BITS_PER_WORD] = dst;

	      /* Clear the destination before we move anything into it.  */
	      emit_move_insn (dst, CONST0_RTX (GET_MODE (dst)));
	    }

	  /* We need a new source operand each time bitpos is on a word
	     boundary.  */
	  if (bitpos % BITS_PER_WORD == 0)
	    src = operand_subword_force (result_val,
					 bitpos / BITS_PER_WORD,
					 BLKmode);

	  /* Use bitpos for the source extraction (left justified) and
	     xbitpos for the destination store (right justified).  */
	  store_bit_field (dst, bitsize, xbitpos % BITS_PER_WORD, word_mode,
			   extract_bit_field (src, bitsize,
					      bitpos % BITS_PER_WORD, 1,
					      NULL_RTX, word_mode, word_mode));
	}

      tmpmode = GET_MODE (result_rtl);
      if (tmpmode == BLKmode)
	{
	  /* Find the smallest integer mode large enough to hold the
	     entire structure and use that mode instead of BLKmode
	     on the USE insn for the return register.  */
	  for (tmpmode = GET_CLASS_NARROWEST_MODE (MODE_INT);
	       tmpmode != VOIDmode;
	       tmpmode = GET_MODE_WIDER_MODE (tmpmode))
	    /* Have we found a large enough mode?  */
	    if (GET_MODE_SIZE (tmpmode) >= bytes)
	      break;

	  /* No suitable mode found.  */
	  if (tmpmode == VOIDmode)
	    abort ();

	  PUT_MODE (result_rtl, tmpmode);
	}

      if (GET_MODE_SIZE (tmpmode) < GET_MODE_SIZE (word_mode))
	result_reg_mode = word_mode;
      else
	result_reg_mode = tmpmode;
      result_reg = gen_reg_rtx (result_reg_mode);

      for (i = 0; i < n_regs; i++)
	emit_move_insn (operand_subword (result_reg, i, 0, result_reg_mode),
			result_pseudos[i]);

      if (tmpmode != result_reg_mode)
	result_reg = gen_lowpart (tmpmode, result_reg);

      expand_value_return (result_reg);
    }
  else if (retval_rhs != 0
	   && !VOID_TYPE_P (TREE_TYPE (retval_rhs))
	   && (REG_P (result_rtl)
	       || (GET_CODE (result_rtl) == PARALLEL)))
    {
      /* Calculate the return value into a temporary (usually a pseudo
         reg).  */
      tree ot = TREE_TYPE (DECL_RESULT (current_function_decl));
      tree nt = build_qualified_type (ot, TYPE_QUALS (ot) | TYPE_QUAL_CONST);

      val = assign_temp (nt, 0, 0, 1);
      val = expand_expr (retval_rhs, val, GET_MODE (val), 0);
      val = force_not_mem (val);
      /* Return the calculated value.  */
      expand_value_return (shift_return_value (val));
    }
  else
    {
      /* No hard reg used; calculate value into hard return reg.  */
      expand_expr (retval, const0_rtx, VOIDmode, 0);
      expand_value_return (result_rtl);
    }
}

/* Given a pointer to a BLOCK node return nonzero if (and only if) the node
   in question represents the outermost pair of curly braces (i.e. the "body
   block") of a function or method.

   For any BLOCK node representing a "body block" of a function or method, the
   BLOCK_SUPERCONTEXT of the node will point to another BLOCK node which
   represents the outermost (function) scope for the function or method (i.e.
   the one which includes the formal parameters).  The BLOCK_SUPERCONTEXT of
   *that* node in turn will point to the relevant FUNCTION_DECL node.  */

int
is_body_block (tree stmt)
{
  if (lang_hooks.no_body_blocks)
    return 0;

  if (TREE_CODE (stmt) == BLOCK)
    {
      tree parent = BLOCK_SUPERCONTEXT (stmt);

      if (parent && TREE_CODE (parent) == BLOCK)
	{
	  tree grandparent = BLOCK_SUPERCONTEXT (parent);

	  if (grandparent && TREE_CODE (grandparent) == FUNCTION_DECL)
	    return 1;
	}
    }

  return 0;
}

/* Emit code to restore vital registers at the beginning of a nonlocal goto
   handler.  */
static void
expand_nl_goto_receiver (void)
{
  /* Clobber the FP when we get here, so we have to make sure it's
     marked as used by this function.  */
  emit_insn (gen_rtx_USE (VOIDmode, hard_frame_pointer_rtx));

  /* Mark the static chain as clobbered here so life information
     doesn't get messed up for it.  */
  emit_insn (gen_rtx_CLOBBER (VOIDmode, static_chain_rtx));

#ifdef HAVE_nonlocal_goto
  if (! HAVE_nonlocal_goto)
#endif
    /* First adjust our frame pointer to its actual value.  It was
       previously set to the start of the virtual area corresponding to
       the stacked variables when we branched here and now needs to be
       adjusted to the actual hardware fp value.

       Assignments are to virtual registers are converted by
       instantiate_virtual_regs into the corresponding assignment
       to the underlying register (fp in this case) that makes
       the original assignment true.
       So the following insn will actually be
       decrementing fp by STARTING_FRAME_OFFSET.  */
    emit_move_insn (virtual_stack_vars_rtx, hard_frame_pointer_rtx);

#if ARG_POINTER_REGNUM != HARD_FRAME_POINTER_REGNUM
  if (fixed_regs[ARG_POINTER_REGNUM])
    {
#ifdef ELIMINABLE_REGS
      /* If the argument pointer can be eliminated in favor of the
	 frame pointer, we don't need to restore it.  We assume here
	 that if such an elimination is present, it can always be used.
	 This is the case on all known machines; if we don't make this
	 assumption, we do unnecessary saving on many machines.  */
      static const struct elims {const int from, to;} elim_regs[] = ELIMINABLE_REGS;
      size_t i;

      for (i = 0; i < ARRAY_SIZE (elim_regs); i++)
	if (elim_regs[i].from == ARG_POINTER_REGNUM
	    && elim_regs[i].to == HARD_FRAME_POINTER_REGNUM)
	  break;

      if (i == ARRAY_SIZE (elim_regs))
#endif
	{
	  /* Now restore our arg pointer from the address at which it
	     was saved in our stack frame.  */
	  emit_move_insn (virtual_incoming_args_rtx,
			  copy_to_reg (get_arg_pointer_save_area (cfun)));
	}
    }
#endif

#ifdef HAVE_nonlocal_goto_receiver
  if (HAVE_nonlocal_goto_receiver)
    emit_insn (gen_nonlocal_goto_receiver ());
#endif

  /* @@@ This is a kludge.  Not all machine descriptions define a blockage
     insn, but we must not allow the code we just generated to be reordered
     by scheduling.  Specifically, the update of the frame pointer must
     happen immediately, not later.  So emit an ASM_INPUT to act as blockage
     insn.  */
  emit_insn (gen_rtx_ASM_INPUT (VOIDmode, ""));
}

/* Generate RTL for the automatic variable declaration DECL.
   (Other kinds of declarations are simply ignored if seen here.)  */

void
expand_decl (tree decl)
{
  tree type;

  type = TREE_TYPE (decl);

  /* For a CONST_DECL, set mode, alignment, and sizes from those of the
     type in case this node is used in a reference.  */
  if (TREE_CODE (decl) == CONST_DECL)
    {
      DECL_MODE (decl) = TYPE_MODE (type);
      DECL_ALIGN (decl) = TYPE_ALIGN (type);
      DECL_SIZE (decl) = TYPE_SIZE (type);
      DECL_SIZE_UNIT (decl) = TYPE_SIZE_UNIT (type);
      return;
    }

  /* Otherwise, only automatic variables need any expansion done.  Static and
     external variables, and external functions, will be handled by
     `assemble_variable' (called from finish_decl).  TYPE_DECL requires
     nothing.  PARM_DECLs are handled in `assign_parms'.  */
  if (TREE_CODE (decl) != VAR_DECL)
    return;

  if (TREE_STATIC (decl) || DECL_EXTERNAL (decl))
    return;

  /* Create the RTL representation for the variable.  */

  if (type == error_mark_node)
    SET_DECL_RTL (decl, gen_rtx_MEM (BLKmode, const0_rtx));

  else if (DECL_SIZE (decl) == 0)
    /* Variable with incomplete type.  */
    {
      rtx x;
      if (DECL_INITIAL (decl) == 0)
	/* Error message was already done; now avoid a crash.  */
	x = gen_rtx_MEM (BLKmode, const0_rtx);
      else
	/* An initializer is going to decide the size of this array.
	   Until we know the size, represent its address with a reg.  */
	x = gen_rtx_MEM (BLKmode, gen_reg_rtx (Pmode));

      set_mem_attributes (x, decl, 1);
      SET_DECL_RTL (decl, x);
    }
  else if (use_register_for_decl (decl))
    {
      /* Automatic variable that can go in a register.  */
      int unsignedp = TYPE_UNSIGNED (type);
      enum machine_mode reg_mode
	= promote_mode (type, DECL_MODE (decl), &unsignedp, 0);

      SET_DECL_RTL (decl, gen_reg_rtx (reg_mode));

      /* Note if the object is a user variable.  */
      if (!DECL_ARTIFICIAL (decl))
	{
	  mark_user_reg (DECL_RTL (decl));

	  /* Trust user variables which have a pointer type to really
	     be pointers.  Do not trust compiler generated temporaries
	     as our type system is totally busted as it relates to
	     pointer arithmetic which translates into lots of compiler
	     generated objects with pointer types, but which are not really
	     pointers.  */
	  if (POINTER_TYPE_P (type))
	    mark_reg_pointer (DECL_RTL (decl),
			      TYPE_ALIGN (TREE_TYPE (TREE_TYPE (decl))));
	}

      maybe_set_unchanging (DECL_RTL (decl), decl);
    }

  else if (TREE_CODE (DECL_SIZE_UNIT (decl)) == INTEGER_CST
	   && ! (flag_stack_check && ! STACK_CHECK_BUILTIN
		 && 0 < compare_tree_int (DECL_SIZE_UNIT (decl),
					  STACK_CHECK_MAX_VAR_SIZE)))
    {
      /* Variable of fixed size that goes on the stack.  */
      rtx oldaddr = 0;
      rtx addr;
      rtx x;

      /* If we previously made RTL for this decl, it must be an array
	 whose size was determined by the initializer.
	 The old address was a register; set that register now
	 to the proper address.  */
      if (DECL_RTL_SET_P (decl))
	{
	  if (!MEM_P (DECL_RTL (decl))
	      || !REG_P (XEXP (DECL_RTL (decl), 0)))
	    abort ();
	  oldaddr = XEXP (DECL_RTL (decl), 0);
	}

      /* Set alignment we actually gave this decl.  */
      DECL_ALIGN (decl) = (DECL_MODE (decl) == BLKmode ? BIGGEST_ALIGNMENT
			   : GET_MODE_BITSIZE (DECL_MODE (decl)));
      DECL_USER_ALIGN (decl) = 0;

      x = assign_temp (decl, 1, 1, 1);
      set_mem_attributes (x, decl, 1);
      SET_DECL_RTL (decl, x);

      if (oldaddr)
	{
	  addr = force_operand (XEXP (DECL_RTL (decl), 0), oldaddr);
	  if (addr != oldaddr)
	    emit_move_insn (oldaddr, addr);
	}
    }
  else
    /* Dynamic-size object: must push space on the stack.  */
    {
      rtx address, size, x;

      /* Record the stack pointer on entry to block, if have
	 not already done so.  */
      do_pending_stack_adjust ();

      /* Compute the variable's size, in bytes.  This will expand any
	 needed SAVE_EXPRs for the first time.  */
      size = expand_expr (DECL_SIZE_UNIT (decl), NULL_RTX, VOIDmode, 0);
      free_temp_slots ();

      /* Allocate space on the stack for the variable.  Note that
	 DECL_ALIGN says how the variable is to be aligned and we
	 cannot use it to conclude anything about the alignment of
	 the size.  */
      address = allocate_dynamic_stack_space (size, NULL_RTX,
					      TYPE_ALIGN (TREE_TYPE (decl)));

      /* Reference the variable indirect through that rtx.  */
      x = gen_rtx_MEM (DECL_MODE (decl), address);
      set_mem_attributes (x, decl, 1);
      SET_DECL_RTL (decl, x);


      /* Indicate the alignment we actually gave this variable.  */
#ifdef STACK_BOUNDARY
      DECL_ALIGN (decl) = STACK_BOUNDARY;
#else
      DECL_ALIGN (decl) = BIGGEST_ALIGNMENT;
#endif
      DECL_USER_ALIGN (decl) = 0;
    }
}

/* Emit code to allocate T_SIZE bytes of dynamic stack space for ALLOC.  */
void
expand_stack_alloc (tree alloc, tree t_size)
{
  rtx address, dest, size;
  tree var, type;

  if (TREE_CODE (alloc) != ADDR_EXPR)
    abort ();
  var = TREE_OPERAND (alloc, 0);
  if (TREE_CODE (var) != VAR_DECL)
    abort ();

  type = TREE_TYPE (var);

  /* Compute the variable's size, in bytes.  */
  size = expand_expr (t_size, NULL_RTX, VOIDmode, 0);
  free_temp_slots ();

  /* Allocate space on the stack for the variable.  */
  address = XEXP (DECL_RTL (var), 0);
  dest = allocate_dynamic_stack_space (size, address, TYPE_ALIGN (type));
  if (dest != address)
    emit_move_insn (address, dest);

  /* Indicate the alignment we actually gave this variable.  */
#ifdef STACK_BOUNDARY
  DECL_ALIGN (var) = STACK_BOUNDARY;
#else
  DECL_ALIGN (var) = BIGGEST_ALIGNMENT;
#endif
  DECL_USER_ALIGN (var) = 0;
}

/* Emit code to save the current value of stack.  */
rtx
expand_stack_save (void)
{
  rtx ret = NULL_RTX;

  do_pending_stack_adjust ();
  emit_stack_save (SAVE_BLOCK, &ret, NULL_RTX);
  return ret;
}

/* Emit code to restore the current value of stack.  */
void
expand_stack_restore (tree var)
{
  rtx sa = DECL_RTL (var);

  emit_stack_restore (SAVE_BLOCK, sa, NULL_RTX);
}

/* Emit code to perform the initialization of a declaration DECL.  */

void
expand_decl_init (tree decl)
{
  int was_used = TREE_USED (decl);

  /* If this is a CONST_DECL, we don't have to generate any code.  Likewise
     for static decls.  */
  if (TREE_CODE (decl) == CONST_DECL
      || TREE_STATIC (decl))
    return;

  /* Compute and store the initial value now.  */

  push_temp_slots ();

  if (DECL_INITIAL (decl) == error_mark_node)
    {
      enum tree_code code = TREE_CODE (TREE_TYPE (decl));

      if (code == INTEGER_TYPE || code == REAL_TYPE || code == ENUMERAL_TYPE
	  || code == POINTER_TYPE || code == REFERENCE_TYPE)
	expand_assignment (decl, convert (TREE_TYPE (decl), integer_zero_node),
			   0);
    }
  else if (DECL_INITIAL (decl) && TREE_CODE (DECL_INITIAL (decl)) != TREE_LIST)
    {
      emit_line_note (DECL_SOURCE_LOCATION (decl));
      expand_assignment (decl, DECL_INITIAL (decl), 0);
    }

  /* Don't let the initialization count as "using" the variable.  */
  TREE_USED (decl) = was_used;

  /* Free any temporaries we made while initializing the decl.  */
  preserve_temp_slots (NULL_RTX);
  free_temp_slots ();
  pop_temp_slots ();
}


/* DECL is an anonymous union.  CLEANUP is a cleanup for DECL.
   DECL_ELTS is the list of elements that belong to DECL's type.
   In each, the TREE_VALUE is a VAR_DECL, and the TREE_PURPOSE a cleanup.  */

void
expand_anon_union_decl (tree decl, tree cleanup ATTRIBUTE_UNUSED,
			tree decl_elts)
{
  rtx x;
  tree t;

  /* If any of the elements are addressable, so is the entire union.  */
  for (t = decl_elts; t; t = TREE_CHAIN (t))
    if (TREE_ADDRESSABLE (TREE_VALUE (t)))
      {
	TREE_ADDRESSABLE (decl) = 1;
	break;
      }

  expand_decl (decl);
  x = DECL_RTL (decl);

  /* Go through the elements, assigning RTL to each.  */
  for (t = decl_elts; t; t = TREE_CHAIN (t))
    {
      tree decl_elt = TREE_VALUE (t);
      enum machine_mode mode = TYPE_MODE (TREE_TYPE (decl_elt));

      /* If any of the elements are addressable, so is the entire
	 union.  */
      if (TREE_USED (decl_elt))
	TREE_USED (decl) = 1;

      /* Propagate the union's alignment to the elements.  */
      DECL_ALIGN (decl_elt) = DECL_ALIGN (decl);
      DECL_USER_ALIGN (decl_elt) = DECL_USER_ALIGN (decl);

      /* If the element has BLKmode and the union doesn't, the union is
         aligned such that the element doesn't need to have BLKmode, so
         change the element's mode to the appropriate one for its size.  */
      if (mode == BLKmode && DECL_MODE (decl) != BLKmode)
	DECL_MODE (decl_elt) = mode
	  = mode_for_size_tree (DECL_SIZE (decl_elt), MODE_INT, 1);

      /* (SUBREG (MEM ...)) at RTL generation time is invalid, so we
         instead create a new MEM rtx with the proper mode.  */
      if (MEM_P (x))
	{
	  if (mode == GET_MODE (x))
	    SET_DECL_RTL (decl_elt, x);
	  else
	    SET_DECL_RTL (decl_elt, adjust_address_nv (x, mode, 0));
	}
      else if (REG_P (x))
	{
	  if (mode == GET_MODE (x))
	    SET_DECL_RTL (decl_elt, x);
	  else
	    SET_DECL_RTL (decl_elt, gen_lowpart_SUBREG (mode, x));
	}
      else
	abort ();
    }
}

/* Enter a case (Pascal) or switch (C) statement.
   Push a block onto case_stack and nesting_stack
   to accumulate the case-labels that are seen
   and to record the labels generated for the statement.

   EXIT_FLAG is nonzero if `exit_something' should exit this case stmt.
   Otherwise, this construct is transparent for `exit_something'.

   EXPR is the index-expression to be dispatched on.
   TYPE is its nominal type.  We could simply convert EXPR to this type,
   but instead we take short cuts.  */

void
expand_start_case (tree index_expr)
{
  struct nesting *thiscase = ALLOC_NESTING ();

  /* Make an entry on case_stack for the case we are entering.  */

  thiscase->desc = CASE_NESTING;
  thiscase->next = case_stack;
  thiscase->all = nesting_stack;
  thiscase->depth = ++nesting_depth;
  thiscase->exit_label = 0;
  thiscase->data.case_stmt.case_list = 0;
  thiscase->data.case_stmt.index_expr = index_expr;
  thiscase->data.case_stmt.default_label = 0;
  case_stack = thiscase;
  nesting_stack = thiscase;

  do_pending_stack_adjust ();

  /* Make sure case_stmt.start points to something that won't
     need any transformation before expand_end_case.  */
  if (!NOTE_P (get_last_insn ()))
    emit_note (NOTE_INSN_DELETED);

  thiscase->data.case_stmt.start = get_last_insn ();
}

/* Do the insertion of a case label into
   case_stack->data.case_stmt.case_list.  The labels are fed to us
   in descending order from the sorted vector of case labels used
   in the tree part of the middle end.  So the list we construct is
   sorted in ascending order.  */

void
add_case_node (tree low, tree high, tree label)
{
  struct case_node *r;

  /* If there's no HIGH value, then this is not a case range; it's
     just a simple case label.  But that's just a degenerate case
     range.
     If the bounds are equal, turn this into the one-value case.  */
  if (!high || tree_int_cst_equal (low, high))
    high = low;

  /* Handle default labels specially.  */
  if (!high && !low)
    {
#ifdef ENABLE_CHECKING
      if (case_stack->data.case_stmt.default_label != 0)
	abort ();
#endif
      case_stack->data.case_stmt.default_label = label;
      return;
    }

  /* Add this label to the chain.  */
  r = ggc_alloc (sizeof (struct case_node));
  r->low = low;
  r->high = high;
  r->code_label = label;
  r->parent = r->left = NULL;
  r->right = case_stack->data.case_stmt.case_list;
  case_stack->data.case_stmt.case_list = r;
}

/* Maximum number of case bit tests.  */
#define MAX_CASE_BIT_TESTS  3

/* By default, enable case bit tests on targets with ashlsi3.  */
#ifndef CASE_USE_BIT_TESTS
#define CASE_USE_BIT_TESTS  (ashl_optab->handlers[word_mode].insn_code \
			     != CODE_FOR_nothing)
#endif


/* A case_bit_test represents a set of case nodes that may be
   selected from using a bit-wise comparison.  HI and LO hold
   the integer to be tested against, LABEL contains the label
   to jump to upon success and BITS counts the number of case
   nodes handled by this test, typically the number of bits
   set in HI:LO.  */

struct case_bit_test
{
  HOST_WIDE_INT hi;
  HOST_WIDE_INT lo;
  rtx label;
  int bits;
};

/* Determine whether "1 << x" is relatively cheap in word_mode.  */

static
bool lshift_cheap_p (void)
{
  static bool init = false;
  static bool cheap = true;

  if (!init)
    {
      rtx reg = gen_rtx_REG (word_mode, 10000);
      int cost = rtx_cost (gen_rtx_ASHIFT (word_mode, const1_rtx, reg), SET);
      cheap = cost < COSTS_N_INSNS (3);
      init = true;
    }

  return cheap;
}

/* Comparison function for qsort to order bit tests by decreasing
   number of case nodes, i.e. the node with the most cases gets
   tested first.  */

static int
case_bit_test_cmp (const void *p1, const void *p2)
{
  const struct case_bit_test *d1 = p1;
  const struct case_bit_test *d2 = p2;

  return d2->bits - d1->bits;
}

/*  Expand a switch statement by a short sequence of bit-wise
    comparisons.  "switch(x)" is effectively converted into
    "if ((1 << (x-MINVAL)) & CST)" where CST and MINVAL are
    integer constants.

    INDEX_EXPR is the value being switched on, which is of
    type INDEX_TYPE.  MINVAL is the lowest case value of in
    the case nodes, of INDEX_TYPE type, and RANGE is highest
    value minus MINVAL, also of type INDEX_TYPE.  NODES is
    the set of case nodes, and DEFAULT_LABEL is the label to
    branch to should none of the cases match.

    There *MUST* be MAX_CASE_BIT_TESTS or less unique case
    node targets.  */

static void
emit_case_bit_tests (tree index_type, tree index_expr, tree minval,
		     tree range, case_node_ptr nodes, rtx default_label)
{
  struct case_bit_test test[MAX_CASE_BIT_TESTS];
  enum machine_mode mode;
  rtx expr, index, label;
  unsigned int i,j,lo,hi;
  struct case_node *n;
  unsigned int count;

  count = 0;
  for (n = nodes; n; n = n->right)
    {
      label = label_rtx (n->code_label);
      for (i = 0; i < count; i++)
	if (same_case_target_p (label, test[i].label))
	  break;

      if (i == count)
	{
	  if (count >= MAX_CASE_BIT_TESTS)
	    abort ();
          test[i].hi = 0;
          test[i].lo = 0;
	  test[i].label = label;
	  test[i].bits = 1;
	  count++;
	}
      else
        test[i].bits++;

      lo = tree_low_cst (fold (build2 (MINUS_EXPR, index_type,
				       n->low, minval)), 1);
      hi = tree_low_cst (fold (build2 (MINUS_EXPR, index_type,
				       n->high, minval)), 1);
      for (j = lo; j <= hi; j++)
        if (j >= HOST_BITS_PER_WIDE_INT)
	  test[i].hi |= (HOST_WIDE_INT) 1 << (j - HOST_BITS_PER_INT);
	else
	  test[i].lo |= (HOST_WIDE_INT) 1 << j;
    }

  qsort (test, count, sizeof(*test), case_bit_test_cmp);

  index_expr = fold (build2 (MINUS_EXPR, index_type,
			     convert (index_type, index_expr),
			     convert (index_type, minval)));
  index = expand_expr (index_expr, NULL_RTX, VOIDmode, 0);
  do_pending_stack_adjust ();

  mode = TYPE_MODE (index_type);
  expr = expand_expr (range, NULL_RTX, VOIDmode, 0);
  emit_cmp_and_jump_insns (index, expr, GTU, NULL_RTX, mode, 1,
			   default_label);

  index = convert_to_mode (word_mode, index, 0);
  index = expand_binop (word_mode, ashl_optab, const1_rtx,
			index, NULL_RTX, 1, OPTAB_WIDEN);

  for (i = 0; i < count; i++)
    {
      expr = immed_double_const (test[i].lo, test[i].hi, word_mode);
      expr = expand_binop (word_mode, and_optab, index, expr,
			   NULL_RTX, 1, OPTAB_WIDEN);
      emit_cmp_and_jump_insns (expr, const0_rtx, NE, NULL_RTX,
			       word_mode, 1, test[i].label);
    }

  emit_jump (default_label);
}

#ifndef HAVE_casesi
#define HAVE_casesi 0
#endif

#ifndef HAVE_tablejump
#define HAVE_tablejump 0
#endif

/* Terminate a case (Pascal) or switch (C) statement
   in which ORIG_INDEX is the expression to be tested.
   If ORIG_TYPE is not NULL, it is the original ORIG_INDEX
   type as given in the source before any compiler conversions.
   Generate the code to test it and jump to the right place.  */

void
expand_end_case_type (tree orig_index, tree orig_type)
{
  tree minval = NULL_TREE, maxval = NULL_TREE, range = NULL_TREE;
  rtx default_label = 0;
  struct case_node *n, *m;
  unsigned int count, uniq;
  rtx index;
  rtx table_label;
  int ncases;
  rtx *labelvec;
  int i;
  rtx before_case, end, lab;
  struct nesting *thiscase = case_stack;
  tree index_expr, index_type;
  bool exit_done = false;
  int unsignedp;

  /* Don't crash due to previous errors.  */
  if (thiscase == NULL)
    return;

  index_expr = thiscase->data.case_stmt.index_expr;
  index_type = TREE_TYPE (index_expr);
  unsignedp = TYPE_UNSIGNED (index_type);
  if (orig_type == NULL)
    orig_type = TREE_TYPE (orig_index);

  do_pending_stack_adjust ();

  /* An ERROR_MARK occurs for various reasons including invalid data type.  */
  if (index_type != error_mark_node)
    {
      /* If we don't have a default-label, create one here,
	 after the body of the switch.  */
      if (thiscase->data.case_stmt.default_label == 0)
	{
	  thiscase->data.case_stmt.default_label
	    = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);
	  /* Share the exit label if possible.  */
          if (thiscase->exit_label)
	    {
	      SET_DECL_RTL (thiscase->data.case_stmt.default_label,
			    thiscase->exit_label);
	      exit_done = true;
	    }
	  expand_label (thiscase->data.case_stmt.default_label);
	}
      default_label = label_rtx (thiscase->data.case_stmt.default_label);

      before_case = get_last_insn ();

      /* Get upper and lower bounds of case values.
	 Also convert all the case values to the index expr's data type.  */

      uniq = 0;
      count = 0;
      for (n = thiscase->data.case_stmt.case_list; n; n = n->right)
	{
	  /* Check low and high label values are integers.  */
	  if (TREE_CODE (n->low) != INTEGER_CST)
	    abort ();
	  if (TREE_CODE (n->high) != INTEGER_CST)
	    abort ();

	  n->low = convert (index_type, n->low);
	  n->high = convert (index_type, n->high);

	  /* Count the elements and track the largest and smallest
	     of them (treating them as signed even if they are not).  */
	  if (count++ == 0)
	    {
	      minval = n->low;
	      maxval = n->high;
	    }
	  else
	    {
	      if (INT_CST_LT (n->low, minval))
		minval = n->low;
	      if (INT_CST_LT (maxval, n->high))
		maxval = n->high;
	    }
	  /* A range counts double, since it requires two compares.  */
	  if (! tree_int_cst_equal (n->low, n->high))
	    count++;

	  /* Count the number of unique case node targets.  */
          uniq++;
	  lab = label_rtx (n->code_label);
          for (m = thiscase->data.case_stmt.case_list; m != n; m = m->right)
            if (same_case_target_p (label_rtx (m->code_label), lab))
              {
                uniq--;
                break;
              }
	}

      /* Compute span of values.  */
      if (count != 0)
	range = fold (build2 (MINUS_EXPR, index_type, maxval, minval));

      if (count == 0)
	{
	  expand_expr (index_expr, const0_rtx, VOIDmode, 0);
	  emit_jump (default_label);
	}

      /* Try implementing this switch statement by a short sequence of
	 bit-wise comparisons.  However, we let the binary-tree case
	 below handle constant index expressions.  */
      else if (CASE_USE_BIT_TESTS
	       && ! TREE_CONSTANT (index_expr)
	       && compare_tree_int (range, GET_MODE_BITSIZE (word_mode)) < 0
	       && compare_tree_int (range, 0) > 0
	       && lshift_cheap_p ()
	       && ((uniq == 1 && count >= 3)
		   || (uniq == 2 && count >= 5)
		   || (uniq == 3 && count >= 6)))
	{
	  /* Optimize the case where all the case values fit in a
	     word without having to subtract MINVAL.  In this case,
	     we can optimize away the subtraction.  */
	  if (compare_tree_int (minval, 0) > 0
	      && compare_tree_int (maxval, GET_MODE_BITSIZE (word_mode)) < 0)
	    {
	      minval = integer_zero_node;
	      range = maxval;
	    }
	  emit_case_bit_tests (index_type, index_expr, minval, range,
			       thiscase->data.case_stmt.case_list,
			       default_label);
	}

      /* If range of values is much bigger than number of values,
	 make a sequence of conditional branches instead of a dispatch.
	 If the switch-index is a constant, do it this way
	 because we can optimize it.  */

      else if (count < case_values_threshold ()
	       || compare_tree_int (range,
				    (optimize_size ? 3 : 10) * count) > 0
	       /* RANGE may be signed, and really large ranges will show up
		  as negative numbers.  */
	       || compare_tree_int (range, 0) < 0
#ifndef ASM_OUTPUT_ADDR_DIFF_ELT
	       || flag_pic
#endif
	       || TREE_CONSTANT (index_expr)
	       /* If neither casesi or tablejump is available, we can
		  only go this way.  */
	       || (!HAVE_casesi && !HAVE_tablejump))
	{
	  index = expand_expr (index_expr, NULL_RTX, VOIDmode, 0);

	  /* If the index is a short or char that we do not have
	     an insn to handle comparisons directly, convert it to
	     a full integer now, rather than letting each comparison
	     generate the conversion.  */

	  if (GET_MODE_CLASS (GET_MODE (index)) == MODE_INT
	      && ! have_insn_for (COMPARE, GET_MODE (index)))
	    {
	      enum machine_mode wider_mode;
	      for (wider_mode = GET_MODE (index); wider_mode != VOIDmode;
		   wider_mode = GET_MODE_WIDER_MODE (wider_mode))
		if (have_insn_for (COMPARE, wider_mode))
		  {
		    index = convert_to_mode (wider_mode, index, unsignedp);
		    break;
		  }
	    }

	  do_pending_stack_adjust ();

	  if (MEM_P (index))
	    index = copy_to_reg (index);
	  if (GET_CODE (index) == CONST_INT
	      || TREE_CODE (index_expr) == INTEGER_CST)
	    {
	      /* Make a tree node with the proper constant value
		 if we don't already have one.  */
	      if (TREE_CODE (index_expr) != INTEGER_CST)
		{
		  index_expr
		    = build_int_2 (INTVAL (index),
				   unsignedp || INTVAL (index) >= 0 ? 0 : -1);
		  index_expr = convert (index_type, index_expr);
		}

	      /* For constant index expressions we need only
		 issue an unconditional branch to the appropriate
		 target code.  The job of removing any unreachable
		 code is left to the optimization phase if the
		 "-O" option is specified.  */
	      for (n = thiscase->data.case_stmt.case_list; n; n = n->right)
		if (! tree_int_cst_lt (index_expr, n->low)
		    && ! tree_int_cst_lt (n->high, index_expr))
		  break;

	      if (n)
		emit_jump (label_rtx (n->code_label));
	      else
		emit_jump (default_label);
	    }
	  else
	    {
	      /* If the index expression is not constant we generate
		 a binary decision tree to select the appropriate
		 target code.  This is done as follows:

		 The list of cases is rearranged into a binary tree,
		 nearly optimal assuming equal probability for each case.

		 The tree is transformed into RTL, eliminating
		 redundant test conditions at the same time.

		 If program flow could reach the end of the
		 decision tree an unconditional jump to the
		 default code is emitted.  */

	      use_cost_table
		= (TREE_CODE (orig_type) != ENUMERAL_TYPE
		   && estimate_case_costs (thiscase->data.case_stmt.case_list));
	      balance_case_nodes (&thiscase->data.case_stmt.case_list, NULL);
	      emit_case_nodes (index, thiscase->data.case_stmt.case_list,
			       default_label, index_type);
	      emit_jump (default_label);
	    }
	}
      else
	{
	  table_label = gen_label_rtx ();
	  if (! try_casesi (index_type, index_expr, minval, range,
			    table_label, default_label))
	    {
	      index_type = integer_type_node;

	      /* Index jumptables from zero for suitable values of
                 minval to avoid a subtraction.  */
	      if (! optimize_size
		  && compare_tree_int (minval, 0) > 0
		  && compare_tree_int (minval, 3) < 0)
		{
		  minval = integer_zero_node;
		  range = maxval;
		}

	      if (! try_tablejump (index_type, index_expr, minval, range,
				   table_label, default_label))
		abort ();
	    }

	  /* Get table of labels to jump to, in order of case index.  */

	  ncases = tree_low_cst (range, 0) + 1;
	  labelvec = alloca (ncases * sizeof (rtx));
	  memset (labelvec, 0, ncases * sizeof (rtx));

	  for (n = thiscase->data.case_stmt.case_list; n; n = n->right)
	    {
	      /* Compute the low and high bounds relative to the minimum
		 value since that should fit in a HOST_WIDE_INT while the
		 actual values may not.  */
	      HOST_WIDE_INT i_low
		= tree_low_cst (fold (build2 (MINUS_EXPR, index_type,
					      n->low, minval)), 1);
	      HOST_WIDE_INT i_high
		= tree_low_cst (fold (build2 (MINUS_EXPR, index_type,
					      n->high, minval)), 1);
	      HOST_WIDE_INT i;

	      for (i = i_low; i <= i_high; i ++)
		labelvec[i]
		  = gen_rtx_LABEL_REF (Pmode, label_rtx (n->code_label));
	    }

	  /* Fill in the gaps with the default.  */
	  for (i = 0; i < ncases; i++)
	    if (labelvec[i] == 0)
	      labelvec[i] = gen_rtx_LABEL_REF (Pmode, default_label);

	  /* Output the table.  */
	  emit_label (table_label);

	  if (CASE_VECTOR_PC_RELATIVE || flag_pic)
	    emit_jump_insn (gen_rtx_ADDR_DIFF_VEC (CASE_VECTOR_MODE,
						   gen_rtx_LABEL_REF (Pmode, table_label),
						   gen_rtvec_v (ncases, labelvec),
						   const0_rtx, const0_rtx));
	  else
	    emit_jump_insn (gen_rtx_ADDR_VEC (CASE_VECTOR_MODE,
					      gen_rtvec_v (ncases, labelvec)));

	  /* If the case insn drops through the table,
	     after the table we must jump to the default-label.
	     Otherwise record no drop-through after the table.  */
#ifdef CASE_DROPS_THROUGH
	  emit_jump (default_label);
#else
	  emit_barrier ();
#endif
	}

      before_case = NEXT_INSN (before_case);
      end = get_last_insn ();
      if (squeeze_notes (&before_case, &end))
	abort ();
      reorder_insns (before_case, end,
		     thiscase->data.case_stmt.start);
    }

  if (thiscase->exit_label && !exit_done)
    emit_label (thiscase->exit_label);

  POPSTACK (case_stack);

  free_temp_slots ();
}

/* Generate code to jump to LABEL if OP1 and OP2 are equal.  */

static void
do_jump_if_equal (rtx op1, rtx op2, rtx label, int unsignedp)
{
  if (GET_CODE (op1) == CONST_INT && GET_CODE (op2) == CONST_INT)
    {
      if (op1 == op2)
	emit_jump (label);
    }
  else
    emit_cmp_and_jump_insns (op1, op2, EQ, NULL_RTX,
			     (GET_MODE (op1) == VOIDmode
			     ? GET_MODE (op2) : GET_MODE (op1)),
			     unsignedp, label);
}

/* Not all case values are encountered equally.  This function
   uses a heuristic to weight case labels, in cases where that
   looks like a reasonable thing to do.

   Right now, all we try to guess is text, and we establish the
   following weights:

	chars above space:	16
	digits:			16
	default:		12
	space, punct:		8
	tab:			4
	newline:		2
	other "\" chars:	1
	remaining chars:	0

   If we find any cases in the switch that are not either -1 or in the range
   of valid ASCII characters, or are control characters other than those
   commonly used with "\", don't treat this switch scanning text.

   Return 1 if these nodes are suitable for cost estimation, otherwise
   return 0.  */

static int
estimate_case_costs (case_node_ptr node)
{
  tree min_ascii = integer_minus_one_node;
  tree max_ascii = convert (TREE_TYPE (node->high), build_int_2 (127, 0));
  case_node_ptr n;
  int i;

  /* If we haven't already made the cost table, make it now.  Note that the
     lower bound of the table is -1, not zero.  */

  if (! cost_table_initialized)
    {
      cost_table_initialized = 1;

      for (i = 0; i < 128; i++)
	{
	  if (ISALNUM (i))
	    COST_TABLE (i) = 16;
	  else if (ISPUNCT (i))
	    COST_TABLE (i) = 8;
	  else if (ISCNTRL (i))
	    COST_TABLE (i) = -1;
	}

      COST_TABLE (' ') = 8;
      COST_TABLE ('\t') = 4;
      COST_TABLE ('\0') = 4;
      COST_TABLE ('\n') = 2;
      COST_TABLE ('\f') = 1;
      COST_TABLE ('\v') = 1;
      COST_TABLE ('\b') = 1;
    }

  /* See if all the case expressions look like text.  It is text if the
     constant is >= -1 and the highest constant is <= 127.  Do all comparisons
     as signed arithmetic since we don't want to ever access cost_table with a
     value less than -1.  Also check that none of the constants in a range
     are strange control characters.  */

  for (n = node; n; n = n->right)
    {
      if ((INT_CST_LT (n->low, min_ascii)) || INT_CST_LT (max_ascii, n->high))
	return 0;

      for (i = (HOST_WIDE_INT) TREE_INT_CST_LOW (n->low);
	   i <= (HOST_WIDE_INT) TREE_INT_CST_LOW (n->high); i++)
	if (COST_TABLE (i) < 0)
	  return 0;
    }

  /* All interesting values are within the range of interesting
     ASCII characters.  */
  return 1;
}

/* Determine whether two case labels branch to the same target.
   Since we now do tree optimizations, just comparing labels is
   good enough.  */

static bool
same_case_target_p (rtx l1, rtx l2)
{
  return l1 == l2;
}

/* Take an ordered list of case nodes
   and transform them into a near optimal binary tree,
   on the assumption that any target code selection value is as
   likely as any other.

   The transformation is performed by splitting the ordered
   list into two equal sections plus a pivot.  The parts are
   then attached to the pivot as left and right branches.  Each
   branch is then transformed recursively.  */

static void
balance_case_nodes (case_node_ptr *head, case_node_ptr parent)
{
  case_node_ptr np;

  np = *head;
  if (np)
    {
      int cost = 0;
      int i = 0;
      int ranges = 0;
      case_node_ptr *npp;
      case_node_ptr left;

      /* Count the number of entries on branch.  Also count the ranges.  */

      while (np)
	{
	  if (!tree_int_cst_equal (np->low, np->high))
	    {
	      ranges++;
	      if (use_cost_table)
		cost += COST_TABLE (TREE_INT_CST_LOW (np->high));
	    }

	  if (use_cost_table)
	    cost += COST_TABLE (TREE_INT_CST_LOW (np->low));

	  i++;
	  np = np->right;
	}

      if (i > 2)
	{
	  /* Split this list if it is long enough for that to help.  */
	  npp = head;
	  left = *npp;
	  if (use_cost_table)
	    {
	      /* Find the place in the list that bisects the list's total cost,
		 Here I gets half the total cost.  */
	      int n_moved = 0;
	      i = (cost + 1) / 2;
	      while (1)
		{
		  /* Skip nodes while their cost does not reach that amount.  */
		  if (!tree_int_cst_equal ((*npp)->low, (*npp)->high))
		    i -= COST_TABLE (TREE_INT_CST_LOW ((*npp)->high));
		  i -= COST_TABLE (TREE_INT_CST_LOW ((*npp)->low));
		  if (i <= 0)
		    break;
		  npp = &(*npp)->right;
		  n_moved += 1;
		}
	      if (n_moved == 0)
		{
		  /* Leave this branch lopsided, but optimize left-hand
		     side and fill in `parent' fields for right-hand side.  */
		  np = *head;
		  np->parent = parent;
		  balance_case_nodes (&np->left, np);
		  for (; np->right; np = np->right)
		    np->right->parent = np;
		  return;
		}
	    }
	  /* If there are just three nodes, split at the middle one.  */
	  else if (i == 3)
	    npp = &(*npp)->right;
	  else
	    {
	      /* Find the place in the list that bisects the list's total cost,
		 where ranges count as 2.
		 Here I gets half the total cost.  */
	      i = (i + ranges + 1) / 2;
	      while (1)
		{
		  /* Skip nodes while their cost does not reach that amount.  */
		  if (!tree_int_cst_equal ((*npp)->low, (*npp)->high))
		    i--;
		  i--;
		  if (i <= 0)
		    break;
		  npp = &(*npp)->right;
		}
	    }
	  *head = np = *npp;
	  *npp = 0;
	  np->parent = parent;
	  np->left = left;

	  /* Optimize each of the two split parts.  */
	  balance_case_nodes (&np->left, np);
	  balance_case_nodes (&np->right, np);
	}
      else
	{
	  /* Else leave this branch as one level,
	     but fill in `parent' fields.  */
	  np = *head;
	  np->parent = parent;
	  for (; np->right; np = np->right)
	    np->right->parent = np;
	}
    }
}

/* Search the parent sections of the case node tree
   to see if a test for the lower bound of NODE would be redundant.
   INDEX_TYPE is the type of the index expression.

   The instructions to generate the case decision tree are
   output in the same order as nodes are processed so it is
   known that if a parent node checks the range of the current
   node minus one that the current node is bounded at its lower
   span.  Thus the test would be redundant.  */

static int
node_has_low_bound (case_node_ptr node, tree index_type)
{
  tree low_minus_one;
  case_node_ptr pnode;

  /* If the lower bound of this node is the lowest value in the index type,
     we need not test it.  */

  if (tree_int_cst_equal (node->low, TYPE_MIN_VALUE (index_type)))
    return 1;

  /* If this node has a left branch, the value at the left must be less
     than that at this node, so it cannot be bounded at the bottom and
     we need not bother testing any further.  */

  if (node->left)
    return 0;

  low_minus_one = fold (build2 (MINUS_EXPR, TREE_TYPE (node->low),
				node->low, integer_one_node));

  /* If the subtraction above overflowed, we can't verify anything.
     Otherwise, look for a parent that tests our value - 1.  */

  if (! tree_int_cst_lt (low_minus_one, node->low))
    return 0;

  for (pnode = node->parent; pnode; pnode = pnode->parent)
    if (tree_int_cst_equal (low_minus_one, pnode->high))
      return 1;

  return 0;
}

/* Search the parent sections of the case node tree
   to see if a test for the upper bound of NODE would be redundant.
   INDEX_TYPE is the type of the index expression.

   The instructions to generate the case decision tree are
   output in the same order as nodes are processed so it is
   known that if a parent node checks the range of the current
   node plus one that the current node is bounded at its upper
   span.  Thus the test would be redundant.  */

static int
node_has_high_bound (case_node_ptr node, tree index_type)
{
  tree high_plus_one;
  case_node_ptr pnode;

  /* If there is no upper bound, obviously no test is needed.  */

  if (TYPE_MAX_VALUE (index_type) == NULL)
    return 1;

  /* If the upper bound of this node is the highest value in the type
     of the index expression, we need not test against it.  */

  if (tree_int_cst_equal (node->high, TYPE_MAX_VALUE (index_type)))
    return 1;

  /* If this node has a right branch, the value at the right must be greater
     than that at this node, so it cannot be bounded at the top and
     we need not bother testing any further.  */

  if (node->right)
    return 0;

  high_plus_one = fold (build2 (PLUS_EXPR, TREE_TYPE (node->high),
				node->high, integer_one_node));

  /* If the addition above overflowed, we can't verify anything.
     Otherwise, look for a parent that tests our value + 1.  */

  if (! tree_int_cst_lt (node->high, high_plus_one))
    return 0;

  for (pnode = node->parent; pnode; pnode = pnode->parent)
    if (tree_int_cst_equal (high_plus_one, pnode->low))
      return 1;

  return 0;
}

/* Search the parent sections of the
   case node tree to see if both tests for the upper and lower
   bounds of NODE would be redundant.  */

static int
node_is_bounded (case_node_ptr node, tree index_type)
{
  return (node_has_low_bound (node, index_type)
	  && node_has_high_bound (node, index_type));
}

/* Emit step-by-step code to select a case for the value of INDEX.
   The thus generated decision tree follows the form of the
   case-node binary tree NODE, whose nodes represent test conditions.
   INDEX_TYPE is the type of the index of the switch.

   Care is taken to prune redundant tests from the decision tree
   by detecting any boundary conditions already checked by
   emitted rtx.  (See node_has_high_bound, node_has_low_bound
   and node_is_bounded, above.)

   Where the test conditions can be shown to be redundant we emit
   an unconditional jump to the target code.  As a further
   optimization, the subordinates of a tree node are examined to
   check for bounded nodes.  In this case conditional and/or
   unconditional jumps as a result of the boundary check for the
   current node are arranged to target the subordinates associated
   code for out of bound conditions on the current node.

   We can assume that when control reaches the code generated here,
   the index value has already been compared with the parents
   of this node, and determined to be on the same side of each parent
   as this node is.  Thus, if this node tests for the value 51,
   and a parent tested for 52, we don't need to consider
   the possibility of a value greater than 51.  If another parent
   tests for the value 50, then this node need not test anything.  */

static void
emit_case_nodes (rtx index, case_node_ptr node, rtx default_label,
		 tree index_type)
{
  /* If INDEX has an unsigned type, we must make unsigned branches.  */
  int unsignedp = TYPE_UNSIGNED (index_type);
  enum machine_mode mode = GET_MODE (index);
  enum machine_mode imode = TYPE_MODE (index_type);

  /* See if our parents have already tested everything for us.
     If they have, emit an unconditional jump for this node.  */
  if (node_is_bounded (node, index_type))
    emit_jump (label_rtx (node->code_label));

  else if (tree_int_cst_equal (node->low, node->high))
    {
      /* Node is single valued.  First see if the index expression matches
	 this node and then check our children, if any.  */

      do_jump_if_equal (index,
			convert_modes (mode, imode,
				       expand_expr (node->low, NULL_RTX,
						    VOIDmode, 0),
				       unsignedp),
			label_rtx (node->code_label), unsignedp);

      if (node->right != 0 && node->left != 0)
	{
	  /* This node has children on both sides.
	     Dispatch to one side or the other
	     by comparing the index value with this node's value.
	     If one subtree is bounded, check that one first,
	     so we can avoid real branches in the tree.  */

	  if (node_is_bounded (node->right, index_type))
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       GT, NULL_RTX, mode, unsignedp,
				       label_rtx (node->right->code_label));
	      emit_case_nodes (index, node->left, default_label, index_type);
	    }

	  else if (node_is_bounded (node->left, index_type))
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       LT, NULL_RTX, mode, unsignedp,
				       label_rtx (node->left->code_label));
	      emit_case_nodes (index, node->right, default_label, index_type);
	    }

	  /* If both children are single-valued cases with no
	     children, finish up all the work.  This way, we can save
	     one ordered comparison.  */
	  else if (tree_int_cst_equal (node->right->low, node->right->high)
		   && node->right->left == 0
		   && node->right->right == 0
		   && tree_int_cst_equal (node->left->low, node->left->high)
		   && node->left->left == 0
		   && node->left->right == 0)
	    {
	      /* Neither node is bounded.  First distinguish the two sides;
		 then emit the code for one side at a time.  */

	      /* See if the value matches what the right hand side
		 wants.  */
	      do_jump_if_equal (index,
				convert_modes (mode, imode,
					       expand_expr (node->right->low,
							    NULL_RTX,
							    VOIDmode, 0),
					       unsignedp),
				label_rtx (node->right->code_label),
				unsignedp);

	      /* See if the value matches what the left hand side
		 wants.  */
	      do_jump_if_equal (index,
				convert_modes (mode, imode,
					       expand_expr (node->left->low,
							    NULL_RTX,
							    VOIDmode, 0),
					       unsignedp),
				label_rtx (node->left->code_label),
				unsignedp);
	    }

	  else
	    {
	      /* Neither node is bounded.  First distinguish the two sides;
		 then emit the code for one side at a time.  */

	      tree test_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);

	      /* See if the value is on the right.  */
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       GT, NULL_RTX, mode, unsignedp,
				       label_rtx (test_label));

	      /* Value must be on the left.
		 Handle the left-hand subtree.  */
	      emit_case_nodes (index, node->left, default_label, index_type);
	      /* If left-hand subtree does nothing,
		 go to default.  */
	      emit_jump (default_label);

	      /* Code branches here for the right-hand subtree.  */
	      expand_label (test_label);
	      emit_case_nodes (index, node->right, default_label, index_type);
	    }
	}

      else if (node->right != 0 && node->left == 0)
	{
	  /* Here we have a right child but no left so we issue conditional
	     branch to default and process the right child.

	     Omit the conditional branch to default if we it avoid only one
	     right child; it costs too much space to save so little time.  */

	  if (node->right->right || node->right->left
	      || !tree_int_cst_equal (node->right->low, node->right->high))
	    {
	      if (!node_has_low_bound (node, index_type))
		{
		  emit_cmp_and_jump_insns (index,
					   convert_modes
					   (mode, imode,
					    expand_expr (node->high, NULL_RTX,
							 VOIDmode, 0),
					    unsignedp),
					   LT, NULL_RTX, mode, unsignedp,
					   default_label);
		}

	      emit_case_nodes (index, node->right, default_label, index_type);
	    }
	  else
	    /* We cannot process node->right normally
	       since we haven't ruled out the numbers less than
	       this node's value.  So handle node->right explicitly.  */
	    do_jump_if_equal (index,
			      convert_modes
			      (mode, imode,
			       expand_expr (node->right->low, NULL_RTX,
					    VOIDmode, 0),
			       unsignedp),
			      label_rtx (node->right->code_label), unsignedp);
	}

      else if (node->right == 0 && node->left != 0)
	{
	  /* Just one subtree, on the left.  */
	  if (node->left->left || node->left->right
	      || !tree_int_cst_equal (node->left->low, node->left->high))
	    {
	      if (!node_has_high_bound (node, index_type))
		{
		  emit_cmp_and_jump_insns (index,
					   convert_modes
					   (mode, imode,
					    expand_expr (node->high, NULL_RTX,
							 VOIDmode, 0),
					    unsignedp),
					   GT, NULL_RTX, mode, unsignedp,
					   default_label);
		}

	      emit_case_nodes (index, node->left, default_label, index_type);
	    }
	  else
	    /* We cannot process node->left normally
	       since we haven't ruled out the numbers less than
	       this node's value.  So handle node->left explicitly.  */
	    do_jump_if_equal (index,
			      convert_modes
			      (mode, imode,
			       expand_expr (node->left->low, NULL_RTX,
					    VOIDmode, 0),
			       unsignedp),
			      label_rtx (node->left->code_label), unsignedp);
	}
    }
  else
    {
      /* Node is a range.  These cases are very similar to those for a single
	 value, except that we do not start by testing whether this node
	 is the one to branch to.  */

      if (node->right != 0 && node->left != 0)
	{
	  /* Node has subtrees on both sides.
	     If the right-hand subtree is bounded,
	     test for it first, since we can go straight there.
	     Otherwise, we need to make a branch in the control structure,
	     then handle the two subtrees.  */
	  tree test_label = 0;

	  if (node_is_bounded (node->right, index_type))
	    /* Right hand node is fully bounded so we can eliminate any
	       testing and branch directly to the target code.  */
	    emit_cmp_and_jump_insns (index,
				     convert_modes
				     (mode, imode,
				      expand_expr (node->high, NULL_RTX,
						   VOIDmode, 0),
				      unsignedp),
				     GT, NULL_RTX, mode, unsignedp,
				     label_rtx (node->right->code_label));
	  else
	    {
	      /* Right hand node requires testing.
		 Branch to a label where we will handle it later.  */

	      test_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       GT, NULL_RTX, mode, unsignedp,
				       label_rtx (test_label));
	    }

	  /* Value belongs to this node or to the left-hand subtree.  */

	  emit_cmp_and_jump_insns (index,
				   convert_modes
				   (mode, imode,
				    expand_expr (node->low, NULL_RTX,
						 VOIDmode, 0),
				    unsignedp),
				   GE, NULL_RTX, mode, unsignedp,
				   label_rtx (node->code_label));

	  /* Handle the left-hand subtree.  */
	  emit_case_nodes (index, node->left, default_label, index_type);

	  /* If right node had to be handled later, do that now.  */

	  if (test_label)
	    {
	      /* If the left-hand subtree fell through,
		 don't let it fall into the right-hand subtree.  */
	      emit_jump (default_label);

	      expand_label (test_label);
	      emit_case_nodes (index, node->right, default_label, index_type);
	    }
	}

      else if (node->right != 0 && node->left == 0)
	{
	  /* Deal with values to the left of this node,
	     if they are possible.  */
	  if (!node_has_low_bound (node, index_type))
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->low, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       LT, NULL_RTX, mode, unsignedp,
				       default_label);
	    }

	  /* Value belongs to this node or to the right-hand subtree.  */

	  emit_cmp_and_jump_insns (index,
				   convert_modes
				   (mode, imode,
				    expand_expr (node->high, NULL_RTX,
						 VOIDmode, 0),
				    unsignedp),
				   LE, NULL_RTX, mode, unsignedp,
				   label_rtx (node->code_label));

	  emit_case_nodes (index, node->right, default_label, index_type);
	}

      else if (node->right == 0 && node->left != 0)
	{
	  /* Deal with values to the right of this node,
	     if they are possible.  */
	  if (!node_has_high_bound (node, index_type))
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       GT, NULL_RTX, mode, unsignedp,
				       default_label);
	    }

	  /* Value belongs to this node or to the left-hand subtree.  */

	  emit_cmp_and_jump_insns (index,
				   convert_modes
				   (mode, imode,
				    expand_expr (node->low, NULL_RTX,
						 VOIDmode, 0),
				    unsignedp),
				   GE, NULL_RTX, mode, unsignedp,
				   label_rtx (node->code_label));

	  emit_case_nodes (index, node->left, default_label, index_type);
	}

      else
	{
	  /* Node has no children so we check low and high bounds to remove
	     redundant tests.  Only one of the bounds can exist,
	     since otherwise this node is bounded--a case tested already.  */
	  int high_bound = node_has_high_bound (node, index_type);
	  int low_bound = node_has_low_bound (node, index_type);

	  if (!high_bound && low_bound)
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->high, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       GT, NULL_RTX, mode, unsignedp,
				       default_label);
	    }

	  else if (!low_bound && high_bound)
	    {
	      emit_cmp_and_jump_insns (index,
				       convert_modes
				       (mode, imode,
					expand_expr (node->low, NULL_RTX,
						     VOIDmode, 0),
					unsignedp),
				       LT, NULL_RTX, mode, unsignedp,
				       default_label);
	    }
	  else if (!low_bound && !high_bound)
	    {
	      /* Widen LOW and HIGH to the same width as INDEX.  */
	      tree type = lang_hooks.types.type_for_mode (mode, unsignedp);
	      tree low = build1 (CONVERT_EXPR, type, node->low);
	      tree high = build1 (CONVERT_EXPR, type, node->high);
	      rtx low_rtx, new_index, new_bound;

	      /* Instead of doing two branches, emit one unsigned branch for
		 (index-low) > (high-low).  */
	      low_rtx = expand_expr (low, NULL_RTX, mode, 0);
	      new_index = expand_simple_binop (mode, MINUS, index, low_rtx,
					       NULL_RTX, unsignedp,
					       OPTAB_WIDEN);
	      new_bound = expand_expr (fold (build2 (MINUS_EXPR, type,
						     high, low)),
				       NULL_RTX, mode, 0);

	      emit_cmp_and_jump_insns (new_index, new_bound, GT, NULL_RTX,
				       mode, 1, default_label);
	    }

	  emit_jump (label_rtx (node->code_label));
	}
    }
}

#include "gt-stmt.h"
