/* reachable.c */

/*
 * MONA is Copyright (C) 1997-1998 BRICS. All rights reserved. 
 *
 * Reproduction of all or part of this software is permitted for
 * educational or research use on condition that this copyright notice is
 * included in any copy. This software comes with no warranty of any
 * kind. In no event will BRICS be liable for any damages resulting from
 * use of this software.
 */

#include <stdlib.h>
#include "gta.h"
#include "dyn.h"

GTA *orig, *res; /* original and result GTA */
BehaviourMatrix *resbeh;
State **newNumber; /* maps old state numbers to new */
State **oldNumber; /* inverse of newNumber */
State *nextNewNumber; /* used size of newNumber */
SsId s, lSs, rSs; /* current, left and right state spaces */

/* auxiliary functions */

static unsigned fn_reachable(unsigned v)
{
  /* first time reached? */
  if (newNumber[s][v] == -1) {
    SsId i;

    /* assign new state number and insert into unproc */
    newNumber[s][v] = nextNewNumber[s];
    oldNumber[s][nextNewNumber[s]] = v;
    nextNewNumber[s]++;

    /* extend matrices */
    for (i = 0; i < guide.numHitsLeft[s]; i++)
      extendLeftBM(&resbeh[guide.hitsLeft[s][i]]);
    for (i = 0; i < guide.numHitsRight[s]; i++)
      extendRightBM(&resbeh[guide.hitsRight[s][i]]);
  }
  return newNumber[s][v];
}

static void makeReachableBehaviour(State i, State j)
{
  bdd_apply1(orig->ss[s].bddm, 
	     BDD_ROOT(orig->ss[s].bddm, 
		      BEH(orig->ss[s], oldNumber[lSs][i], oldNumber[rSs][j])), 
	     res->ss[s].bddm, 
	     fn_reachable);
  BM(resbeh[s], i, j) = BDD_LAST_HANDLE(res->ss[s].bddm);
}	
    
/* main function */

GTA *gtaReachable(GTA *g)
{
  int i, j, done;

  /* initialize */ 
  orig = g;
  res = gtaMake();
  resbeh = (BehaviourMatrix *) mem_alloc(sizeof(BehaviourMatrix)*guide.numSs);
  newNumber = (State **) mem_alloc(sizeof(State *)*guide.numSs);
  oldNumber = (State **) mem_alloc(sizeof(State *)*guide.numSs);
  nextNewNumber = (unsigned *) mem_alloc(sizeof(unsigned)*guide.numSs);
  for (s = 0; s < guide.numSs; s++) {
    unsigned maxSize = orig->ss[s].size;

    newNumber[s] = (State *) mem_alloc(sizeof(State)*maxSize);
    oldNumber[s] = (State *) mem_alloc(sizeof(State)*maxSize);
    for (i = 0; i < maxSize; i++)
      newNumber[s][i] = -1; /* initialize as unreachable */
    nextNewNumber[s] = 1;
    newNumber[s][orig->ss[s].initial] = 0; /* initial state is reachable, gets number 0 */
    oldNumber[s][0] = orig->ss[s].initial; /* inverse of newNumber */
    
    res->ss[s].bddm = bdd_new_manager(bdd_size(orig->ss[s].bddm), 
				      bdd_size(orig->ss[s].bddm)/8+2);
    bdd_prepare_apply1(orig->ss[s].bddm);

    initBMtoSize(&resbeh[s], 1, 1);
  }

  /* iterate until all reachable have been moved */
  do {
    done = 1;
    for (s = 0; s < guide.numSs; s++) {
      lSs = guide.muLeft[s];
      rSs = guide.muRight[s];

      if (resbeh[s].lf < resbeh[s].lu) {
	/* left-direction has been extended */
	int lu = resbeh[s].lu, rf = resbeh[s].rf;
	done = 0;

	for (i = resbeh[s].lf; i < lu; i++)
	  for (j = 0; j < rf; j++)
	    makeReachableBehaviour(i, j);

	resbeh[s].lf = lu;
      }

      if (resbeh[s].rf < resbeh[s].ru) {
	/* right-direction has been extended */
	int ru = resbeh[s].ru, lf = resbeh[s].lf;
	done = 0;

	for (i = 0; i < lf; i++)
	  for (j = resbeh[s].rf; j < ru; j++)
	    makeReachableBehaviour(i, j);

	resbeh[s].rf = ru;
      }
    }
  } while (!done);

  /* move behaviour to result automaton */  
  for (s = 0; s < guide.numSs; s++) {
    StateSpace *ss = &res->ss[s];

    ss->initial = 0; 
    ss->size = nextNewNumber[s];
    ss->ls = resbeh[s].ls;
    ss->rs = resbeh[s].rs;
    ss->behaviour = resbeh[s].m;
  }

  /* set final status */
  res->final = (int *) mem_alloc(sizeof(int)*res->ss[0].size);
  for (i = 0; i < res->ss[0].size; i++)
    res->final[i] = orig->final[oldNumber[0][i]];

  /* clean up */
  for (s = 0; s < guide.numSs; s++) {
    free(newNumber[s]);
    free(oldNumber[s]);
  }
  free(newNumber);
  free(oldNumber);
  free(nextNewNumber);
  free(resbeh);
  gtaFree(orig); /* notice: original is freed!! */
  return res;
}
