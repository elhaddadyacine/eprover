/*-----------------------------------------------------------------------

File  : ccl_fcvindexing.h

Author: Stephan Schulz

Contents

  Functions for handling frequency count vector indexing for clause
  subsumption.

  2003 by the author.
  This code is released under the GNU General Public Licence.
  See the file COPYING in the main CLIB directory for details.
  Run "eprover -h" for contact information.

Changes

<1> Tue Jul  1 13:05:36 CEST 2003
    New

-----------------------------------------------------------------------*/

#ifndef CCL_FCVINDEXING

#define CCL_FCVINDEXING

#include <ccl_freqvectors.h>


/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/

#define FVINDEX_MAX_FEATURES_DEFAULT 50  /* Maximal lenght of feature vector */
#define FVINDEX_SYMBOL_SLACK_DEFAULT 30  /* Reserve symbols for splitting */

typedef struct fvindex_parms_cell
{
   FVIndexType features;
   bool use_perm_vectors;
   bool eleminate_uninformative;
   long max_symbols;
   long symbol_slack;
}FVIndexParmsCell, *FVIndexParms_p;


/*

Case 1: Just one successor
- Key is >=0
- Array size is 0
- Pointer to successor

Case 1a: No successor
- Key is -1
- Array size is 0
- Pointer is NULL

Case 2: Many successors
- Key is -1
- Array Size > 0
- Pointer to array 

Case 3: Final cell
- Number = -2
- Array size = 0
- Pointer contains PTree of clauses

Initial case: Many successors with array size N

*/

typedef struct fv_index_cell
{
   long type_or_key;
   long clause_count;
   long array_size;
   union 
   {
      struct fv_index_cell **successors;
      struct fv_index_cell *succ;
      PTree_p clauses;
   } u1;
}FVIndexCell, *FVIndex_p;

typedef struct fvi_anchor_cell
{
   long         symbol_limit;
   FVIndexType  features;
   PermVector_p perm_vector;
   long         node_count;
   long         array_count;
   FVIndex_p    index;
}FVIAnchorCell, *FVIAnchor_p;


/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

#define FVIndexParmsCellAlloc() (FVIndexParmsCell*)SizeMalloc(sizeof(FVIndexParmsCell))
#define FVIndexParmsCellFree(junk) SizeFree(junk, sizeof(FVIndexParmsCell))

FVIndexParms_p FVIndexParmsAlloc(void);
#define FVIndexParmsFree(junk) FVIndexParmsCellFree(junk)

#define FVINDEXTYPE_EMPTY   -1
#define FVINDEXTYPE_MANY    -2
#define FVINDEXTYPE_FINAL   -3

#define FVIndexEmptyNode(node) ((node)->type_or_key == FVINDEXTYPE_EMPTY)
#define FVIndexUnaryNode(node) ((node)->type_or_key >= 0)
#define FVIndexFinalNode(node) ((node)->type_or_key == FVINDEXTYPE_FINAL)
#define FVIndexManySuccNode(node) ((node)->type_or_key == FVINDEXTYPE_MANY)

#define FVIndexCellAlloc()    (FVIndexCell*)SizeMalloc(sizeof(FVIndexCell))
#define FVIndexCellFree(junk) SizeFree(junk, sizeof(FVIndexCell))

FVIndex_p FVIndexAlloc(void);
void      FVIndexFree(FVIndex_p junk);

#define FVIAnchorCellAlloc()    (FVIAnchorCell*)SizeMalloc(sizeof(FVIAnchorCell))
#define FVIAnchorCellFree(junk) SizeFree(junk, sizeof(FVIAnchorCell))

FVIAnchor_p FVIAnchorAlloc(long symbol_limit, FVIndexType features, PermVector_p perm);
void        FVIAnchorFree(FVIAnchor_p junk);

#ifdef CONSTANT_MEM_ESTIMATE
#define FVINDEX_MEM 20
#else
#define FVINDEX_MEM MEMSIZE(FVIndexCell)
#endif

#define FVIndexStorage(index) ((index)?(index)->node_count*FVINDEX_MEM+\
			       (index)->array_count*LONG_MEM:0)

FVIndex_p   FVIndexGetNextNonEmptyNode(FVIndex_p node, long key);
void        FVIndexInsert(FVIAnchor_p index, FreqVector_p vec_clause);

bool        FVIndexDelete(FVIAnchor_p index, Clause_p clause);

long        FVIndexCountNodes(FVIndex_p index, bool leafs, bool empty);

FVPackedClause_p FVIndexPackClause(Clause_p clause, FVIAnchor_p anchor);

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/





