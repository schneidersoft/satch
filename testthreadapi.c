// Compile 'testthreadapi.c' against the library and run it to check that
// the library is reentrant, i.e., that solver instances used concurrently
// in different threads do not interfere.  Build with threads enabled and
// ideally with '-fsanitize=thread' ('./configure --tsan') to detect data
// races between solver instances.

#include "satch.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#undef NDEBUG
#include <assert.h>

#define THREADS 8
#define RUNS 4

static void *
run_thread (void *ptr)
{
  const int id = (int) (intptr_t) ptr;
  const int kind = id % THREADS;

  struct satch *solver = satch_init ();

  // Half of the instances produce verbose output to check that messages
  // of concurrent solvers are printed atomically per line.

  if (kind % 2)
    satch_set_verbose_level (solver, 1);

  int res;

  switch (kind % 4)
    {
    case 0:			// Satisfiable with forced assignments.

      satch_add (solver, 1), satch_add (solver, 2), satch_add (solver, 0);
      satch_add (solver, 1), satch_add (solver, -2), satch_add (solver, 0);
      satch_add (solver, -1), satch_add (solver, 2), satch_add (solver, 0);
      res = satch_solve (solver, -1);
      assert (res == SATISFIABLE);
      assert (satch_val (solver, 1) == 1);
      assert (satch_val (solver, 2) == 2);
      break;

    case 1:			// Unsatisfiable (pigeonhole).

      for (int r = -1; r <= 1; r += 2)
        for (int s = -1; s <= 1; s += 2)
          for (int t = -1; t <= 1; t += 2)
            satch_add (solver, r * 1), satch_add (solver, s * 2),
              satch_add (solver, t * 3), satch_add (solver, 0);
      res = satch_solve (solver, -1);
      assert (res == UNSATISFIABLE);
      break;

    case 2:			// Unsatisfiable incrementally with limits.

      for (int i = 1; i < 10; i++)
        {
          satch_add (solver, i), satch_add (solver, 0);
          satch_add (solver, -i), satch_add (solver, 0);
        }
      {
        int limit = 0;
        while (!(res = satch_solve (solver, limit)))
          limit++;
        assert (res == UNSATISFIABLE);
      }
      break;

    default:			// Satisfiable long chain.

      assert (kind % 4 == 3);
      satch_reserve (solver, 100);
      for (int i = 1; i < 100; i++)
        {
          satch_add (solver, i), satch_add (solver, i + 1),
            satch_add (solver, 0);
          satch_add (solver, -i), satch_add (solver, i + 1),
            satch_add (solver, 0);
        }
      satch_add (solver, -1), satch_add (solver, 0);
      res = satch_solve (solver, -1);
      assert (res == SATISFIABLE);
      assert (satch_val (solver, 100) == 100);
      break;
    }

  if (res != SATISFIABLE && res != UNSATISFIABLE)
    return (void *) (intptr_t) 1;

  satch_release (solver);
  return 0;
}

int
main (void)
{
  pthread_t threads[THREADS * RUNS];

  for (int i = 0; i < THREADS * RUNS; i++)
    {
      const int error = pthread_create (threads + i, 0, run_thread,
                                        (void *) (intptr_t) i);
      if (error)
        {
          fputs ("error: pthread_create failed\n", stderr);
          exit (1);
        }
    }

  for (int i = 0; i < THREADS * RUNS; i++)
    {
      void *retval;
      const int error = pthread_join (threads[i], &retval);
      if (error)
        {
          fputs ("error: pthread_join failed\n", stderr);
          exit (1);
        }
      if (retval)
        {
          fputs ("error: solver status check failed\n", stderr);
          exit (1);
        }
    }

  return 0;
}
