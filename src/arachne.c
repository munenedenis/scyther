/**
 *@file arachne.c
 *
 * Introduces a method for proofs akin to the Athena modelchecker
 * http://www.ece.cmu.edu/~dawnsong/athena/
 *
 */

#include "term.h"
#include "termlist.h"
#include "role.h"
#include "system.h"
#include "compiler.h"
#include "states.h"
#include "mgu.h"
#include "arachne.h"

static System sys;
Protocol INTRUDER;		// Pointers, to be set by the Init
Role I_GOAL;			// Same here.
Role I_TEE;
Role I_SPLIT;
Role I_TUPLE;
Role I_ENCRYPT;
Role I_DECRYPT;

static int indentDepth;

#ifdef DEBUG
static char *explanation;	// Pointer to a string that describes what we just tried to do
static int e_run;
static Term e_term1;
static Term e_term2;
static Term e_term3;
#endif

struct goalstruct
{
  int run;
  int index;
  Roledef rd;
};

typedef struct goalstruct Goal;

/**
 * Forward declarations
 */

int iterate ();

/**
 * Program code
 */

//! Init Arachne engine
void
arachneInit (const System mysys)
{
  Roledef rd = NULL;

  void add_event (int event, Term message)
  {
    rd = roledefAdd (rd, event, NULL, NULL, NULL, message, NULL);
  }
  Role add_role (const char *rolenamestring)
  {
    Role r;
    Term rolename;

    rolename = makeGlobalConstant (rolenamestring);
    r = roleCreate (rolename);
    r->roledef = rd;
    rd = NULL;
    r->next = INTRUDER->roles;
    INTRUDER->roles = r;
    // compute_role_variables (sys, INTRUDER, r);
    return r;
  }

  sys = mysys;			// make sys available for this module as a global
  /*
   * Add intruder protocol roles
   */

  INTRUDER = protocolCreate (makeGlobalConstant (" INTRUDER "));

  add_event (READ, NULL);
  I_GOAL = add_role (" I_GOAL ");

  return;
}

//! Close Arachne engine
void
arachneDone ()
{
  return;
}

//------------------------------------------------------------------------
// Detail
//------------------------------------------------------------------------

/*
 * runs[rid].step is now the number of 'valid' events within the run, but we
 * call it 'length' here.
 */
#define INVALID		-1
#define isGoal(rd)	(rd->type == READ && !rd->internal)
#define isBound(rd)	(rd->bind_run != INVALID)
#define length		step

//! Indent print
void
indentPrint ()
{
#ifdef DEBUG
  int i;

  for (i = 0; i < indentDepth; i++)
    eprintf ("|   ");
#else
  eprintf (">> ");
#endif
}

//! Iterate but discard the info of the termlist
int
mgu_iterate (const Termlist tl)
{
  return iterate ();
}

//------------------------------------------------------------------------
// Sub
//------------------------------------------------------------------------

//! Iterate over all send types in the roles (including the intruder ones)
/**
 * Function is called with (protocol pointer, role pointer, roledef pointer, index)
 * and returns an integer. If it is false, iteration aborts.
 */
int
iterate_role_sends (int (*func) ())
{
  Protocol p;

  p = sys->protocols;
  while (p != NULL)
    {
      Role r;

      r = p->roles;
      while (r != NULL)
	{
	  Roledef rd;
	  int index;

	  rd = r->roledef;
	  index = 0;
	  while (rd != NULL)
	    {
	      if (rd->type == SEND)
		{
		  if (!func (p, r, rd, index))
		    return 0;
		}
	      index++;
	      rd = rd->next;
	    }
	  r = r->next;
	}
      p = p->next;
    }
  return 1;
}

//! Generate a new intruder goal
int
create_intruder_goal (Term t)
{
  int run;
  Roledef rd;

  roleInstance (sys, INTRUDER, I_GOAL, NULL);
  run = sys->maxruns - 1;
  rd = sys->runs[run].start;
  sys->runs[run].length = 1;
  rd->message = termDuplicate (t);
#ifdef DEBUG
  explanation = "Adding intruder goal for message ";
  e_term1 = t;
#endif
  return run;
}

//! Generates a new intruder goal, iterates
/**
 * Sloppy, does not unify term but hardcodes it into the stuff.
 */
int
add_intruder_goal_iterate (Goal goal)
{
  int flag;
  int run;

  run = create_intruder_goal (goal.rd->message);
  goal.rd->bind_run = run;
  goal.rd->bind_index = 0;

  flag = iterate ();

  roleInstanceDestroy (sys);	// destroy the created run
  goal.rd->bind_run = INVALID;
  return flag;
}

//! Bind a goal to an existing regular run, if possible
int
bind_existing_run (const Goal goal, const Protocol p, const Role r,
		   const int index)
{
  int run, flag;

  flag = 1;
  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].protocol == p && sys->runs[run].role == r)
	{
	  int old_length;
	  Roledef rd;

	  // Roledef entry
	  rd = roledef_shift (sys->runs[run].start, index);

	  // mgu and iterate
	  old_length = sys->runs[run].length;
	  if (index >= old_length)
	    sys->runs[run].length = index + 1;
#ifdef DEBUG
	  explanation = "Bind existing run";
	  e_run = run;
	  e_term1 = goal.rd->message;
#endif
	  flag = (flag
		  && termMguInTerm (goal.rd->message, rd->message,
				    mgu_iterate));
	  sys->runs[run].length = old_length;
	}
    }
  return flag;
}

//! Bind a goal to a new run
int
bind_new_run (const Goal goal, const Protocol p, const Role r,
	      const int index)
{
  int run;
  int flag;
  Roledef rd;
  int old_run;
  int old_index;

  roleInstance (sys, p, r, NULL);
  run = sys->maxruns - 1;
  sys->runs[run].length = index + 1;
  old_run = goal.rd->bind_run;
  old_index = goal.rd->bind_index;
  goal.rd->bind_run = run;
  goal.rd->bind_index = index;
#ifdef DEBUG
  explanation = "Bind new run";
  e_run = run;
  e_term1 = r->nameterm;
  rd = roledef_shift (sys->runs[run].start, index);
  e_term2 = rd->message;
#endif

  iterate ();

  goal.rd->bind_run = old_run;
  goal.rd->bind_index = old_index;
  roleInstanceDestroy (sys);
  return flag;
}

//! Print the current semistate
void
printSemiState ()
{
  int run;

  for (run = 0; run < sys->maxruns; run++)
    {
      int index;
      Roledef rd;

      indentPrint ();
      eprintf ("[ Run %i, ", run);
      termPrint (sys->runs[run].role->nameterm);
      eprintf (" ]\n");

      index = 0;
      rd = sys->runs[run].start;
      while (index < sys->runs[run].length)
	{
	  indentPrint ();
	  eprintf ("\\ %i ", index);
	  roledefPrint (rd);
	  eprintf ("\n");
	  index++;
	  rd = rd->next;
	}
    }
}

//------------------------------------------------------------------------
// Larger logical componentents
//------------------------------------------------------------------------

//! Goal selection
/**
 * Should be ordered to prefer most constrained; for now, it is simply the first one encountered.
 */
Goal
select_goal ()
{
  Goal goal;
  int run;

  goal.run = INVALID;
  goal.rd = NULL;
  for (run = 0; run < sys->maxruns; run++)
    {
      Roledef rd;
      int index;

      index = 0;
      rd = sys->runs[run].start;
      while (rd != NULL && index < sys->runs[run].length)
	{
	  if (isGoal (rd) && !isBound (rd))
	    {
	      // Return this goal
	      goal.run = run;
	      goal.index = index;
	      goal.rd = rd;
	      return goal;
	    }
	  index++;
	  rd = rd->next;
	}
    }
  return goal;
}

//! Bind a regular goal
int
bind_goal_regular (const Goal goal)
{
  /*
   * This is a local function so we have access to goal
   */
  int bind_this (Protocol p, Role r, Roledef rd, int index)
  {
    int element_f1 (Termlist substlist)
    {
      /**
       * Two options; as this, it is from an existing run,
       * or from a new one.
       *
       * Note that we only bind to regular runs here
       */
      int flag;

      if (p == INTRUDER)
	{
	  return 1;		// don't abort scans
	}
      flag = bind_existing_run (goal, p, r, index);
      if (flag)
	{
	  flag = bind_new_run (goal, p, r, index);
	}
      return flag;
    }

    // Test for interm unification
    return termMguInTerm (goal.rd->message, rd->message, element_f1);
  }

  // Bind to all possible sends or intruder node;
  return (iterate_role_sends (bind_this) && add_intruder_goal_iterate (goal));
}

//! Bind an intruder goal to a regular run
int
bind_intruder_to_regular (const Goal goal)
{
  int bind_this_f2 (Protocol p, Role r, Roledef rd, int index)
  {
    int element_f2 (Termlist substlist, Termlist keylist)
    {
      int flag;

      /**
       * Note that we only bind to regular runs here
       */
      if (p == INTRUDER)
	{
	  return 1;		// don't abort scans
	}
      else
	{
	  int keygoals;

	  /**
	   * In any case, the list of keys is added as a new goal.
	   */
	  int add_key_goal (Term t)
	  {
	    keygoals++;
	    create_intruder_goal (t);
	    //!@todo This needs a mapping Pi relation as well.
	    return 1;
	  }

	  keygoals = 0;
	  termlist_iterate (keylist, add_key_goal);
	  /**
	   * Two options; as this, it is from an existing run,
	   * or from a new one.
	   */

	  /**
	   * This code has a major bug (memory destruction)
	   * in both branches
	   *@todo FIX!!
	   */
	  flag = (bind_existing_run (goal, p, r, index)
		  && bind_new_run (goal, p, r, index));

	  /**
	   * deconstruct key list goals
	   */
	  while (keygoals > 0)
	    {
	      roleInstanceDestroy (sys);
	      keygoals--;
	    }

	  return flag;
	}
    }

    // Test for subterm unification
    return termMguSubTerm (goal.rd->message, rd->message, element_f2,
			   sys->traceKnow[0]->inverses, NULL);
  }

  // Bind to all possible sends?
  return iterate_role_sends (bind_this_f2);
}

//! Bind an intruder goal by intruder construction
int
bind_intruder_to_construct (const Goal goal)
{
}


//! Bind an intruder goal
/**
 * Computes F2 as in Athena explanations.
 */
int
bind_goal_intruder (const Goal goal)
{
  return (bind_intruder_to_regular (goal) &&
	  bind_intruder_to_construct (goal));
}

//! Bind a goal in all possible ways
int
bind_goal (const Goal goal)
{
  if (sys->runs[goal.run].protocol == INTRUDER)
    {
      return bind_goal_intruder (goal);
    }
  else
    {
      return bind_goal_regular (goal);
    }
}

//! Prune determination
/**
 *@returns true iff this state is invalid for some reason
 */
int
prune ()
{
  if (indentDepth > 2)
    {
      // Hardcoded limit on iterations
#ifdef DEBUG
      indentPrint ();
      eprintf ("Pruned because too many iteration levels.\n");
#endif
      return 1;
    }
  if (sys->maxruns > 5)
    {
      // Hardcoded limit on runs
#ifdef DEBUG
      indentPrint ();
      eprintf ("Pruned because too many runs.\n");
#endif
      return 1;
    }
  return 0;
}

//------------------------------------------------------------------------
// Main logic core
//------------------------------------------------------------------------

//! Main recursive procedure for Arachne
int
iterate ()
{
  int flag;
  Goal goal;

  flag = 1;
  indentDepth++;
  if (!prune ())
    {
      /**
       * Not pruned: count
       */

      sys->states = statesIncrease (sys->states);
#ifdef DEBUG
      if (explanation != NULL)
	{
	  indentPrint ();
	  eprintf ("%s ", explanation);

	  if (e_run != INVALID)
	    eprintf ("#%i ", e_run);
	  if (e_term1 != NULL)
	    {
	      termPrint (e_term1);
	      eprintf (" ");
	    }
	  if (e_term2 != NULL)
	    {
	      termPrint (e_term2);
	      eprintf (" ");
	    }
	  if (e_term3 != NULL)
	    {
	      termPrint (e_term3);
	      eprintf (" ");
	    }
	  eprintf ("\n");
	  explanation = NULL;
	  e_run = INVALID;
	  e_term1 = NULL;
	  e_term2 = NULL;
	  e_term3 = NULL;
	}
#endif

      /**
       * Check whether its a final state (i.e. all goals bound)
       */

      goal = select_goal ();
      if (goal.run == INVALID)
	{
	  /*
	   * all goals bound, check for property
	   */
	  sys->claims = statesIncrease (sys->claims);
	  printSemiState ();
	  //!@todo Property check in Arachne.
	}
      else
	{
#ifdef DEBUG
	  indentPrint ();
	  eprintf ("Trying to bind goal ");
	  termPrint (goal.rd->message);
	  eprintf (" from run %i, index %i.\n", goal.run, goal.index);
#endif
	  /*
	   * bind this goal in all possible ways and iterate
	   */
	  flag = bind_goal (goal);
	}
    }
  indentDepth--;
  return flag;
}

//! Main code for Arachne
/**
 * For this test, we manually set up some stuff.
 *
 * But later, this will just iterate over all claims.
 */
int
arachne ()
{
  /*
   * set up claim role(s)
   */

  if (sys->maxruns > 0)
    {
      sys->runs[0].length = roledef_length (sys->runs[0].start);
    }

#ifdef DEBUG
  explanation = NULL;
  e_run = INVALID;
  e_term1 = NULL;
  e_term2 = NULL;
  e_term3 = NULL;
#endif
  indentDepth = 0;
  printSemiState ();

  /*
   * iterate
   */
  iterate ();
}
