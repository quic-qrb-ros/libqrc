/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef __APP_CHARGER_CONTROL_MSG_H
#define __APP_CHARGER_CONTROL_MSG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdio.h>
#include <stdint.h>

/********************************************************************************
 * Pre-processor Definitions
 ********************************************************************************/

#define CHARGER_PIPE "charger"

enum charger_ctl_cmd_e
{
  GET_CTL_VOLTAGE = 10,
  GET_CTL_CURRENT,
  GET_CTL_SM_STATE,
  GET_CTL_EXCEPTION,
  GET_CTL_PILE_STATE,
  GET_CTL_IS_CHARGING,
  GET_CTL_ALL_STATE,
  START_CTL_CHARGING,
  STOP_CTL_CHARGING
};

enum charger_ctl_exception_e
{
  CHARGER_CTL_VOLTAGE_ERROR = 1,
  CHARGER_CTL_CHARGING_CURRENT_ERROR,
  CHARGER_CTL_PILE_STAT_ERROR,
  CHARGER_CTL_IS_CHARGING_ERROR,
  CHARGER_CTL_GET_SPEED_ERROR,
  CHARGER_CTL_SM_STATE_ERROR,
  CHARGER_CTL_DEV_NOT_READY_ERROR,
  CHARGER_CTL_ENTER_EXCEPTION,
  CHARGER_CTL_GET_ALL_ERROR
};

enum charger_ctl_sm_state_e
{
  CHR_CTL_SM_UNKNOWN,
  CHR_CTL_SM_IDLE = 1,
  CHR_CTL_SM_SEARCHING,
  CHR_CTL_SM_CONTROLLING,
  CHR_CTL_SM_FORCE_CHARGING,
  CHR_CTL_SM_CHARGING,
  CHR_CTL_SM_CHARGER_DONE,
  CHR_CTL_SM_EXCEPTION
};

struct charger_ctl_msg_s
{
  uint32_t cmd_type;
  union
  {
    float    voltage;
    float    current;
    uint32_t pile_stats;
    uint32_t is_charging;
    uint32_t sm_state;
    uint32_t exception_value;
  } cmd_data;
} __attribute__((aligned(4)));

#endif /* __APP_CHARGER_CONTROL_MSG_H */
