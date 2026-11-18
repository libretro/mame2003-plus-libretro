/***************************************************************************

    band_pool.c

    The job word carries the bands with a sequence number, so a worker
    reads every parameter of a job behind one acquire load and a worker
    that sits a job out reads nothing else.  The calling thread changes
    the job and user fields only once every worker of the previous job
    has retired its band.

***************************************************************************/

#include "band_pool.h"

#ifdef BAND_POOL_THREADS

static void band_pool_worker_main(void *arg)
{
	struct band_pool_worker *w = (struct band_pool_worker *)arg;
	band_pool_t *p = w->pool;
	int seen = w->seen;

	for (;;)
	{
		int cmd, bands;

		for (;;)
		{
			int key;
			cmd = retro_atomic_load_acquire_int(&p->cmd);
			if (cmd != seen || retro_atomic_load_acquire_int(&p->quit))
				break;
			key = retro_eventcount_prepare_wait(&p->start);
			cmd = retro_atomic_load_acquire_int(&p->cmd);
			if (cmd != seen || retro_atomic_load_acquire_int(&p->quit))
			{
				retro_eventcount_cancel_wait(&p->start);
				break;
			}
			retro_eventcount_commit_wait(&p->start, key);
		}
		if (retro_atomic_load_acquire_int(&p->quit))
			break;

		seen = cmd;
		bands = cmd & 7;
		if (w->index < bands)
		{
			p->job(p->user, w->index, bands);
			if (retro_atomic_fetch_sub_int(&p->pending, 1) == 1)
				retro_eventcount_notify(&p->done);
		}
	}
	retro_eventcount_thread_cleanup();
}

int band_pool_prepare(band_pool_t *p, int bands)
{
	if (bands > BAND_POOL_MAX)
		bands = BAND_POOL_MAX;
	if (bands < 2)
		return 0;
	if (!p->ready)
	{
		if (!retro_eventcount_init(&p->start))
			return 0;
		if (!retro_eventcount_init(&p->done))
		{
			retro_eventcount_free(&p->start);
			return 0;
		}
		retro_atomic_int_init(&p->cmd, 0);
		retro_atomic_int_init(&p->pending, 0);
		retro_atomic_int_init(&p->quit, 0);
		p->ready = true;
	}
	while (p->workers < bands - 1)
	{
		struct band_pool_worker *w = &p->worker[p->workers + 1];
		w->pool = p;
		w->index = p->workers + 1;
		w->seen = retro_atomic_load_relaxed_int(&p->cmd);
		w->thread = sthread_create(band_pool_worker_main, w);
		if (!w->thread)
			break;
		p->workers++;
	}
	if (bands > p->workers + 1)
		bands = p->workers + 1;
	return bands < 2 ? 0 : bands;
}

void band_pool_run(band_pool_t *p, int bands, band_pool_job_t job, void *user)
{
	p->job = job;
	p->user = user;
	retro_atomic_store_relaxed_int(&p->pending, bands - 1);
	p->seq = (p->seq + 1) & 0x0fffffff;
	retro_atomic_store_release_int(&p->cmd, (p->seq << 3) | bands);
	retro_eventcount_notify(&p->start);

	job(user, 0, bands);

	for (;;)
	{
		int key;
		if (retro_atomic_load_acquire_int(&p->pending) == 0)
			break;
		key = retro_eventcount_prepare_wait(&p->done);
		if (retro_atomic_load_acquire_int(&p->pending) == 0)
		{
			retro_eventcount_cancel_wait(&p->done);
			break;
		}
		retro_eventcount_commit_wait(&p->done, key);
	}
}

void band_pool_stop(band_pool_t *p)
{
	int i;
	if (!p->ready)
		return;
	retro_atomic_store_release_int(&p->quit, 1);
	retro_eventcount_notify(&p->start);
	for (i = 1; i < p->workers + 1; i++)
	{
		sthread_join(p->worker[i].thread);
		p->worker[i].thread = NULL;
	}
	retro_eventcount_free(&p->start);
	retro_eventcount_free(&p->done);
	retro_eventcount_thread_cleanup();
	retro_atomic_store_relaxed_int(&p->cmd, 0);
	retro_atomic_store_relaxed_int(&p->pending, 0);
	retro_atomic_store_relaxed_int(&p->quit, 0);
	p->job = NULL;
	p->user = NULL;
	p->workers = 0;
	p->seq = 0;
	p->ready = false;
}

#else
/* ISO C forbids an empty translation unit */
typedef int band_pool_unused;
#endif
