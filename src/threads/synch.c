/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <list.h>

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
  {
    list_push_back (&(sema->waiters), &(thread_current ()->elem));
    thread_block (); // es-abdelrahman blocks the thread so it always keeps sema->value >= 0 as it make value threads work after they end 
  }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
// es-abdelrahman
// the problem is the unblocking should be for the higher priority first
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    thread_unblock (thread_with_highest_priority(&sema->waiters)); 
  }
  sema->value++;
  intr_set_level (old_level);
  // after releasing some thread yield
  thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema); // other thread is created with name "sema-test" runs sema_test_helper 
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1); // note that this Implementation isn't secure against missuse as some one can use the semaphore directly to break the lock mechanism
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

void 
donate_priority(struct lock * lock , struct thread* donator){
  // current thread is the (donator)
  struct thread *current = donator;
  // save donated priorities to reverse donation in the release
    /* sanity */
  if (lock == NULL || lock->holder == NULL)
    return;

  if(lock->holder->original_priority > current->priority){
    return; // no need to donate as the original priority is higher than the current one
  }
  struct lock_donated_priority* lock_donated_priority = malloc(sizeof* lock_donated_priority );
  lock_donated_priority->lock = lock;
  // I have made error before by assigning the value of the holder of the lock not the donator 
  lock_donated_priority->donated_priority = current->priority; // save the donated priority even it is smaller than the current one 
  // this should be initialized inside the init_thread but results in page fragmentation 
  if(list_begin(&lock->holder->locks_donated_priorities) == NULL){
    list_init(&lock->holder->locks_donated_priorities);
  }
  // better solution is to insert sorted and just pick the front element  // and even if the lock repeated no problem
  list_push_back(&lock->holder->locks_donated_priorities , &lock_donated_priority->lock_donated_elem); // the problem is from here
  // using this debuger function I can see the lock_donated_priorities list is inserted inside but there is error arises but it crashes
  // donate from current to holder
  if(current->priority > lock->holder->priority){
    lock->holder->priority = current->priority; // donated ----> need to use set priority function no as the recieving isn't the current thread
    if(lock->holder->status == THREAD_RUNNING)return;
    // waiting on onther lock donate to w_lock
    if(lock->holder->status == THREAD_BLOCKED){
        donate_priority( lock->holder->w_lock,lock->holder);
    }
  }
 
}
// no need to list just keep track of only waiting lock variable <-------------
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  struct thread* current = thread_current();

  
  if(lock->holder != NULL){
    donate_priority(lock,thread_current());
      current->w_lock = lock; // set the waiting lock
    }
  
  sema_down (&(lock->semaphore)); // if the value is 0 it will block the thread until it becomes available
  lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
reverse_donation(struct lock *lock){
  // in release pick the highest priority donated if there is no more donated priorities get the original one
  struct thread *holder = lock->holder;
  struct list_elem* elem = list_begin(&holder->locks_donated_priorities);
  // if there is no element in the list this means there is no donation before
  if(elem == NULL) return;
  // search for the lock that I got donated from and remove it from the list if the same lock is used twice !!!
  while(elem != list_end(&holder->locks_donated_priorities)){
    struct lock_donated_priority *l_d_p = list_entry(elem,struct lock_donated_priority,lock_donated_elem );
    // this is perfect now as before was removing the lock based on lock only now we use the donated value to distinguish the thread
    if(l_d_p->lock == lock ){list_remove(&l_d_p->lock_donated_elem);} //remove the lock from the list
    elem = list_next(elem);
  }
  
  // search for the second highest priority after deleting previous one
  int max_sec_priority = -1;
  elem = list_begin(&holder->locks_donated_priorities);
  // if there is no other donators other than the deleted lock
  if(elem == NULL){holder->priority=(holder->original_priority); return;}
  
  // -----------------the problem is here ------------------ // after deleting the previous lock it returns to the original priority which means max_sec_priority is -1 so the problem isn't in the loop it changes the value of max_sec_priority
  while(elem != list_end(&holder->locks_donated_priorities)){
    struct lock_donated_priority *l_d_p = list_entry(elem,struct lock_donated_priority,lock_donated_elem );
    if(l_d_p->donated_priority > max_sec_priority){max_sec_priority = l_d_p->donated_priority; }
    elem = list_next(elem);
  }
  // I think thread_set_priority isn't right here as thread that calling release not necessarily the holder one
  if(max_sec_priority ==-1){holder->priority=(holder->original_priority); return;}

  holder->priority = max_sec_priority;
}
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  reverse_donation(lock);
  lock->holder = NULL; 
  sema_up (&lock->semaphore);
  // thread_yield(); // this was all the problem oooooooooh alhamdu llah now I added to the sema_up it will consider yielding there so no need to repeat it
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}


/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0); // init the private semaphore of the waiter to 0 so when we do sema_down it will block the thread
  list_push_back (&cond->waiters, &waiter.elem); // add the waiter to the list of waiters of the condition variable
  lock_release (lock); // release the lock before blocking the thread so other threads can use it
  sema_down (&waiter.semaphore); // block the thread until it is signaled by another thread using cond_signal
  lock_acquire (lock); // reacquire the lock after being signaled
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */

/*
  es-abdelrahman
  find the semaphore with the highest priority and return it
*/
struct semaphore_elem*
sema_with_highest_priority(struct list* para_list)
{
  if (list_empty(para_list))
    return NULL;
  struct list_elem* e = list_begin(para_list);
  struct semaphore_elem* sema_elem = list_entry(e , struct semaphore_elem , elem);
  struct semaphore_elem* sema_elem_max_priority = sema_elem;
  struct semaphore* sema_max_priority = &sema_elem->semaphore;
  
  

  struct list_elem* thread_list_elem = list_begin(&sema_max_priority->waiters);
  struct thread* t_max_priority = list_entry(thread_list_elem , struct thread , elem);
  int64_t max_priority = t_max_priority->priority;
  if(e==NULL){
    return NULL;
  }
  e = list_next(e);
  while( e != list_end(para_list)){
    struct semaphore_elem* sema_elem = list_entry(e , struct semaphore_elem , elem);
    struct semaphore* sema = &(sema_elem->semaphore);
    struct list_elem* thread_list_elem = list_begin(&sema->waiters);
    struct thread* t= list_entry(thread_list_elem , struct thread , elem);
    if (max_priority < t->priority ){
      max_priority = t->priority;
      t_max_priority = t;
      sema_max_priority = sema;
      sema_elem_max_priority = sema_elem;
    } 
    e = list_next(e);
  }
  list_remove(&sema_elem_max_priority->elem);
  return sema_elem_max_priority;
};

void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  // es-abdelrahman sema_up should pop the highest priority each thread is wrapped inside a different semaphore
  if (!list_empty (&cond->waiters)) 
    sema_up (&sema_with_highest_priority(&cond->waiters)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
