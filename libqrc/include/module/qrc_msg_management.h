/***************************************************************************
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
****************************************************************************/
#ifndef __QRC_MSG_MANAGEMENT_H
#define __QRC_MSG_MANAGEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*qrc_msg_cb)(struct qrc_pipe_s *pipe, void *data, size_t len, bool response);

typedef struct qrc_pipe_s
{
  char            pipe_name[10];
  pthread_cond_t  pipe_cond;
  pthread_mutex_t pipe_mutex;
  volatile bool   is_pipe_timeout_busy; /* true: bus timeout in use */
  uint8_t         pipe_id;
  uint8_t         peer_pipe_id;
  bool            pipe_ready;
  void (*cb)(struct qrc_pipe_s *pipe, void *data, size_t len, bool response);
} qrc_pipe_s;

enum qrc_write_status_e
{
  SUCCESS = 0,
  TIMEOUT,
  ACK_ERR,
  FAILED
};

bool                    init_qrc_management(void);
bool                    qrc_require_pipe(qrc_pipe_s *p);
bool                    qrc_release_pipe(qrc_pipe_s *p);
qrc_pipe_s *            qrc_get_pipe(const char *pipe_name);
bool                    qrc_register_message_cb(qrc_pipe_s *pipe, const qrc_msg_cb fun_cb);
enum qrc_write_status_e qrc_write(const qrc_pipe_s *pipe, const uint8_t *data, const size_t len, const bool data_ack);
enum qrc_write_status_e qrc_sync_write(const qrc_pipe_s *pipe, const void *data, const size_t len, const void *respond_data, const size_t res_len);
enum qrc_write_status_e qrc_write_fast(const qrc_pipe_s *pipe, const void *data, const size_t len);
enum qrc_write_status_e qrc_response(const qrc_pipe_s *pipe, const void *data, const size_t len);
bool                    deinit_qrc_management(void);

#ifdef __cplusplus
}
#endif

#endif
