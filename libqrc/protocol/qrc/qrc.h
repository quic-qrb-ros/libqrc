/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/
#ifndef __QRC_H
#define __QRC_H

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>

#include "TinyFrame.h"
#include "qrc_msg_management.h"

/* qrc control pipe id */
#define QRC_CONTROL_PIPE_ID 0
#define QRC_OK              0
#define QRC_ERROR           (-1)
enum qrc_msg_cmd
{
  QRC_REQUEST = 1,
  QRC_RESPONSE,
  QRC_WRITE_LOCK,
  QRC_WRITE_LOCK_ACK,
  QRC_WRITE_UNLOCK,
  QRC_WRITE_UNLOCK_ACK,
  QRC_ACK,
  QRC_CONNECT_REQUEST,
  QRC_CONNECT_RESPONSE,
};

enum ack_request
{
  NO_ACK = 0,
  ACK
};

typedef struct qrc_msg
{
  uint8_t cmd;
  uint8_t pipe_id;
  char    pipe_name[10];
} qrc_msg;

typedef struct qrc_frame
{
  uint8_t sync_mode : 1;
  uint8_t ack : 1;
  uint8_t receiver_id : 6;
} qrc_frame;

typedef struct qrc_thread_pool_s *qrc_thread_pool;

struct qrc_msg_cb_args_s
{
  qrc_msg_cb         fun_cb;
  struct qrc_pipe_s *pipe;
  uint8_t *          data;
  size_t             len;
  bool               response;
  uint8_t            need_ack;
};

typedef void (*qrc_work)(struct qrc_msg_cb_args_s args);
struct qrc_thread_pool_s *qrc_thread_pool_init(int num);
int                       qrc_threadpool_add_work(struct qrc_thread_pool_s *thpool, qrc_work work_fun, struct qrc_msg_cb_args_s args);
void                      qrc_threadpool_wait(struct qrc_thread_pool_s *thpool);
void                      qrc_threadpool_destroy(struct qrc_thread_pool_s *thpool);
void                      qrc_threads_join(struct qrc_thread_pool_s *thpool);
void                      qrc_pipe_threads_join(void);

bool        qrc_control_write(const struct qrc_pipe_s *pipe, const uint8_t pipe_id, const enum qrc_msg_cmd cmd);
bool        qrc_init(void);
uint8_t     get_pipe_number(void);
qrc_pipe_s  qrc_pipe_node_init(void);
bool        qrc_pipe_list_init(void);
qrc_pipe_s *qrc_pipe_insert(const char *pipe_name);
qrc_pipe_s *qrc_pipe_find_by_name(const char *pipe_name);
qrc_pipe_s *qrc_pipe_find_by_pipeid(const uint8_t pipe_id);
qrc_pipe_s *qrc_pipe_modify_by_name(const char *pipe_name, const qrc_pipe_s *new_data);
bool        qrc_frame_send(const qrc_frame *qrcf, const uint8_t *data, const size_t len, const bool qrc_write_lock);

bool is_pipe_timeout_busy(const uint8_t pipe_id);
int  start_pipe_timeout(const uint8_t pipe_id, bool *timeout);

void qrc_bus_unlock(void);
void qrc_bus_lock(void);

bool qrc_destroy(void);

#endif
