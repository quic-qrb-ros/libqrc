/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/** qrc header **/

#ifndef __QTI_QRC_UDRIVER_H
#define __QTI_QRC_UDRIVER_H

#include <stdio.h>

int qrc_udriver_open(const char *qrc_dev);
void qrc_udriver_close(int fd);
ssize_t qrc_udriver_read(int fd, char *buffer, size_t size);
ssize_t qrc_udriver_write(int fd, const char *data, size_t length);
int qrc_udriver_fionread(int fd, int *arg);
int qrc_udriver_tcflsh(int fd);

int qrc_mcb_reset(const char *gpiochip, int reset_gpio);

#endif



