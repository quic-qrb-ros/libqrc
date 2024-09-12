/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/** qrc interface **/

#include "qti_qrc_udriver.h"
#include "qti_qrc_common.h"
#include "gpiod.h"

static int ops_num = 0;
enum bus_protocol_e {
    UART = 0,
    SPI,
    CAN,
    MAX
};

struct qrc_user_driver {
    enum bus_protocol_e protocol;
    struct qrc_device_ops *device_ops;
};

struct qrc_user_driver protocol_list[3] = {
	{UART, &qrc_uart_ops},
	{SPI, NULL},
	{CAN, NULL}
};

int qrc_udriver_open(const char *qrc_dev)
{
	return protocol_list[ops_num].device_ops->open(qrc_dev);
}

void qrc_udriver_close(int fd)
{
	protocol_list[ops_num].device_ops->close(fd);
}

ssize_t qrc_udriver_read(int fd, char *buffer, size_t size)
{
	return protocol_list[ops_num].device_ops->read(fd, buffer, size);
}

ssize_t qrc_udriver_write(int fd, const char *data, size_t length)
{
	return protocol_list[ops_num].device_ops->write(fd, data, length);
}

int qrc_udriver_fionread(int fd, int *arg)
{
	return protocol_list[ops_num].device_ops->fionread(fd, arg);
}

int qrc_udriver_tcflsh(int fd)
{
	return protocol_list[ops_num].device_ops->tcflsh(fd);
}

int qrc_mcb_reset(const char *gpiochip, int reset_gpio)
{
	struct gpiod_chip *chip = NULL;
	struct gpiod_line *line = NULL;
	int ret = 0;
	const char* consumer = NULL;

	chip = gpiod_chip_open(gpiochip);   //Open the GPIO chip
	if (!chip) {
		printf("Failed to open GPIO chip\n");
		ret = -1;
		goto cleanup;
	}

	line = gpiod_chip_get_line(chip, reset_gpio);   //Get the GPIO line
	if (!line) {
		printf("Failed to get GPIO line\n");
		ret = -1;
		goto cleanup;
	}

	consumer = gpiod_line_consumer(line);

	ret = gpiod_line_request_output(line, consumer, 0);
	if (ret < 0) {
		printf("Failed to request GPIO line as output\n");
		goto cleanup;
	}

	ret = gpiod_line_set_value(line, 1);
	if (ret < 0) {
		printf("Failed to set GPIO line value to high\n");
		goto cleanup;
	}

	usleep(100000); //Wait

	ret = gpiod_line_set_value(line, 0);
	if (ret < 0) {
		printf("Failed to set GPIO line value to low\n");
		goto cleanup;
	}

cleanup:
	if (line)
		gpiod_line_release(line);
	if (chip)
		gpiod_chip_close(chip);

	return ret;
}
