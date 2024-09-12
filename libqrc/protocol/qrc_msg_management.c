/***************************************************************************
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
****************************************************************************/
/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "qrc_msg_management.h"
#include "qrc.h"

#define TRY_TIMES (1)
static uint8_t g_pipe_write_lock_owner = 255;

/****************************************************************************
 * @intro: require both MCB and RB5's lock
 * @param p: the initiator of the request
 ****************************************************************************/
bool qrc_require_pipe(qrc_pipe_s *p)
{
  g_pipe_write_lock_owner = p->pipe_id;
  if (false == qrc_control_write(p, p->pipe_id, QRC_WRITE_LOCK))
    {
      printf("ERROR: %s require pipe failed!\n", p->pipe_name);
      return false;
    }
  qrc_bus_lock();
  return true;
}

/****************************************************************************
 * @intro: release both MCB and RB5's lock
 * @param p: the initiator of the request
 ****************************************************************************/
bool qrc_release_pipe(qrc_pipe_s *p)
{
  if (g_pipe_write_lock_owner != p->pipe_id)
    {
      printf("ERROR: %s releases pipe failed! owner of lock is not you!\n", p->pipe_name);
      return false;
    }

  qrc_bus_unlock();
  if (false == qrc_control_write(p, p->pipe_id, QRC_WRITE_UNLOCK))
    {
      printf("ERROR: %s releases pipe failed!\n", p->pipe_name);
      return false;
    }

  return true;
}

/****************************************************************************
 * @intro: initialize qrc procotol
 ****************************************************************************/
bool init_qrc_management(void)
{
  return qrc_init();
}

/****************************************************************************
 * @intro: get a new or exited pipe
 * @param pipe_name: name of pipe
 * @return: pointer of pipe or NULL
 ****************************************************************************/
qrc_pipe_s *qrc_get_pipe(const char *pipe_name)
{
  int pipe_name_len = (int)strlen(pipe_name);
  if (pipe_name_len > 10)
    {
      printf("\nERROR: Pipe name(%s) is too long!\n", pipe_name);
      return NULL;
    }
  qrc_pipe_s *p = qrc_pipe_insert(pipe_name);
  if (NULL == p)
    {
      printf("ERROR: Pipe(%s) create failed!\n", pipe_name);
      return NULL;
    }

  if (true == p->pipe_ready)
    {
      /* pipe has been initialized */
      return p;
    }

  if (false == qrc_control_write(p, p->pipe_id, QRC_REQUEST))
    {
      printf("ERROR: Pipe(%s) send peer request failed!\n", pipe_name);
      return NULL;
    }

  printf("INFO: Pipe(%s) create done id=%d\n", pipe_name, p->pipe_id);
  return p;
}

/****************************************************************************
 * @intro: register callback function of pipe
 * @param pipe_name: name of pipe
 * @param fun_cb: callback function
 * @return: result of registration
 ****************************************************************************/
bool qrc_register_message_cb(qrc_pipe_s *pipe, qrc_msg_cb fun_cb)
{
  if (pipe == NULL)
    {
      printf("ERROR: Register callback function failed! Pipe is NULL!\n");
      return false;
    }
  pipe->cb = fun_cb;
  return true;
}

/****************************************************************************
 * @intro: qrc write with lock
 * @param pipe: writer
 * @param data: data of writer
 * @param len: length of data
 * @param ack: whether writer need response, true means yes, false means no
 * @return: SUCCESS or TIMEOUT or FAILED
 ****************************************************************************/
enum qrc_write_status_e qrc_write(const qrc_pipe_s *pipe, const uint8_t *data, const size_t len, const bool data_ack)
{
  enum qrc_write_status_e res = FAILED;
  bool                    timeout;
  int
  try
    = TRY_TIMES;
  bool send_result;

  if (NULL == pipe || pipe->pipe_id >= get_pipe_number())
    {
      printf("ERROR: No such pipe! Write failed!\n");
      return res;
    }
  if (255 == pipe->peer_pipe_id)
    {
      printf("ERROR: Pipe write failed! (%s) doesn't have peer pipe!\n", pipe->pipe_name);
    }
  else
    {
      qrc_frame qrcf;
      qrcf.receiver_id = pipe->peer_pipe_id;
      qrcf.ack         = NO_ACK;

      if (data_ack == true) /* need ack transport */
        {
          while (try >= 0)
            {
              try
                =
                try
                  -1;

              if (true == is_pipe_timeout_busy(pipe->pipe_id))
                {
                  printf("Warning: Pipe (%s) send with nack by timer is using\n", pipe->pipe_name);
                  send_result = qrc_frame_send(&qrcf, (uint8_t *)data, len, true);
                  res         = (send_result) ? SUCCESS : FAILED;
                  break;
                }
              else /* add ack in frame and start timer */
                {
                  qrcf.ack    = ACK;
                  send_result = qrc_frame_send(&qrcf, (uint8_t *)data, len, true);
                  if (true == send_result)
                    {
                      if (QRC_OK != start_pipe_timeout(pipe->pipe_id, &timeout))
                        {
                          res = ACK_ERR;
                          break;
                        }
                      if (true == timeout)
                        {
                          res = TIMEOUT;
                          printf("Warning: Pipe (%s) timeout, try to send again \n", pipe->pipe_name);
                          continue;
                        }
                      res = SUCCESS;
                      break;
                    }
                  break;
                }
            }
        }
      else /* no ack transport */
        {
          send_result = qrc_frame_send(&qrcf, (uint8_t *)data, len, true);
          res         = (send_result) ? SUCCESS : FAILED;
        }
    }
  return res;
}

/****************************************************************************
 * @intro: qrc write without lock
 * @param pipe: writer
 * @param data: data of writer
 * @param len: length of data
 * @return: SUCCESS or TIMEOUT or FAILED
 ****************************************************************************/
enum qrc_write_status_e qrc_write_fast(const qrc_pipe_s *pipe, const void *data, const size_t len)
{
  enum qrc_write_status_e res = FAILED;
  if (255 == pipe->peer_pipe_id)
    {
      printf("ERROR: Pipe write failed! (%s) doesn't have peer pipe!\n", pipe->pipe_name);
      return FAILED;
    }
  else
    {
      qrc_frame qrcf;
      qrcf.receiver_id = pipe->peer_pipe_id;
      qrcf.ack         = NO_ACK;
      bool send_result = qrc_frame_send(&qrcf, data, len, false);
      res              = (true == send_result) ? SUCCESS : FAILED;
    }

  return res;
}

/*
* wait for implement
*/
enum qrc_write_status_e qrc_sync_write(const qrc_pipe_s *pipe, const void *data, const size_t len, const void *respond_data, const size_t res_len)
{
  return FAILED;
}

/*
* wait for implement
*/
enum qrc_write_status_e qrc_response(const qrc_pipe_s *pipe, const void *data, const size_t len)
{
  return FAILED;
}

bool deinit_qrc_management(void)
{
  return qrc_destroy();
}
