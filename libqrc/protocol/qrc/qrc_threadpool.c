/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "qrc.h"

#ifdef QRC_MCB
#  define QRC_THREAD_PRIORITY  (SCHED_PRIORITY_DEFAULT)
#  define QRC_THREAD_STACKSIZE (1024 * 6)
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* semaphore   */
struct work_sem_s
{
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
  int             value;
};

/* qrc work */
struct qrc_work_s
{
  struct qrc_work_s *      previous;
  qrc_work                 work_fun;
  struct qrc_msg_cb_args_s args;
};

/* qrc work queue */
struct qrc_workqueue_s
{
  pthread_mutex_t    queue_mutex;
  struct qrc_work_s *work_front;
  struct qrc_work_s *work_rear;
  struct work_sem_s *work_sem;
  int                len;
};

/* qrc thread */
struct qrc_thread_s
{
  int                       id;
  pthread_t                 pthread;
  struct qrc_thread_pool_s *qrc_tp;
};

/* qrc thread pool */
struct qrc_thread_pool_s
{
  struct qrc_thread_s ** threads; /* thread list */
  volatile int           num_threads_alive;
  volatile int           num_threads_working;
  pthread_mutex_t        thread_count_lock;
  pthread_cond_t         threads_all_idle;
  struct qrc_workqueue_s workqueue;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int   thread_init(struct qrc_thread_pool_s *qrc_tp, struct qrc_thread_s **threads, int id);
static void *thread_run(struct qrc_thread_s *qrc_thread);
static void  thread_hold(int sig_id);
static void  thread_destroy(struct qrc_thread_s *qrc_thread);

static int                workqueue_init(struct qrc_workqueue_s *workqueue);
static void               workqueue_clear(struct qrc_workqueue_s *workqueue);
static void               workqueue_push(struct qrc_workqueue_s *workqueue, struct qrc_work_s *work);
static struct qrc_work_s *workqueue_pull(struct qrc_workqueue_s *workqueue);
static void               workqueue_destroy(struct qrc_workqueue_s *workqueue_p);

static void work_sem_init(struct work_sem_s *sem, int value);
static void work_sem_reset(struct work_sem_s *sem);
static void work_sem_post(struct work_sem_s *sem);
static void work_sem_post_all(struct work_sem_s *sem);
static void work_sem_wait(struct work_sem_s *sem);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile int g_threads_keepalive;
static volatile int g_threads_on_hold;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int thread_init(struct qrc_thread_pool_s *qrc_tp, struct qrc_thread_s **threads, int id)
{
  *threads = (struct qrc_thread_s *)malloc(sizeof(struct qrc_thread_s));
  if (*threads == NULL)
    {
      printf("thread_init(): Could not allocate memory for thread\n");
      return -1;
    }

  (*threads)->qrc_tp = qrc_tp;
  (*threads)->id     = id;

#ifdef QRC_MCB
  pthread_attr_t     attr;
  struct sched_param sparam;
  int                status;

  status = pthread_attr_init(&attr);
  if (status != 0)
    {
      printf("thread_init: ERROR pthread_attr_init failed, status=%d\n",
             status);
      ASSERT(false);
    }

  status = pthread_attr_setstacksize(&attr, QRC_THREAD_STACKSIZE);
  if (status != 0)
    {
      printf("thread_init: "
             "ERROR pthread_attr_setstacksize failed, status=%d\n",
             status);
      ASSERT(false);
    }

  sparam.sched_priority = QRC_THREAD_PRIORITY;
  status                = pthread_attr_setschedparam(&attr, &sparam);
  if (status != 0)
    {
      printf("thread_init: "
             "ERROR pthread_attr_setschedparam failed, status=%d\n",
             status);
      ASSERT(false);
    }

  status = pthread_create(&(*threads)->pthread, &attr, (void *(*)(void *))thread_run, (*threads));
  if (status != 0)
    {
      printf("thread_init: "
             "ERROR pthread_create failed, status=%d\n",
             status);
      ASSERT(false);
    }
#else
  pthread_create(&(*threads)->pthread, NULL, (void *(*)(void *))thread_run, (*threads));
#endif

  // pthread_detach((*threads)->pthread);
  return 0;
}

/* Sets the calling thread on hold */
static void thread_hold(int sig_id)
{
  (void)sig_id;
  g_threads_on_hold = 1;
  while (g_threads_on_hold)
    {
      sleep(1);
    }
}

static void *thread_run(struct qrc_thread_s *qrc_thread)
{
  struct qrc_thread_pool_s *qrc_tp = qrc_thread->qrc_tp;
  struct sigaction          act;

  sigemptyset(&act.sa_mask);
  act.sa_flags   = SA_ONSTACK;
  act.sa_handler = thread_hold;
  if (sigaction(SIGUSR1, &act, NULL) == -1)
    {
      printf("thread_run(): cannot handle SIGUSR1");
    }

  pthread_mutex_lock(&qrc_tp->thread_count_lock);
  qrc_tp->num_threads_alive += 1;
  pthread_mutex_unlock(&qrc_tp->thread_count_lock);
  while (g_threads_keepalive)
    {
      work_sem_wait(qrc_tp->workqueue.work_sem);
      if (g_threads_keepalive)
        {
          pthread_mutex_lock(&qrc_tp->thread_count_lock);
          qrc_tp->num_threads_working++;
          pthread_mutex_unlock(&qrc_tp->thread_count_lock);

          /* execute qrc function */
          qrc_work                  work_fun;
          struct qrc_msg_cb_args_s *args;

          struct qrc_work_s *work_p = workqueue_pull(&qrc_tp->workqueue);
          if (work_p)
            {
              work_fun = work_p->work_fun;
              args     = &work_p->args;
              work_fun(*args);
              free(work_p);
            }

          pthread_mutex_lock(&qrc_tp->thread_count_lock);
          qrc_tp->num_threads_working--;
          if (!qrc_tp->num_threads_working)
            {
              pthread_cond_signal(&qrc_tp->threads_all_idle);
            }
          pthread_mutex_unlock(&qrc_tp->thread_count_lock);
        }
    }
  pthread_mutex_lock(&qrc_tp->thread_count_lock);
  qrc_tp->num_threads_alive--;
  pthread_mutex_unlock(&qrc_tp->thread_count_lock);
#ifdef QRC_MCB
  ASSERT(false);
#endif
  return NULL;
}

static void thread_destroy(struct qrc_thread_s *qrc_thread)
{
  free(qrc_thread);
}

static int workqueue_init(struct qrc_workqueue_s *workqueue)
{
  workqueue->len        = 0;
  workqueue->work_front = NULL;
  workqueue->work_rear  = NULL;

  workqueue->work_sem = (struct work_sem_s *)malloc(sizeof(struct work_sem_s));
  if (workqueue->work_sem == NULL)
    {
      return -1;
    }

  pthread_mutex_init(&(workqueue->queue_mutex), NULL);
  work_sem_init(workqueue->work_sem, 0);

  return 0;
}

static void workqueue_clear(struct qrc_workqueue_s *workqueue)
{

  while (workqueue->len)
    {
      free(workqueue_pull(workqueue));
    }

  workqueue->work_front = NULL;
  workqueue->work_rear  = NULL;
  work_sem_reset(workqueue->work_sem);
  workqueue->len = 0;
}

static void workqueue_push(struct qrc_workqueue_s *workqueue, struct qrc_work_s *work)
{

  pthread_mutex_lock(&workqueue->queue_mutex);
  work->previous = NULL;

  switch (workqueue->len)
    {

        case 0: {
          workqueue->work_front = work;
          workqueue->work_rear  = work;
          break;
        }

        default: {
          workqueue->work_rear->previous = work;
          workqueue->work_rear           = work;
        }
    }
  workqueue->len++;

  work_sem_post(workqueue->work_sem);
  pthread_mutex_unlock(&workqueue->queue_mutex);
}

static struct qrc_work_s *workqueue_pull(struct qrc_workqueue_s *workqueue)
{

  pthread_mutex_lock(&workqueue->queue_mutex);
  struct qrc_work_s *work_p = workqueue->work_front;

  switch (workqueue->len)
    {
        case 0: {
          break;
        }
        case 1: {
          workqueue->work_front = NULL;
          workqueue->work_rear  = NULL;
          workqueue->len        = 0;
          break;
        }
        default: {
          workqueue->work_front = work_p->previous;
          workqueue->len--;
          work_sem_post(workqueue->work_sem);
        }
    }

  pthread_mutex_unlock(&workqueue->queue_mutex);
  return work_p;
}

static void workqueue_destroy(struct qrc_workqueue_s *workqueue)
{
  workqueue_clear(workqueue);
  free(workqueue->work_sem);
}

static void work_sem_init(struct work_sem_s *sem, int value)
{
  if (value < 0 || value > 1)
    {
      printf("work_sem_init(): value invalid\n");
      exit(1);
    }
  pthread_mutex_init(&(sem->mutex), NULL);
  pthread_cond_init(&(sem->cond), NULL);
  sem->value = value;
}

static void work_sem_reset(struct work_sem_s *sem)
{
  pthread_mutex_destroy(&(sem->mutex));
  pthread_cond_destroy(&(sem->cond));
  work_sem_init(sem, 0);
}

static void work_sem_post(struct work_sem_s *sem)
{
  pthread_mutex_lock(&sem->mutex);
  sem->value = 1;
  pthread_cond_signal(&sem->cond);
  pthread_mutex_unlock(&sem->mutex);
}

static void work_sem_post_all(struct work_sem_s *sem)
{
  pthread_mutex_lock(&sem->mutex);
  sem->value = 1;
  pthread_cond_broadcast(&sem->cond);
  pthread_mutex_unlock(&sem->mutex);
}

static void work_sem_wait(struct work_sem_s *sem)
{
  pthread_mutex_lock(&sem->mutex);
  while (sem->value != 1)
    {
      pthread_cond_wait(&sem->cond, &sem->mutex);
    }
  sem->value = 0;
  pthread_mutex_unlock(&sem->mutex);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* Initialise thread pool */
struct qrc_thread_pool_s *qrc_thread_pool_init(int num)
{
  int n;

  g_threads_on_hold   = 0;
  g_threads_keepalive = 1;

  if (num < 0)
    {
      num = 0;
    }

  struct qrc_thread_pool_s *thpool;
  thpool = (struct qrc_thread_pool_s *)malloc(sizeof(struct qrc_thread_pool_s));
  if (thpool == NULL)
    {
      printf("qrc_thread_pool_init(): Could not allocate memory for thread pool\n");
      return NULL;
    }
  thpool->num_threads_alive   = 0;
  thpool->num_threads_working = 0;

  /* Initialise the work queue */
  if (workqueue_init(&thpool->workqueue) == -1)
    {
      printf("qrc_thread_pool_init(): Could not allocate memory for work queue\n");
      free(thpool);
      return NULL;
    }

  /* Make threads in pool */
  thpool->threads = (struct qrc_thread_s **)malloc(num * sizeof(struct qrc_thread_s *));
  if (thpool->threads == NULL)
    {
      printf("qrc_thread_pool_init(): Could not allocate memory for threads\n");
      workqueue_destroy(&thpool->workqueue);
      free(thpool);
      return NULL;
    }

  pthread_mutex_init(&(thpool->thread_count_lock), NULL);
  pthread_cond_init(&thpool->threads_all_idle, NULL);

  /* Thread init */
  for (n = 0; n < num; n++)
    {
      thread_init(thpool, &thpool->threads[n], n);
    }

  /* Wait for threads to initialize */
  while (thpool->num_threads_alive != num)
    {
      sleep(1);
      printf("wait thread_alive \n");
    }

  return thpool;
}

/* Add work to the thread pool */
int qrc_threadpool_add_work(struct qrc_thread_pool_s *thpool, qrc_work work_fun, struct qrc_msg_cb_args_s args)
{
  struct qrc_work_s *newwork;

  newwork = (struct qrc_work_s *)malloc(sizeof(struct qrc_work_s));
  if (newwork == NULL)
    {
      printf("qrc_threadpool_add_work(): Could not allocate memory for new work\n");
      return -1;
    }

  /* add function and argument */
  newwork->work_fun = work_fun;
  memcpy(&newwork->args, &args, sizeof(struct qrc_msg_cb_args_s));
  /* add work to queue */
  workqueue_push(&thpool->workqueue, newwork);

  return 0;
}

void qrc_threadpool_wait(struct qrc_thread_pool_s *thpool)
{
  pthread_mutex_lock(&thpool->thread_count_lock);
  while (thpool->workqueue.len || thpool->num_threads_working)
    {
      pthread_cond_wait(&thpool->threads_all_idle, &thpool->thread_count_lock);
    }
  pthread_mutex_unlock(&thpool->thread_count_lock);
}

/* Destroy the threadpool */
void qrc_threadpool_destroy(struct qrc_thread_pool_s *thpool)
{
  /* No need to destroy if it's NULL */
  if (thpool == NULL)
    return;

  volatile int threads_total = thpool->num_threads_alive;

  /* End each thread 's infinite loop */
  g_threads_keepalive = 0;

  /* Give one second to kill idle threads */
  double TIMEOUT = 1.0;
  time_t start, end;
  double tpassed = 0.0;
  time(&start);
  while (tpassed < TIMEOUT && thpool->num_threads_alive)
    {
      work_sem_post_all(thpool->workqueue.work_sem);
      time(&end);
      tpassed = difftime(end, start);
    }

  /* Poll remaining threads */
  while (thpool->num_threads_alive)
    {
      work_sem_post_all(thpool->workqueue.work_sem);
      sleep(2);
    }

  /* work queue cleanup */
  workqueue_destroy(&thpool->workqueue);
  /* Deallocs */
  int n;
  for (n = 0; n < threads_total; n++)
    {
      thread_destroy(thpool->threads[n]);
    }
  free(thpool->threads);
  free(thpool);
}

void qrc_threads_join(struct qrc_thread_pool_s *thpool)
{
  pthread_t *threads;
  int        i;
  int        thread_num = thpool->num_threads_alive;

  threads = (pthread_t *)malloc(thread_num * sizeof(pthread_t));

  for (i = 0; i < thread_num; i++)
    {
      threads[i] = thpool->threads[0]->pthread;
    }

  for (i = 0; i < thread_num; i++)
    {
      pthread_join(threads[i], NULL);
      printf(" qrc_threads_join =%d \n", i);
      sleep(1);
    }
  free(threads);
}
