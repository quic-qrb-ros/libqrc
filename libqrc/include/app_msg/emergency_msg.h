/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/
#ifndef _LIBQRC_EMERGENCY_MSG_H
#define _LIBQRC_EMERGENCY_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define EMERG_PIPE "emerg"

/****************************************************************************
 * Public types
 ****************************************************************************/
enum emerg_msg_type_e
{
  ENABLEMENT,
  EVENT,
};

enum emerg_msg_event_type_e
{
  ENTER,
  EXIT,
};

struct emerg_msg_event_s
{
  int type; //enum emerg_msg_event_type_e
  int trigger_sensor;
};

struct emerg_msg_s
{
  int msg_type; //enum emerg_msg_type_e
  union
  {
    int                      value;
    struct emerg_msg_event_s event;
  } data;
} __attribute__((aligned(4)));

#ifdef __cplusplus
}
#endif

#endif
