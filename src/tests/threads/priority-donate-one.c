/* The main thread acquires a lock.  Then it creates two
   higher-priority threads that block acquiring the lock, causing
   them to donate their priorities to the main thread.  When the
   main thread releases the lock, the other threads should
   acquire it in priority order.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire1_thread_func;
static thread_func acquire2_thread_func;

void
test_priority_donate_one (void) 
{
  struct lock lock;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  lock_acquire (&lock);
  //msg("\nrunning remaining code -----> %d\n",lock.holder->tid); // this is called by the thread 2 which is created to finish the function acquire_thread_func only but there is a thing which is very strange
  thread_create ("acquire1", PRI_DEFAULT + 1, acquire1_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("acquire2", PRI_DEFAULT + 2, acquire2_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
      //  msg("before releasing the lock_holder priority -> %d\n",lock.holder->priority); // donation is 100%
      //  struct list_elem* e = list_begin(&lock.holder->locks_donated_priorities);
      //  while(e != list_end(&lock.holder->locks_donated_priorities)){
      //    struct lock_donated_priority *l_d_p = list_entry(e,struct lock_donated_priority,lock_donated_elem );
      //    msg("lock [%d] , donated_priority [%d]",l_d_p->lock,l_d_p->donated_priority);
      //    e= list_next(e);
      //  }
  // msg("tid : %d, priority %d",lock.holder->tid , lock.holder->priority);
  lock_release (&lock); // here all the problems arises 
  // if(lock.holder == NULL){msg("the lock_holder is NULL");} // no problem here this is what should happen
  // msg("\nrunning remaining code -----> %d\n",thread_current()->tid); // this is called by the thread 2 which is created to finish the function acquire_thread_func only but there is a thing which is very strange
  msg ("acquire2, acquire1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}

static void
acquire1_thread_func (void *lock_) 
{
  struct lock *lock = lock_;
  // msg("lock holder for acquire 1 --> %d",lock->holder->tid); //tid = 1
  lock_acquire (lock);
  msg("acquire1: got the lock"); // this isn't called 
  lock_release (lock);
  msg ("acquire1: done");
   //msg("before releasing the lock_holder priority -> %d\n",lock.holder->priority); // donation is 100%
}

static void
acquire2_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  // msg("lock holder for acquire 2 --> %d" ,lock->holder->tid); //tid = 1
  lock_acquire (lock);
  // msg("\nlock holder for acquire 2 -----> %d\n",lock->holder->tid);
  // msg ("acquire1: got the lock #%d#",lock->holder->priority);


  msg ("acquire2: got the lock");
  lock_release (lock);
  msg ("acquire2: done");
}
