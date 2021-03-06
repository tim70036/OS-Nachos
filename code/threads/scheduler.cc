// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"
#include <iostream>

using namespace std;

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------


/* MP3 Compare func */
int burstCmp(Thread *a, Thread *b)
{
    int aTime = a->getBurstTime();
    int bTime = b->getBurstTime();
    if(aTime == bTime)  return 0;
    else if(aTime > bTime) return 1;
    else return -1;
}

int priorityCmp(Thread *a, Thread *b)
{
    int aPriority = a->getPriority();
    int bPriority = b->getPriority();
    if(aPriority == bPriority)  return 0;
    else if(aPriority > bPriority) return -1;
    else return 1;
}

/* MP3 Check aging */
bool Scheduler::CheckAging(Thread *thread)
{
    int nowTime = kernel->stats->totalTicks;
    /* In ready queue and wait time >= 1500 */
    if(thread->getStatus() == READY && nowTime - thread->getStartWaitTime() >= 1500)
    {
        /* Aging */
        int oldPriority = thread->getPriority();
        int newPriority = (oldPriority + 10 > 149) ? 149 : oldPriority + 10;
        thread->setPriority(newPriority);
        if(oldPriority != newPriority){
          cout << "Tick " << nowTime << ": Thread " << thread->getID();
          cout << " changes its priority from " << oldPriority << " to " << newPriority << endl;
        }

        if(newPriority >= 100 && newPriority < 110) /* L2 -> L1 */
        {
            if(kernel->scheduler->L2Queue->IsInList(thread))
                kernel->scheduler->L2Queue->Remove(thread);
            kernel->scheduler->L1Queue->Insert(thread);
            cout << "Tick " << nowTime << ": Thread " << thread->getID() << " is removed from queue L2" << endl;
            cout << "Tick " << nowTime << ": Thread " << thread->getID() << " is inserted into queue L1"<< endl;

            /* Preemptive , Only SJF */
            if( 100 <= kernel->currentThread->getPriority() && kernel->currentThread->getPriority() <= 149 )
            {
   	          if(kernel->currentThread->getID() != thread->getID())
              {
   	            double actBurst = kernel->stats->userTicks - kernel->currentThread->getStartTime();
		            double estBurst = 0.5 * actBurst + 0.5 * kernel->currentThread->getBurstTime();
                if(thread->getBurstTime() < estBurst)
                  kernel->currentThread->Yield();
              }
            }
            /* Reset wait time */
            thread->setStartWaitTime(nowTime);
            return TRUE;
        }
        else if(newPriority >= 50 && newPriority < 60) /* L3 -> L2 */
        {
            kernel->scheduler->readyList->Remove(thread);
            kernel->scheduler->L2Queue->Insert(thread);
            cout << "Tick " << nowTime << ": Thread " << thread->getID() << " is removed from queue L3" << endl;
            cout << "Tick " << nowTime << ": Thread " << thread->getID() << " is inserted into queue L2" << endl;
        }
        /* Reset wait time */
        thread->setStartWaitTime(nowTime);
    }
    return FALSE;
}

Scheduler::Scheduler()
{
    readyList = new List<Thread *>;
    toBeDestroyed = NULL;

    /* MP3 Init Queue */
    L1Queue = new SortedList<Thread *>(burstCmp);
    L2Queue = new SortedList<Thread *>(priorityCmp);
}



//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
    delete readyList;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);

    /* MP3 into queue */
    int p = thread->getPriority();
    int nowTime = kernel->stats->totalTicks;
    cout << "Tick " << nowTime << ": Thread " << thread->getID() << " is inserted into queue L";
    if(100 <=  p && p <= 149)
	{
        L1Queue->Insert(thread);
        cout << 1 << endl;
    }
	else if(50 <= p && p <= 99)
    {
		L2Queue->Insert(thread);
        cout << 2 << endl;
    }
	else
    {
     readyList->Append(thread);
     cout << 3 << endl;
    }

    /* MP3 Aging , now thread starts to wait */
    thread->setStartWaitTime(nowTime);

    /* MP3 preemptive , only SJF */
    if(100 <=  p && p <= 149) /* something is added into L1 queue */
    {
        if( 100 <= kernel->currentThread->getPriority() && kernel->currentThread->getPriority() <= 149 )
        {
          if(kernel->currentThread->getID() != thread->getID())
          {
   	        double actBurst = kernel->stats->userTicks - kernel->currentThread->getStartTime();
		        double estBurst = 0.5 * actBurst + 0.5 * kernel->currentThread->getBurstTime();
            if(thread->getBurstTime() < estBurst)
              kernel->currentThread->Yield();
          }
        }
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    /* MP3 Which is Next ? */
    int nowTime = kernel->stats->totalTicks;
    if(!L1Queue->IsEmpty())
    {
        cout << "Tick " << nowTime << ": Thread " << L1Queue->Front()->getID() << " is removed from queue L";
        cout << 1 << endl;

        return L1Queue->RemoveFront();
    }
    else if(!L2Queue->IsEmpty())
    {
        cout << "Tick " << nowTime << ": Thread " << L2Queue->Front()->getID() << " is removed from queue L";
        cout << 2 << endl;

        return L2Queue->RemoveFront();
    }
    else if (!readyList->IsEmpty())
    {
        cout << "Tick " << nowTime << ": Thread " << readyList->Front()->getID() << " is removed from queue L";
        cout << 3 << endl;

        return readyList->RemoveFront();
    }
    else
        return NULL;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    /* MP3 thread start */

    int nowTime = kernel->stats->totalTicks;
    int nowUserTime = kernel->stats->userTicks;

    nextThread->setStartTime(nowUserTime);
    int oldThreadTime = nowUserTime - oldThread->getStartTime();

    cout << "Tick " << nowTime << ": Thread " << nextThread->getID() <<" is now selected for execution" << endl;
    cout << "Tick " << nowTime << ": Thread " << oldThread->getID() <<" is replaced, and it has executed ";
    cout << oldThreadTime << " ticks" << endl;


    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());




    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".



    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up

    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}
