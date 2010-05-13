/*-----------------------------------------------------------------------

File  : ccl_global_indices.h

Author: Stephan Schulz (schulz@eprover.org)

Contents
 
  Code abstracting several (optional) indices into one structure. 

  Copyright 2010 by the author.
  This code is released under the GNU General Public Licence.
  See the file COPYING in the main CLIB directory for details.
  Run "eprover -h" for contact information.

Changes

<1> Fri May  7 21:13:39 CEST 2010
    New

-----------------------------------------------------------------------*/

#ifndef CCL_GLOBAL_INDICES

#define CCL_GLOBAL_INDICES

#include <ccl_subterm_index.h>
#include <ccl_subterm_index.h>
#include <ccl_clausesets.h>


/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/


typedef struct global_indices_cell
{
   SubtermIndex_p    bw_rw_index;   
}GlobalIndices, *GlobalIndices_p;


/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/


void GlobalIndicesNull(GlobalIndices_p indices);
void GlobalIndicesInit(GlobalIndices_p indices, 
                       bool use_bw_rw_index,
                       bool use_pm_into_index,
                       bool use_pm_from_index);
void GlobalIndicesFreeIndices(GlobalIndices_p indices);
void GlobalIndicesReset(GlobalIndices_p indices);

void GlobalIndicesRelease(GlobalIndices_p indices);

void GlobalIndicesInsertClause(GlobalIndices_p indices, Clause_p clause);
void GlobalIndicesDeleteClause(GlobalIndices_p indices, Clause_p clause);
void GlobalIndicesInsertClauseSet(GlobalIndices_p indices, ClauseSet_p set);


#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/




