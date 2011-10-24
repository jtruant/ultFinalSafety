#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* We want the extra information from these definitions */
#ifndef __USE_GNU
#define __USE_GNU
#endif /* __USE_GNU */
#include <ucontext.h>
#include "ULT.h"
struct ThrdCtlBlk *queueHead=NULL;
struct ThrdCtlBlk *queueTail=NULL;
//number of existing threads
static Tid universalTid=0;
//currently running thread
static Tid runningThread=0;
static Tid returnTid=0;
static Tid tidToRun=0;
static int numThreads = 0;

struct ThrdCtlBlk *fromQueue(Tid searchTid,struct ThrdCtlBlk **queueHead,struct ThrdCtlBlk *retBlock);
void pushQueue(struct ThrdCtlBlk ** queueHead,struct ThrdCtlBlk **queueTail, struct ThrdCtlBlk *pushBlock);
int validTid(Tid searchTid, struct ThrdCtlBlk **queueHead);

Tid ULT_CreateThread(void (*fn)(void *), void *parg)
{
//initialize the new thread
        ThrdCtlBlk *newThread = (ThrdCtlBlk *)malloc(sizeof(ThrdCtlBlk));
        newThread->context = (ucontext_t *)malloc(sizeof(ucontext_t));

        getcontext(&newThread->threadContext);
        newThread->context->uc_stack.ss_sp = (char *)malloc(ULT_MIN_STACK);
        newThread->context->uc_stack.ss_size = ULT_MIN_STACK;

        //makecontext
        makecontext(newThread->context,(void(*)())StubFn, 3, fn, parg);

        //assign thread tid to new thread
        universalTid = universalTid + 1;
	newThread->tid = universalTid;

        numThreads +=1;

	pushQueue(&queueHead, &queueTail, newThread);

        return newThread->tid;

 // assert(0); /* TBD */
 // return ULT_FAILED;
}

void StubFn(void(*root)(void *), void *arg)
{
//thread starts here
Tid ret;
root(arg); //call root function
ret = ULT_DestroyThread(ULT_SELF);
assert(ret == ULT_NONE); //we should only get if we are the last thread
exit(0); //all threads are done, so process should exit
}


Tid ULT_Yield(Tid wantTid)
{
  
  if(wantTid>ULT_MAX_THREADS)
  {   
       /*The tid does not correspond to a valid thread*/ 
     returnTid = ULT_INVALID;
     tidToRun = 0;
     wantTid=0;	 
  }
  else if((queueHead==NULL) && (wantTid==ULT_ANY))//if queue empty and ULT_any then no-op
  {
       returnTid = ULT_NONE;
       tidToRun=0;
       wantTid=0;
  }
  else if(wantTid == ULT_SELF)
  {
	wantTid = runningThread;
	tidToRun=0;
	returnTid= runningThread;
  }
  else if(wantTid == ULT_ANY)
  {    
	returnTid=queueHead->tid;
	tidToRun=queueHead->tid;
	wantTid = queueHead->tid;
  }  
  else if((wantTid!=0)&&(!validTid(wantTid,&queueHead)))//check to see if tid is a valid available tid
  {
        returnTid=ULT_INVALID;
        tidToRun=0;
	wantTid=0; 
  }
  else
  {
    tidToRun=wantTid;
    returnTid = tidToRun;
  }
  
  /*declare context variable*/
  ucontext_t currThread;

  /*build TCB*/
  struct ThrdCtlBlk *currBlock;
  /*allocate memory */
  currBlock=(struct ThrdCtlBlk*)malloc(sizeof(ThrdCtlBlk));
  currBlock->tid=universalTid;
  currBlock->tcbPointerTail=NULL;
  currBlock->tcbPointerHead=NULL;
  /*build TCB for returning from queue*/
  struct ThrdCtlBlk *retBlock;
  retBlock=(struct ThrdCtlBlk*)malloc(sizeof(ThrdCtlBlk));
  retBlock->tcbPointerTail=NULL;
  retBlock->tcbPointerHead=NULL;
  /*get context and set the context of the tcb to that context*/ 
  getcontext(&currThread);
  
  /*change instruction pointer JUMP*/
  currThread.uc_mcontext.gregs[REG_EIP]=currThread.uc_mcontext.gregs[REG_EIP]+166;
  currBlock->threadContext=currThread;

  /*stick thread(TCB) on the ready queue*/
  pushQueue(&queueHead,&queueTail,currBlock);
   
  /*decide on new thread to run*/
  struct ThrdCtlBlk *setBlock;
  setBlock=(struct ThrdCtlBlk*)malloc(sizeof(ThrdCtlBlk));
  
  setBlock=fromQueue(tidToRun,&queueHead,retBlock); 
  currThread=setBlock->threadContext;
  /*
  *set context
  */
  setcontext(&currThread);
  printf("Jump barrier \n");
  runningThread=setBlock->tid;
  
  return returnTid;
  
  
 
}


Tid ULT_DestroyThread(Tid tid)
{
  assert(0); /* TBD */
  return ULT_FAILED;
}

/*method to check if requested tid is valid*/
int validTid(Tid searchTid, struct ThrdCtlBlk **queueHead)
{
     struct ThrdCtlBlk *cycleBlock;
     //allocate memory for cycleBlock
     cycleBlock=(struct ThrdCtlBlk*)malloc(sizeof(ThrdCtlBlk));
     cycleBlock=*queueHead;
     while(cycleBlock!=NULL)
     {
	if(cycleBlock->tid==searchTid)
	{ 
           free(cycleBlock);
	   return 1;	
	}
	else
	{
	   cycleBlock=cycleBlock->tcbPointerTail;
	}
     }  
	free(cycleBlock);
	return 0;
}

struct ThrdCtlBlk *fromQueue(Tid searchTid,struct ThrdCtlBlk **queueHead,struct ThrdCtlBlk *retBlock)
{
       //pointer to ThrdCtlBlk
	struct ThrdCtlBlk *tempBlock;
       //struct ThrdCtlBlk *adjustPointer;
       /*allocate memory */
       tempBlock=(struct ThrdCtlBlk*)malloc(sizeof(ThrdCtlBlk));
       tempBlock=*queueHead;
       
	  while(tempBlock!=NULL)
          {
        	if(tempBlock->tid==searchTid)
		{       
                       // tempBlock->tcbPointerHead->
			retBlock->tid=tempBlock->tid;
			retBlock->threadContext=tempBlock->threadContext;
			
			/*adjust pointers referencing tempBlock*/
			if(tempBlock->tcbPointerHead==NULL)
			{
			  *queueHead=(*queueHead)->tcbPointerTail;
			}
			else
			{
		 	  tempBlock->tcbPointerHead->tcbPointerTail=tempBlock->tcbPointerTail;
			  tempBlock->tcbPointerTail->tcbPointerHead=tempBlock->tcbPointerHead;
			}
			free(tempBlock);
			return retBlock; 
		}
    		else
		{
			tempBlock=tempBlock->tcbPointerTail;
		}
          }
  //this will not be a correct tcb. needs fix		
  return tempBlock;
           	
}

void pushQueue(struct ThrdCtlBlk ** queueHead,struct ThrdCtlBlk ** queueTail, struct ThrdCtlBlk *pushBlock)
{
	if(*queueHead==NULL)
	{
		*queueHead=pushBlock;
		 queueTail=queueHead;
	}
	else
 	{
	pushBlock->tcbPointerTail=*queueHead;
	//set the previous head to point at the new head
	pushBlock->tcbPointerTail->tcbPointerHead=pushBlock;
	//pushBlock->tcbPointerHead=queueHead;
	*queueHead=pushBlock;	
	}
}

