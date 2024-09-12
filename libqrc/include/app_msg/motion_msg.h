/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __MOTION_MSG_H
#define __MOTION_MSG_H

#include <stdio.h>
#include <stdint.h>

/* Robot motion pipe name */
#define MOTION_PIPE "motion"

/* Motion odom pipe name */
#define ODOM_PIPE "odometry"

enum control_mode_e
{
  INACTIVE = 0,
  SPEED,
  POSITION,
  TORQUE,
  SET_DRV_ERR /* Just for test */
};

enum odom_msg_type_e
{
  ODOM_SPEED = 0,
  ODOM_POSITION,
};

enum control_msg_type_e
{
  SET_SPEED = 0,
  SWITCH_MODE,
  SET_EMERGENCY,
  SET_POSITION,
  MOTOR_DRIVER_STATUS
};

enum motor_err_e
{
  NORMAL = 0,
  OVER_POWER,
  LACK_POWER,
  OVER_LOAD,
  EEPROM_ERR,
  ENCODER_ERR,
  OTHER_ERR
};

struct speed_cmd_s
{
  float vx;
  float vz;
};

struct position_cmd_s
{
  int   pose_type;
  float pose;
};

union motion_control_data_u
{
  struct speed_cmd_s    speed_cmd;
  struct position_cmd_s position_cmd;
  int                   emergency;
  int                   mode;
  int                   motor_driver_status;
} __attribute__((aligned(4)));

struct motion_control_msg_s
{
  int                         msg_type;
  union motion_control_data_u data;
} __attribute__((aligned(4)));

/* motion odom structure */
struct motion_odom_s
{
  int       type;
  long long sec;
  long long ns;
  float     x;
  float     z;
} __attribute__((aligned(4)));

#endif