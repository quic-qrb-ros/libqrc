/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __TIME_SYNC_MSG_H
#define __TIME_SYNC_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#define TIME_SYNC_PIPE "time"

enum time_sync_msg_type_e
{
  TIME_LOOP = 0x0,
  GET_TIME,
  SET_TIME,
};

struct time_sync_msg_s
{
  int       type;
  long long sec; /* Seconds */
  long long ns;  /* nanoseconds */
} __attribute__((aligned(4)));

#ifdef __cplusplus
}
#endif

#endif // __TIME_SYNC_MSG_H
