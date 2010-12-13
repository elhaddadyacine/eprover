/*-----------------------------------------------------------------------

File  : cco_proc_ctrl.c

Author: Stephan Schulz (schulz@eprover.org)

Contents
 
  Code for process control.

  Copyright 2010 by the author.
  This code is released under the GNU General Public Licence and
  the GNU Lesser General Public License.
  See the file COPYING in the main E directory for details..
  Run "eprover -h" for contact information.

Changes

<1> Wed Jul 14 15:54:29 BST 2010
    New

-----------------------------------------------------------------------*/

#include <sys/select.h>
#include "cco_proc_ctrl.h"


/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/



/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
//
// Function: EPCtrlAlloc()
//
//   Allocate an initialized EPCtrlCell.
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

EPCtrl_p EPCtrlAlloc(char *name)
{
   EPCtrl_p ctrl = EPCtrlCellAlloc();

   ctrl->pid        = 0;
   ctrl->pipe       = NULL;
   ctrl->input_file = 0;
   ctrl->name       = SecureStrdup(name);
   ctrl->output     = DStrAlloc();

   return ctrl;
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlFree()
//
//   Free a EPCtrlCell.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void EPCtrlFree(EPCtrl_p junk)
{
   if(junk->input_file)
   {
      FREE(junk->input_file);
   }
   if(junk->name)
   {
      FREE(junk->name);
   }
   DStrFree(junk->output);
   EPCtrlCellFree(junk);
}





/*-----------------------------------------------------------------------
//
// Function: EPCtrlCleanup()
//
//   Clean up: Kill process, close pipe, 
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/

void EPCtrlCleanup(EPCtrl_p ctrl)
{
   if(ctrl->pid)
   {
      kill(ctrl->pid, SIGTERM);
      ctrl->pid = 0;
   }
   if(ctrl->pipe)
   {
      pclose(ctrl->pipe);
      ctrl->pipe = NULL;
   }
   if(ctrl->input_file)
   {
      TempFileRemove(ctrl->input_file);
      FREE(ctrl->input_file);
      ctrl->input_file = NULL;
   }
}



/*-----------------------------------------------------------------------
//
// Function: ECtrlCreate()
//
//   Create a pipe running prover with time limit cpu_limit on
//   file. "prover" must conform to the calling conventions of E and
//   provide similar output. This takes over responsibility for the
//   string pointed to by file.
//
// Global Variables: -
//
// Side Effects    : Yes ;-)
//
/----------------------------------------------------------------------*/

EPCtrl_p ECtrlCreate(char* prover, char* name, long cpu_limit, char* file)
{
   DStr_p   cmd = DStrAlloc();
   EPCtrl_p res = EPCtrlAlloc(name);
   char           line[180];
   char*          l;

   DStrAppendStr(cmd, prover);
   DStrAppendStr(cmd, 
                 " --print-pid -s -xAuto -tAuto"
                 " --memory-limit=512 --tstp-format --cpu-limit=");
   DStrAppendInt(cmd, cpu_limit);
   DStrAppendStr(cmd, " ");
   DStrAppendStr(cmd, file);
   
   res->input_file = file;
   /* printf("# Executing: %s\n", DStrView(cmd)); */
   res->pipe = popen(DStrView(cmd), "r");
   if(!res->pipe)
   {
      TmpErrno = errno;
      SysError("Cannot start eprover subprocess", SYS_ERROR);
   }
   res->fileno = fileno(res->pipe);
   l=fgets(line, 180, res->pipe);
   
   if(!strstr(line, "# Pid: "))
   {
      Error("Cannot get eprover PID", OTHER_ERROR);      
   }
   res->pid = atoi(line+7);
   DStrAppendStr(res->output, line);

   DStrFree(cmd);
   return res;
}



/*-----------------------------------------------------------------------
//
// Function: EPCtrlGetResult()
//
//   Read a line from the E process and find out if it gives a
//   result. 
//
// Global Variables: -
//
// Side Effects    : Yes ;-)
//
/----------------------------------------------------------------------*/

ProverResult EPCtrlGetResult(EPCtrl_p ctrl, char* buffer, long buf_size)
{
   ProverResult res = PRNoResult;
   char* l;

   l=fgets(buffer, buf_size, ctrl->pipe);
   
   if(!l)
   {
      res = PRFailure;
   }
   else
   {
      DStrAppendStr(ctrl->output, l);
      
      if(strstr(buffer, SZS_THEOREM_STR))
      {
         res = PRTheorem;
      }
      else if(strstr(buffer, SZS_UNSAT_STR))
      {
         res = PRUnsatisfiable;
      }
      else if(strstr(buffer, SZS_SATSTR_STR))
      {
         res = PRSatisfiable;
      }
      else if(strstr(buffer, SZS_COUNTERSAT_STR))
      {
         res = PRCounterSatisfiable;
      }
      if(res!=PRNoResult)
      {
         ctrl->result = res;
      }
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetAlloc()
//
//   Allocate an empty EPCtrlCell.
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/

EPCtrlSet_p EPCtrlSetAlloc(void)
{   
   EPCtrlSet_p handle = EPCtrlSetCellAlloc();

   handle->procs     = NULL;

   return handle;
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetFree()
//
//   Free an EPCtrlSet(), including the payload.Will clean up the
//   processes. 
//
// Global Variables: -
//
// Side Effects    : Will terminate processes, memory, ...
//
/----------------------------------------------------------------------*/

void EPCtrlSetFree(EPCtrlSet_p junk)
{
   NumTree_p cell;

   while(junk->procs)
   {
      cell = NumTreeExtractRoot(&(junk->procs));
      EPCtrlCleanup(cell->val1.p_val);
      EPCtrlFree(cell->val1.p_val);
      NumTreeCellFree(cell);
   }
   EPCtrlSetCellFree(junk);
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetAddProc()
//
//   Add a process to the process set.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void EPCtrlSetAddProc(EPCtrlSet_p set, EPCtrl_p proc)
{
   IntOrP tmp;

   tmp.p_val = proc;
   NumTreeStore(&(set->procs), proc->fileno, tmp, tmp);
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetFindProc()
//
//   Find the process associated with fd.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

EPCtrl_p EPCtrlSetFindProc(EPCtrlSet_p set, int fd)
{
   NumTree_p cell;

   cell = NumTreeFind(&(set->procs), fd);
   
   if(cell)
   {
      return cell->val1.p_val;
   }
   return NULL;
}


/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetDeleteProc()
//
//   Delete a process from the set.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void EPCtrlSetDeleteProc(EPCtrlSet_p set, EPCtrl_p proc)
{
   NumTree_p cell;
   
   cell = NumTreeExtractEntry(&(set->procs), proc->fileno);
   if(cell)
   {
      EPCtrlCleanup(cell->val1.p_val);
      EPCtrlFree(cell->val1.p_val);
      NumTreeCellFree(cell);      
   }
}

/*-----------------------------------------------------------------------
//
// Function: EPCtrlSetGetResult()
//
//   
//
// Global Variables: 
//
// Side Effects    : 
//
/----------------------------------------------------------------------*/

EPCtrl_p EPCtrlSetGetResult(EPCtrlSet_p set)
{
   ProverResult tmp;
   fd_set readfds, writefds, errorfds;
   PStack_p trav_stack;
   NumTree_p cell;
   int maxfd = 0,i;
   EPCtrl_p handle, res = NULL;
   struct timeval waittime;

   FD_ZERO(&readfds);
   FD_ZERO(&writefds); 
   FD_ZERO(&errorfds);
   waittime.tv_sec  = 0;
   waittime.tv_usec = 500000;

   trav_stack = NumTreeTraverseInit(set->procs);
   while((cell = NumTreeTraverseNext(trav_stack)))
   {
      handle = cell->val1.p_val;
      FD_SET(handle->fileno, &readfds);
      maxfd = handle->fileno;
   }
   NumTreeTraverseExit(trav_stack);

   select(maxfd+1, &readfds, &writefds, &errorfds, &waittime);
   
   for(i=0; i<=maxfd; i++)
   {
      if(FD_ISSET(i, &readfds))
      {
         handle = EPCtrlSetFindProc(set, i);
         tmp = EPCtrlGetResult(handle, set->buffer, EPCTRL_BUFSIZE);
         switch(tmp)
         {
         case PRNoResult:
               break;
         case PRTheorem:
         case PRUnsatisfiable:
               res = handle;
               break;
         case PRSatisfiable:
         case PRCounterSatisfiable:
         case PRFailure:
               /* Process terminates, but no proof found -> Remove it*/
               fprintf(GlobalOut, "# %s terminated unsuccessfully\n", 
                       handle->name);
               EPCtrlSetDeleteProc(set, handle);
               break;
         default:
               assert(false && "Impossible ProverResult");
         }
      }
   }
   return res;
}


/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/

