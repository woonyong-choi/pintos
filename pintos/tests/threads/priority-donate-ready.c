/* Donates to a lock holder that is sitting in the ready list rather
   than running.

   The ready list is kept in priority order and next_thread_to_run()
   pops its front without looking any further, so raising the priority
   of a thread that is already queued there has to move the thread as
   well.  Writing the new priority in place leaves the queue out of
   order and the scheduler keeps preferring the lower-priority thread
   -- exactly the inversion donation exists to prevent.

   Every other priority-donate test donates to the *running* thread,
   so none of them can observe this.

   Timeline:
     main  (31) takes the lock, then creates mid, which preempts it;
                main is now queued, still holding the lock.
     mid   (33) creates donor, which preempts it; mid is queued too,
                ahead of main.
     donor (35) blocks on the lock and donates 35 to main.
                The queue now holds mid(33) before main(35).
     The scheduler must pick main, not mid. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static struct lock lock;

/* Hands out 1, 2, ... in the order threads get the CPU back. */
static volatile int order;
static volatile int main_turn;
static volatile int mid_turn;

static thread_func mid_thread_func;
static thread_func donor_thread_func;

void
test_priority_donate_ready (void)
{
  int donated;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  order = main_turn = mid_turn = 0;

  lock_acquire (&lock);
  thread_create ("mid", PRI_DEFAULT + 2, mid_thread_func, NULL);

  /* We are back on the CPU: mid and donor have both run. */
  main_turn = ++order;
  donated = thread_get_priority ();

  lock_release (&lock);

  /* donor ran on the lock release and mid has finished by now. */
  msg ("priority donated to the queued holder: %d", donated);
  msg ("holder scheduled before the lower-priority thread: %s",
       main_turn < mid_turn ? "true" : "false");
}

static void
mid_thread_func (void *aux UNUSED)
{
  thread_create ("donor", PRI_DEFAULT + 4, donor_thread_func, NULL);
  mid_turn = ++order;
}

static void
donor_thread_func (void *aux UNUSED)
{
  lock_acquire (&lock);
  lock_release (&lock);
}
