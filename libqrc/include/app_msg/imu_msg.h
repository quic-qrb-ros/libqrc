/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __IMU_MSG_H
#define __IMU_MSG_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>

/* IMU pipe name */
#define IMU_PIPE "imu"

struct imu_data_s
{
  float xa;
  float ya;
  float za;
  float xg;
  float yg;
  float zg;
};

struct imu_msg_s
{
  long long         sec; /* seconds */
  long long         ns;  /* nanoseconds */
  struct imu_data_s data;
} __attribute__((aligned(4)));

#endif