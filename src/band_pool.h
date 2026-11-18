/***************************************************************************

    band_pool.h

    The workers that draw a frame in bands for the "Threaded Video" core
    option: band 0 on the calling thread, band i on worker i.  Lock-free:
    the job is published with one release store, the workers retire their
    bands with one read-modify-write each, and both sides park on
    libretro-common eventcounts when there is nothing to do.

***************************************************************************/

#ifndef BAND_POOL_H
#define BAND_POOL_H

#ifdef HAVE_THREADS
#include <retro_atomic.h>
#if defined(RETRO_ATOMIC_LOCK_FREE)
#define BAND_POOL_THREADS 1
#endif
#endif

#ifdef BAND_POOL_THREADS
#include <boolean.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

#define BAND_POOL_MAX	4

typedef void (*band_pool_job_t)(void *user, int band, int nbands);

struct band_pool;

struct band_pool_worker
{
	struct band_pool *pool;
	sthread_t *thread;
	int index;
	int seen;		/* the last job word this worker took */
};

typedef struct band_pool
{
	retro_eventcount_t start;	/* a job word was published, or quit */
	retro_eventcount_t done;	/* the last worker band was drawn */
	struct band_pool_worker worker[BAND_POOL_MAX];
	band_pool_job_t job;
	void *user;
	retro_atomic_int_t cmd;		/* (sequence << 3) | bands */
	retro_atomic_int_t pending;	/* worker bands not drawn yet */
	retro_atomic_int_t quit;
	int workers;			/* threads started */
	int seq;
	bool ready;
} band_pool_t;

/* the bands the pool can draw (workers started as needed), 0 if fewer than 2 */
int band_pool_prepare(band_pool_t *p, int bands);

/* draws band 0 here and the others on the workers; returns when all are drawn */
void band_pool_run(band_pool_t *p, int bands, band_pool_job_t job, void *user);

void band_pool_stop(band_pool_t *p);
#endif

#endif
