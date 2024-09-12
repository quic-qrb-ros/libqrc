/****************************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 ****************************************************************************/

#ifndef TF_CONFIG_H
#define TF_CONFIG_H

#include <stdint.h>
#include <stdio.h>

#define TF_USE_MUTEX    0
#define TF_ID_BYTES     1
#define TF_LEN_BYTES    2
#define TF_TYPE_BYTES   1
#define TF_CKSUM_TYPE   TF_CKSUM_CRC16
#define TF_USE_SOF_BYTE 1
#define TF_SOF_BYTE     0x01
typedef uint16_t TF_TICKS;
typedef uint8_t  TF_COUNT;
#define TF_MAX_PAYLOAD_RX       1024
#define TF_SENDBUF_LEN          1024
#define TF_MAX_ID_LST           10
#define TF_MAX_TYPE_LST         10
#define TF_MAX_GEN_LST          5
#define TF_PARSER_TIMEOUT_TICKS 10

#define TF_Error(format, ...) printf("[TF] " format "\n", ##__VA_ARGS__)

#endif