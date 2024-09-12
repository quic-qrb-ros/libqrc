/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/
#include "qrc.h"
#include "qti_qrc_udriver.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define QRC_MSG_TIME_OUT_MS (500) /* ms */
#define QRC_MSG_TIME_OUT_S  (4)   /* s */
#define MAX_PIPE_ID         (64)

#define MCB_RESET_MAGIC_CMD    0x7102
#define DEFAULT_TF_MSG_TYPE    0x22
#define QRC_HW_SYNC_MSG        "OK"
#define QRC_CONTROL_THREAD_NUM (1) /* must be single thread for mutex */

struct qrc_s
{
  pthread_mutex_t   pipe_list_mutex;
  pthread_mutex_t   qrc_write_mutex;
  struct qrc_pipe_s pipe_list[MAX_PIPE_ID];
  int               fd;
  TinyFrame *       tf;
  qrc_thread_pool   msg_threadpool;
  qrc_thread_pool   control_threadpool;
  uint8_t           pipe_cnt;
  volatile bool     peer_pipe_list_ready;

  /* used for bus timeout */
  pthread_cond_t  bus_lock_cond;
  pthread_mutex_t bus_lock_mutex;
  volatile bool   is_bus_timeout_busy; /* true: bus lock in use */

  pthread_t read_thread;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct qrc_s g_qrc;

#ifdef QRC_RB5
#  define QRC_THREAD_NUM (2)
#  define QRC_IOC_MAGIC  'q'
#  define QRC_FIONREAD   _IO(QRC_IOC_MAGIC, 5)
#  define QRC_RESET_MCB  _IO(QRC_IOC_MAGIC, 2)
#  define QRC_FD         ("/dev/ttyHS1")
#  define QRC_BOOT_APP   '2'
#  define QRC_GPIOCHIP   ("/dev/gpiochip0")
#  define QRC_RESETGPIO  168
#  define QRC_MAX_READ_SIZE  1024
#endif

#ifdef QRC_RB3
#  define QRC_THREAD_NUM (2)
#  define QRC_IOC_MAGIC  'q'
#  define QRC_FIONREAD   _IO(QRC_IOC_MAGIC, 5)
#  define QRC_RESET_MCB  _IO(QRC_IOC_MAGIC, 2)
#  define QRC_FD         ("/dev/ttyHS2")
#  define QRC_BOOT_APP   '2'
#  define QRC_GPIOCHIP   ("/dev/gpiochip4")
#  define QRC_RESETGPIO  147
#  define QRC_MAX_READ_SIZE  1024
#endif

#ifdef QRC_MCB
#  define QRC_THREAD_NUM (2)
#  define QRC_FD         ("/dev/ttyS2")
#  define QRC_FIONREAD   FIONREAD
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/
void             TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);
static TF_Result read_response_listener(TinyFrame *tf, TF_Msg *msg);
static void *    read_thread(void *args);
static void      qrc_control_pipe_callback(qrc_pipe_s *pipe, void *data, size_t len, bool response);
static void      stop_pipe_timeout(const uint8_t pipe_id);
static void      qrc_msg_cb_work(struct qrc_msg_cb_args_s args);
static int       qrc_hardware_sync(int qrc_fd);

static void qrc_lock_stop_timeout(void);
static int  qrc_lock_start_timeout(bool *timeout);

/****************************************************************************
 * @intro: send TF frame
 * @param tf: tf
 * @param buff: TF frame
 * @param len: length of buff
 ****************************************************************************/
void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
  uint32_t write_cnt = 0;
#if defined(QRC_RB5) || defined(QRC_RB3)
  write_cnt          = qrc_udriver_write(g_qrc.fd, buff, len);
#else // QRC_MCB
  write_cnt          = write(g_qrc.fd, buff, len);
#endif
  if (write_cnt != len)
    {
      printf("ERROR: Write failed!\n");
    }
}

/****************************************************************************
 * @intro: for connection establishment of new pipe, can be only used by pipe_list[0]
 * @param pipe_name: pipe name of caller
 * @param pipe_id: pipe id of caller
 * @param cmd: enum qrc_msg_cmd
 * @return: result of TF_Send()
 ****************************************************************************/
bool qrc_control_write(const struct qrc_pipe_s *pipe, const uint8_t pipe_id, const enum qrc_msg_cmd cmd)
{
  bool      timeout;
  bool      send_result;
  qrc_frame qrcf;
  qrcf.receiver_id = 0;
  qrcf.ack         = NO_ACK;

  qrc_msg msg;
  msg.cmd     = cmd;
  msg.pipe_id = pipe_id;

  if (NULL == pipe)
    {
      printf("ERROR: qrc_control_write pipe is invalid\n");
      return false;
    }

  if (QRC_REQUEST == cmd || QRC_RESPONSE == cmd) /* apply new pipe */
    {
      memset(msg.pipe_name, '\0', 10);
      memcpy(msg.pipe_name, pipe->pipe_name, strlen(pipe->pipe_name) * sizeof(char));
    }

  send_result = qrc_frame_send(&qrcf, (uint8_t *)(&msg), sizeof(qrc_msg), true);
  if (!send_result)
    {
      printf("ERROR: qrc_control_write send msg failed\n");
      return false;
    }

  /* do cmd action */
  switch (cmd)
    {
      case QRC_REQUEST:
        case QRC_CONNECT_REQUEST: {
          /* start control pipe timeout */
          if (QRC_OK != start_pipe_timeout(QRC_CONTROL_PIPE_ID, &timeout))
            {
              printf("ERROR: qrc_control_write  timeout failed\n");
              return false;
            }
          return !timeout;
        }
      case QRC_WRITE_LOCK:
        case QRC_WRITE_UNLOCK: {
          /* start bus lock timeout */
          if (QRC_OK != qrc_lock_start_timeout(&timeout))
            {
              return false;
            }
          return !timeout;
        }
      case QRC_ACK:
      case QRC_RESPONSE:
      case QRC_WRITE_LOCK_ACK:
      case QRC_WRITE_UNLOCK_ACK:
      case QRC_CONNECT_RESPONSE:
      default:;
    }

  return true;
}

/****************************************************************************
 * @intro: this function will be called when tf receive msg
 * @param tf: receiver
 * @param msg: msg received
 * @return: TF_STAY
 ****************************************************************************/
static TF_Result read_response_listener(TinyFrame *tf, TF_Msg *msg)
{
  qrc_frame                qrcf;
  struct qrc_msg_cb_args_s args;

  memcpy(&qrcf, msg->data, sizeof(qrc_frame));

  qrc_pipe_s *p = qrc_pipe_find_by_pipeid(qrcf.receiver_id);
  if (NULL == p)
    {
      printf("ERROR: here is no pipe with peer pipe id %u, receive failed!\n", qrcf.receiver_id);
    }
  else
    {
      if (NULL != p->cb)
        {
          args.fun_cb = p->cb;
          args.pipe   = p;
          args.len    = msg->len - sizeof(qrc_frame);
          args.data   = (uint8_t *)malloc(args.len);
          memcpy(args.data, msg->data + sizeof(qrc_frame), args.len);
          args.response = false;
          args.need_ack = qrcf.ack;
          if (p->pipe_id == QRC_CONTROL_PIPE_ID)
            {
              qrc_threadpool_add_work(g_qrc.control_threadpool, qrc_msg_cb_work, args);
            }
          else
            {
              qrc_threadpool_add_work(g_qrc.msg_threadpool, qrc_msg_cb_work, args);
            }
        }
    }

  return TF_STAY;
}

/****************************************************************************
 * @intro: callback function of pipelist[0], whose pipe id is 0
 * @param pipe: pipelist[0]
 * @param data: data received
 * @param len: length of data
 * @param response: no use
 ****************************************************************************/
static void qrc_control_pipe_callback(qrc_pipe_s *pipe, void *data, size_t len, bool response)
{
  qrc_msg qmsg;
  memcpy(&qmsg, data, sizeof(qrc_msg));
  uint8_t cmd           = qmsg.cmd;
  uint8_t pipe_id       = qmsg.pipe_id; /*pipe id of receiver*/
  char    pipe_name[10] = "\0";
  memcpy(pipe_name, qmsg.pipe_name, 10);

  switch (cmd)
    {
        case QRC_REQUEST: {
          qrc_pipe_s *p = qrc_pipe_insert(pipe_name);
          if (p == NULL)
            {
              printf("ERROR: corresponding pipe(%s) create failed!\n", pipe_name);
            }
          else
            {
              p->peer_pipe_id = pipe_id;
              p->pipe_ready   = true;
              qrc_control_write(p, p->pipe_id, QRC_RESPONSE);
            }
          break;
        }
        case QRC_RESPONSE: {
          qrc_pipe_s *p = qrc_pipe_find_by_name(pipe_name);
          if (p == NULL)
            {
              printf("ERROR: pipe name(%s) doesn't exit, can not handle QRC_RESPONSE!\n", pipe_name);
            }
          else
            {
              p->peer_pipe_id = pipe_id;
              p->pipe_ready   = true;
              stop_pipe_timeout(QRC_CONTROL_PIPE_ID);
            }
          break;
        }
        case QRC_WRITE_LOCK: {
          qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
          qrc_control_write(p, p->pipe_id, QRC_WRITE_LOCK_ACK);
          qrc_bus_lock();
          break;
        }
        case QRC_WRITE_UNLOCK: {
          qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
          qrc_bus_unlock();
          qrc_control_write(p, p->pipe_id, QRC_WRITE_UNLOCK_ACK);
          break;
        }
        case QRC_ACK: {
          qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
          stop_pipe_timeout(p->pipe_id); /*pipe_id == user id*/
          break;
        }
        case QRC_WRITE_LOCK_ACK: {
          qrc_lock_stop_timeout();
          break;
        }
        case QRC_WRITE_UNLOCK_ACK: {
          qrc_lock_stop_timeout();
          break;
        }
        case QRC_CONNECT_REQUEST: {
          qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
          qrc_control_write(p, QRC_CONTROL_PIPE_ID, QRC_CONNECT_RESPONSE);
          break;
        }
        case QRC_CONNECT_RESPONSE: {
          g_qrc.peer_pipe_list_ready = true;
          stop_pipe_timeout(QRC_CONTROL_PIPE_ID);
          break;
        }
      default:
        printf("WARNING: qrc_control_pipe_callback cmd=%d is invalid\n", cmd);
        break;
    }
}

/****************************************************************************
 * @intro: initilize a new pipe
 * @return: new pipe
 ****************************************************************************/
qrc_pipe_s qrc_pipe_init(void)
{
  qrc_pipe_s pipe;
  memset(pipe.pipe_name, '\0', 10);

  if (0 != pthread_cond_init(&pipe.pipe_cond, NULL))
    {
      printf("\nERROR: pipe cond initalize failed!\n");
      return pipe;
    }
  if (0 != pthread_mutex_init(&pipe.pipe_mutex, NULL))
    {
      printf("\nERROR: pipe mutex initalize failed!\n");
      return pipe;
    }
  pipe.pipe_id              = 255;
  pipe.peer_pipe_id         = 255;
  pipe.is_pipe_timeout_busy = false;
  pipe.cb                   = NULL;
  pipe.pipe_ready           = false;

  return pipe;
}

/****************************************************************************
 * @intro: initilize the pipe list
 ****************************************************************************/
bool qrc_pipe_list_init(void)
{
  bool res;

  pthread_mutex_lock(&g_qrc.pipe_list_mutex);

  /* init qrc control pipe */
  g_qrc.pipe_list[QRC_CONTROL_PIPE_ID]              = qrc_pipe_init();
  g_qrc.pipe_list[QRC_CONTROL_PIPE_ID].pipe_id      = QRC_CONTROL_PIPE_ID;
  g_qrc.pipe_list[QRC_CONTROL_PIPE_ID].peer_pipe_id = QRC_CONTROL_PIPE_ID;
  char *pipe_name                                   = "QRC_CTL";

  memcpy(g_qrc.pipe_list[QRC_CONTROL_PIPE_ID].pipe_name, pipe_name, strlen(pipe_name) * sizeof(char));
  g_qrc.pipe_list[QRC_CONTROL_PIPE_ID].cb = qrc_control_pipe_callback;
  g_qrc.pipe_cnt                          = 1;

  pthread_mutex_unlock(&g_qrc.pipe_list_mutex);

  /* send connect request to peer for shake hands */
  res = qrc_control_write(&g_qrc.pipe_list[QRC_CONTROL_PIPE_ID], QRC_CONTROL_PIPE_ID, QRC_CONNECT_REQUEST);

  return res;
}

/****************************************************************************
 * @intro: create a new pipe named pipe_name
 * @return: pointer of new pipe or exited pipe
 ****************************************************************************/
qrc_pipe_s *qrc_pipe_insert(const char *pipe_name)
{
  pthread_mutex_lock(&g_qrc.pipe_list_mutex);
  qrc_pipe_s *lt = g_qrc.pipe_list;
  if (g_qrc.pipe_cnt >= 63)
    {
      pthread_mutex_unlock(&g_qrc.pipe_list_mutex);
      return NULL;
    }
  qrc_pipe_s *find_res = qrc_pipe_find_by_name(pipe_name);
  if (NULL == find_res)
    {
      uint8_t new_pipe_index     = g_qrc.pipe_cnt;
      g_qrc.pipe_cnt             = (g_qrc.pipe_cnt + 1) % 64;
      lt[new_pipe_index]         = qrc_pipe_init();
      lt[new_pipe_index].pipe_id = new_pipe_index;
      //debug
      lt[new_pipe_index].peer_pipe_id = new_pipe_index;
      memcpy(lt[new_pipe_index].pipe_name, pipe_name, strlen(pipe_name) * sizeof(char));
      find_res = &lt[new_pipe_index];
    }
  pthread_mutex_unlock(&g_qrc.pipe_list_mutex);
  return find_res;
}

/****************************************************************************
 * @intro: find a pipe named pipe_name
 * @return: pointer of pipe or NULL
 ****************************************************************************/
qrc_pipe_s *qrc_pipe_find_by_name(const char *pipe_name)
{
  for (uint8_t i = 1; i < g_qrc.pipe_cnt; i++)
    {
      if (0 == strcmp(g_qrc.pipe_list[i].pipe_name, pipe_name))
        {
          return &g_qrc.pipe_list[i];
        }
    }
  return NULL;
}

/****************************************************************************
 * @intro: find a pipe by its pipe_id
 * @return: pointer of pipe or NULL
 ****************************************************************************/
qrc_pipe_s *qrc_pipe_find_by_pipeid(const uint8_t pipe_id)
{
  if (g_qrc.pipe_cnt <= pipe_id)
    {
      return NULL;
    }
  return &g_qrc.pipe_list[pipe_id];
}

/****************************************************************************
 * @intro: modify the data of pipe
 * @return: pointer of pipe or NULL
 ****************************************************************************/
qrc_pipe_s *qrc_pipe_modify_by_name(const char *pipe_name, const qrc_pipe_s *new_data)
{
  return NULL;
}

/****************************************************************************
 * @intro: send TF frame
 * @param qrcf: qrcf_frame(sync_mode + ack + receiver_id)
 * @param data: qrc_msg(qrc_msg_cmd + pipe id + pipe name) or user data
 * @param len: length of data
 * @param qrc_write_lock: whether hold lock to ensure the integrity of the frame, default is true
 * @return: result of TF_Send()
 ****************************************************************************/
bool qrc_frame_send(const qrc_frame *qrcf, const uint8_t *data, const size_t len, const bool qrc_write_lock)
{
  int status;

  if (true == qrc_write_lock)
    {
      status = pthread_mutex_lock(&g_qrc.qrc_write_mutex);
      if (status != 0)
        {
          printf("ERROR: qrc_frame_send: pthread_mutex_lock failed=%d\n", status);
          return false;
        }
    }
  TF_Msg msg;
  TF_ClearMsg(&msg);
  msg.type         = DEFAULT_TF_MSG_TYPE;
  uint8_t *msg_qrc = (uint8_t *)malloc(sizeof(qrc_frame) + len);
  if (NULL == msg_qrc)
    {
      printf("qrc frame : malloc error\n");
      return false;
    }
  memcpy(msg_qrc, qrcf, sizeof(qrc_frame));
  memcpy(msg_qrc + sizeof(qrc_frame), data, len);
  msg.data = msg_qrc;
  msg.len  = sizeof(qrc_frame) + len;

  bool send_res = TF_Send(g_qrc.tf, &msg);
  free(msg_qrc);
  if (true == qrc_write_lock)
    {
      status = pthread_mutex_unlock(&g_qrc.qrc_write_mutex);
      if (status != 0)
        {
          printf("ERROR: qrc_frame_send: pthread_mutex_unlock failed=%d\n", status);
          return false;
        }
    }

  return send_res;
}

bool is_pipe_timeout_busy(const uint8_t pipe_id)
{
  qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
  if (p == NULL)
    {
      printf("ERROR: input pipe id is invalid\n");
      return QRC_ERROR;
    }
  return p->is_pipe_timeout_busy;
}

/****************************************************************************
 * @intro: start the timeout of pipe whose pipe id is pipe_id
 * @param pipe_id: pipe id
 ****************************************************************************/
int start_pipe_timeout(const uint8_t pipe_id, bool *timeout)
{
  qrc_pipe_s *   p = qrc_pipe_find_by_pipeid(pipe_id);
  struct timeval now;

  *timeout = false;

  if (p == NULL)
    {
      printf("ERROR: start_pipe_timeout input pipe id is invalid\n");
      return QRC_ERROR;
    }

  if (true == p->is_pipe_timeout_busy)
    {
      printf("Warning: start_pipe_timeout  timeout is using\n");
      return QRC_ERROR;
    }

  gettimeofday(&now, NULL);

  struct timespec outtime;
  outtime.tv_sec  = now.tv_sec + QRC_MSG_TIME_OUT_S;
  outtime.tv_nsec = now.tv_usec; /*500ms*/

  pthread_mutex_lock(&p->pipe_mutex);
  p->is_pipe_timeout_busy = true;
  if (0 != pthread_cond_timedwait(&p->pipe_cond, &p->pipe_mutex, &outtime))
    {
      printf("\nERROR: pipe(%s) TIMEOUT!\n", p->pipe_name);
      *timeout = true;
    }

  p->is_pipe_timeout_busy = false;
  pthread_mutex_unlock(&p->pipe_mutex);

  return QRC_OK;
}

/****************************************************************************
 * @intro: wake up the timeout of pipe whose pipe id is pipe_id
 * @param pipe_id: pipe id
 ****************************************************************************/
static void stop_pipe_timeout(const uint8_t pipe_id)
{
  qrc_pipe_s *p = qrc_pipe_find_by_pipeid(pipe_id);
  int         status;

  if (p == NULL)
    {
      printf("ERROR: stop_pipe_timeout input pipe id is invalid\n");
      return;
    }

  if (false == p->is_pipe_timeout_busy)
    {
      printf("WARNING: stop_pipe_timeout timeout in idle\n");
    }

  status = pthread_mutex_lock(&p->pipe_mutex);
  if (status != 0)
    {
      printf("stop_pipe_timeout:ERROR pthread_mutex_lock failed=%d\n", status);
      return;
    }

  if (0 != pthread_cond_signal(&p->pipe_cond))
    {
      printf("\nERROR: Can not wake up ack pipe(%s) thread!\n", p->pipe_name);
      pthread_mutex_unlock(&p->pipe_mutex);
      return;
    }

  status = pthread_mutex_unlock(&p->pipe_mutex);
  if (status != 0)
    {
      printf("stop_pipe_timeout:ERROR pthread_mutex_unlock failed=%d\n", status);
      return;
    }
}

/****************************************************************************
 * @intro: lock the qrc_write_mutex
 ****************************************************************************/
void qrc_bus_lock(void)
{
  int status;
  status = pthread_mutex_lock(&g_qrc.qrc_write_mutex);
  if (status != 0)
    {
      printf("qrc_bus_lock:ERROR pthread_mutex_lock failed=%d\n", status);
    }
}

/****************************************************************************
 * @intro: unlock the qrc_write_mutex
 ****************************************************************************/
void qrc_bus_unlock(void)
{
  int status;
  status = pthread_mutex_unlock(&g_qrc.qrc_write_mutex);
  if (status != 0)
    {
      printf("qrc_bus_unlock:ERROR pthread_mutex_unlock failed=%d\n", status);
    }
}

/****************************************************************************
 * @intro: timeout for qrc bus lock
 ****************************************************************************/
static int qrc_lock_start_timeout(bool *timeout)
{
  struct timeval now;

  if (true == g_qrc.is_bus_timeout_busy)
    {
      printf("Warning: qrc_lock_start_timeout  timeout is using\n");
      return QRC_ERROR;
    }

  gettimeofday(&now, NULL);

  struct timespec outtime;
  outtime.tv_sec  = now.tv_sec + QRC_MSG_TIME_OUT_S;
  outtime.tv_nsec = now.tv_usec;

  *timeout = false;
  pthread_mutex_lock(&g_qrc.bus_lock_mutex);
  g_qrc.is_bus_timeout_busy = true;
  if (0 != pthread_cond_timedwait(&g_qrc.bus_lock_cond, &g_qrc.bus_lock_mutex, &outtime))
    {
      printf("\nERROR: bus lock TIMEOUT!\n");
      *timeout = true; /* timeout happened */
    }

  g_qrc.is_bus_timeout_busy = false;

  pthread_mutex_unlock(&g_qrc.bus_lock_mutex);

  return QRC_OK;
}

static void qrc_lock_stop_timeout(void)
{
  pthread_mutex_lock(&g_qrc.bus_lock_mutex);

  if (0 != pthread_cond_signal(&g_qrc.bus_lock_cond))
    {
      printf("\nERROR: Can not wake up bus lock thread!\n");
      pthread_mutex_unlock(&g_qrc.bus_lock_mutex);
      return;
    }
  pthread_mutex_unlock(&g_qrc.bus_lock_mutex);
}

/****************************************************************************
 * @intro: thread of reading response
 ****************************************************************************/
static void *read_thread(void *args)
{
#if defined(QRC_RB5) || defined(QRC_RB3)
  while (1)
    {
      uint8_t buf[QRC_MAX_READ_SIZE];
      ssize_t      read_len = qrc_udriver_read(g_qrc.fd, (char *)buf, sizeof(buf));
      if (read_len > 0) {
        TF_Accept(g_qrc.tf, (uint8_t *)buf, (uint32_t)read_len);
      }
      else {
        usleep(100);
      }
    }
#else //QRC_MCB
  int readable_len;
  while (1)
    {
      readable_len = 0;
      if (ioctl(g_qrc.fd, QRC_FIONREAD, &readable_len) < 0)
        {
          printf("\nERROR: qrc get readable size fail!\n");
          return NULL;
        }
      if (readable_len > 0)
        {
          uint8_t *buf      = malloc(readable_len * sizeof(uint8_t));
          int      read_len = read(g_qrc.fd, buf, readable_len);
          if (read_len > 0)
            {
              TF_Accept(g_qrc.tf, (uint8_t *)buf, (uint32_t)read_len);
            }
          free(buf);
        }
      else
        usleep(100);
    }
#endif
}

/****************************************************************************
 * @intro: execute the function args.fun_cb
 * @param args: thread holder
 ****************************************************************************/
static void qrc_msg_cb_work(struct qrc_msg_cb_args_s args)
{
  if (ACK == args.need_ack)
    {
      qrc_control_write(args.pipe, args.pipe->peer_pipe_id, QRC_ACK);
    }

  args.fun_cb(args.pipe, args.data, args.len, args.response);
  free(args.data);
}

uint8_t get_pipe_number(void)
{
  return g_qrc.pipe_cnt;
}

/* Hardware sync */
static int qrc_hardware_sync(int qrc_fd)
{
  int readable_len = 0;
  int
  try
    = 10;
  char     ack[]     = QRC_HW_SYNC_MSG;
  uint32_t write_cnt = 0;

#if defined(QRC_RB5) || defined(QRC_RB3)
  /* reset MCB */
  qrc_mcb_reset(QRC_GPIOCHIP, QRC_RESETGPIO);
  sleep(2);

  /* Boot APP */
  char mcb_boot_app = QRC_BOOT_APP;

  write_cnt = qrc_udriver_write(qrc_fd, &mcb_boot_app, 1);
  if (write_cnt != 1)
    {
      printf("ERROR: qrc bus write failed!\n");
    }

  printf("INFO: qrc start bus sync\n");

  while (try > 0)
    {
      try--;
      sleep(1);

      char buf[QRC_MAX_READ_SIZE];
      ssize_t read_len = qrc_udriver_read(qrc_fd, buf, sizeof(buf));
      if (read_len >= 3)
      {
        int count = 0;
        while (count <= (read_len - 2))
          {
            if (buf[count] == 'O' && buf[count + 1] == 'K')
              {
                write_cnt = qrc_udriver_write(qrc_fd, ack, sizeof(ack));
                if (write_cnt != sizeof(ack))
                  {
                    printf("ERROR: qrc bus write SYNC MSG failed!\n");
                    qrc_udriver_close(qrc_fd);
                    return -1;
                  }
                printf("DEBUG: qrc bus SYNC done\n");
                return 0;
              }
            count = count + 1;
          }
      }
      printf("INFO: qrc bus write SYNC try = %d \n", try);
    }

#else // QRC_MCB
  while (try > 0)
    {
      write_cnt = write(qrc_fd, ack, sizeof(ack));
      if (write_cnt != sizeof(ack))
        {
          printf("ERROR: qrc bus write SYNC MSG failed!\n");
          close(qrc_fd);
          return -1;
        }

      /* check if received ACK msg */
      if (ioctl(qrc_fd, QRC_FIONREAD, &readable_len) < 0)
        {
          printf("\nERROR: qrc get readable size fail!\n");
          close(qrc_fd);
          return -1;
        }

      if (readable_len >= 3)
        {
          char *buf      = malloc(readable_len * sizeof(char));
          int   read_len = read(qrc_fd, buf, readable_len);
          if (read_len >= 3)
            {
              int count = 0;
              while (count <= (read_len - 2))
                {
                  if (buf[count] == 'O' && buf[count + 1] == 'K')
                    {
                      printf("INFO: qrc bus sync done\n");
                      return 0;
                    }
                  count = count + 1;
                }
            }
          free(buf);
        }
      printf("INFO: qrc bus write SYNC try = %d \n", try);
      try
        --;
      sleep(1);
    }

#endif
  printf("\nERROR: qrc BUS try to sync failed \n");
  return -1;
}

/****************************************************************************
 * Public function
 ****************************************************************************/

/****************************************************************************
 * @intro: initilial
 * @return: result of initilial
 ****************************************************************************/
bool qrc_init(void)
{
#if defined(QRC_RB5) || defined(QRC_RB3)
  g_qrc.fd = qrc_udriver_open(QRC_FD);
#else
  g_qrc.fd = open(QRC_FD, O_RDWR);
#endif
  if (-1 == g_qrc.fd)
    {
      printf("ERROR: %s open failed!\n", QRC_FD);
      close(g_qrc.fd);
      return false;
    }

  if (0 != qrc_hardware_sync(g_qrc.fd))
    {
      printf("ERROR: qrc HW sync failed!\n");
      close(g_qrc.fd);
      return false;
    }

  if (0 != pthread_mutex_init(&g_qrc.pipe_list_mutex, NULL))
    {
      printf("\nERROR: pipe mutex initalize failed!\n");
      close(g_qrc.fd);
      return false;
    }
  if (0 != pthread_mutex_init(&g_qrc.qrc_write_mutex, NULL))
    {
      printf("\nERROR: pipe mutex initalize failed!\n");
      close(g_qrc.fd);
      return false;
    }

  /* init qrc lock timeout cond & mutex */
  if (0 != pthread_cond_init(&g_qrc.bus_lock_cond, NULL))
    {
      printf("\nERROR: bus_lock_cond cond initalize failed!\n");
      return false;
    }
  if (0 != pthread_mutex_init(&g_qrc.bus_lock_mutex, NULL))
    {
      printf("\nERROR: bus_lock_mutex initalize failed!\n");
      return false;
    }

  g_qrc.peer_pipe_list_ready = false;
  g_qrc.msg_threadpool       = qrc_thread_pool_init(QRC_THREAD_NUM);
  g_qrc.control_threadpool   = qrc_thread_pool_init(QRC_CONTROL_THREAD_NUM);
  g_qrc.tf                   = TF_Init(TF_MASTER);
  TF_AddGenericListener(g_qrc.tf, read_response_listener);

  pthread_create(&g_qrc.read_thread, NULL, read_thread, NULL);

  return qrc_pipe_list_init();
}

void qrc_pipe_threads_join(void)
{
  qrc_threads_join(g_qrc.msg_threadpool);
  qrc_threads_join(g_qrc.control_threadpool);
}

bool qrc_destroy(void)
{
  printf("INFO: qrc destroy\n");
  qrc_threadpool_destroy(g_qrc.msg_threadpool);
  qrc_threadpool_destroy(g_qrc.control_threadpool);
  pthread_cancel(g_qrc.read_thread);

#if defined(QRC_RB5) || defined(QRC_RB3)
  /* reset MCB */
  qrc_mcb_reset(QRC_GPIOCHIP, QRC_RESETGPIO);
#endif

  return close(g_qrc.fd) == 0;
}
