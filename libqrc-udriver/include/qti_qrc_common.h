/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/** qrc header **/

#ifndef __QTI_QRC_COMMON_H
#define __QTI_QRC_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct qrc_device_ops {
	int (*open)(const char *qrc_dev);
	void (*close)(int);
	ssize_t (*read)(int, char *, size_t);
	ssize_t (*write)(int, const char *, size_t);
	int (*fionread)(int, int *arg);
	int (*tcflsh)(int);
};

extern struct qrc_device_ops qrc_uart_ops;

#endif



