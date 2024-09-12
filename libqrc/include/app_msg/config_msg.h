/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __CONFIG_MSG_H
#define __CONFIG_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CONFIG_PIPE "config"

enum config_msg_type_e
{
  CAR = 0,
  MOTION,
  SCALE,
  SENSOR,
  RC,
  OBSTACLE_AVOIDANCE,
  CONFIG_MSG_TYPE_MAX,
  APPLY,
};

enum mcb_task_id_e
{
  ID_MOTION_MANAGEMENT = 0x00,
  ID_CHARGER_MANAGEMENT,
  ID_RC_MANAGEMENT,
  ID_AVOID_MANAGEMENT,
  ID_TIME_SYNC,
  ID_IMU,
  ID_MISC,
  ID_MOTION_ODOM,
  ID_ROBOT_CONTROLLER,
  ID_CLIENT_CONTROLLER,
  ID_CHARGER_CONTROLLER,
  ID_REMOTE_CONTROLLER,
  ID_EMERGENCY,
  ID_MAX_TASK,
};

struct config_apply_s
{
  int                status;     /* 0 is success, not 0 is failed */
  enum mcb_task_id_e error_type; /* which task caused error */
} __attribute__((aligned(4)));

enum car_model_e
{
  CYCLE_CAR,
  AMR_6040,
  CAR_MODE_MAX,
};

enum kinematic_model_e
{
  DIFF_CAR,
  ACKERMAN_CAR,
  KINEMATIC_MODEL_MAX,
};

struct config_car_s
{
  int   car_model;
  int   kinematic_model;
  float wheel_space;
  float wheel_perimeter;
} __attribute__((aligned(4)));

struct config_motion_s
{
  float    max_speed;
  float    max_angle_speed;
  float    max_position_dist;
  float    max_position_angle;
  float    max_position_line_speed;
  float    max_position_angle_speed;
  float    pid_speed[3];
  float    pid_position[3];
  uint32_t odom_frequency;
} __attribute__((aligned(4)));

struct config_scale_s
{
  float speed_scale[2]; /* 0: line speed; 1: angle speed */
  float position_scale[2];
  float speed_odom_scale[2];
  float position_odom_scale[2];
} __attribute__((aligned(4)));

struct config_sensor_s
{
  int imu_enable;
  int ultra_enable;
  int ultra_quantity;
} __attribute__((aligned(4)));

struct config_remote_controller_s
{
  int   rc_enable;
  float max_speed;
  float max_angle_speed;
} __attribute__((aligned(4)));

struct config_obstacle_avoidance_s
{
  float bottom_dist; /*unit: m*/
  float side_dist;
  float front_dist;
} __attribute__((aligned(4)));

struct config_msg_s
{
  int type;
  union
  {
    struct config_apply_s              apply;
    struct config_car_s                car;
    struct config_motion_s             motion;
    struct config_scale_s              scale;
    struct config_sensor_s             sensor;
    struct config_remote_controller_s  rc;
    struct config_obstacle_avoidance_s ob;
  } data;
} __attribute__((aligned(4)));

#ifdef __cplusplus
}
#endif

#endif // __ROBOT_CONFIG_MSG_H
