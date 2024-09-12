/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __CLIENT_CONTROL_MSG_H
#define __CLIENT_CONTROL_MSG_H

#include <stdio.h>
#include <stdint.h>

/* Client pipe name */
#define CLIENT_PIPE "client"

enum client_msg_type_e
{
  SET_CLIENT = 0,
  GET_CLIENT
};

/* control client */

enum control_client_e
{
  ROBOT_CONTROLLER = 0,
  CHARGER_CONTROLLER,
  REMOTE_CONTROLLER,
  MAX_CLIENT
};

struct client_msg_s
{
  int msg_type;
  int client;
} __attribute__((aligned(4)));

#endif