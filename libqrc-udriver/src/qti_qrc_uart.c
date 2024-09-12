/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "qti_qrc_common.h"
#include <termios.h>

#define DEFAULT_BAUDRATE 115200

static int userial_to_tcio_baud(int cfg_baud)
{
    switch (cfg_baud) {
        case 115200:
            return B115200;
        case 57600:
            return B57600;
        case 19200:
            return B19200;
        case 9600:
            return B9600;
        case 2400:
            return B2400;
        case 1200:
            return B1200;
        default:
            fprintf(stderr, "Error: Invalid baudrate\n");
            exit(EXIT_FAILURE);
    }
}

static int qrc_serial_set_baud(int fd, uint32_t serial_baud)
{
	struct termios termios;
	int ret;
	ret = tcgetattr(fd, &termios);
	if (ret < 0) {
		fprintf(stderr, "Failed to get tty device attributes\n");
		return ret;
	}

	int tcio_baud = userial_to_tcio_baud(serial_baud);
	cfsetispeed(&termios, tcio_baud);
	cfsetospeed(&termios, tcio_baud);
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	termios.c_oflag &= ~OPOST;
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	termios.c_cflag &= ~(CSIZE | PARENB);
	termios.c_cflag |= CS8;
	termios.c_cflag |= CRTSCTS;
	termios.c_cflag |= CLOCAL;
	termios.c_cflag &= ~CRTSCTS;

	ret = tcsetattr(fd, TCSANOW, &termios);
	if (ret < 0) {
		fprintf(stderr, "Failed to set tty device attributes\n");
		return ret;
	}
	return ret;
}

int qrc_serial_open(const char *qrc_dev)
{
	int fd = open(qrc_dev, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "Failed to open QRC tty device\n");
		return fd;
	}

	int ret = qrc_serial_set_baud(fd, DEFAULT_BAUDRATE);  //Set the baud rate
	if (ret < 0) {
		fprintf(stderr, "Failed to set tty device baud\n");
		close(fd);
		return ret;
	}
	return fd;
}

static ssize_t qrc_serial_write(int fd, const char *data, size_t size)
{
	if (fd < 0) {
		fprintf(stderr, "Failed to write, No initialization\n");
		return -1;
	}
	if (data == NULL) {
		fprintf(stderr, "Failed to write, data is empty\n");
		return -1;
	}

	if (size == 0) {
		return 0;
	}
	return write(fd, data, size);
}

static ssize_t qrc_serial_read(int fd, char *data, size_t size)
{
	if (fd < 0) {
		fprintf(stderr, "Failed to read, No initialization\n");
		return -1;
	}

	if (data == NULL) {
		fprintf(stderr, "Failed to read, data is empty\n");
		return -1;
	}

	if (size == 0) {
		return 0;
	}

	size_t bytes_available = 0;
	ioctl(fd, FIONREAD, (int *)&bytes_available);
	if (bytes_available == 0) {
		return 0;
	}
	if (bytes_available >= size) {
		return read(fd, data, size);
	} else {
		return read(fd, data, bytes_available);
	}
}

static int qrc_serial_fionread(int fd, int *arg)
{
	if (fd < 0) {
		fprintf(stderr, "Failed to fionread, No initialization\n");
		return -1;
	}
	return ioctl(fd, FIONREAD, arg);
}

static int qrc_serial_tcflsh(int fd)
{
	if (fd < 0) {
		fprintf(stderr, "Failed to tcflsh, No initialization\n");
		return -1;
	}
	return ioctl(fd, TCFLSH, TCIOFLUSH);
}

static void qrc_serial_close(int fd)
{
	if (fd < 0) {
		fprintf(stderr, "Warning: No initialization\n");
		return;
	}
	close(fd);
}

struct qrc_device_ops qrc_uart_ops = {
	.open		= qrc_serial_open,
	.close		= qrc_serial_close,
	.write		= qrc_serial_write,
	.read		= qrc_serial_read,
	.fionread	= qrc_serial_fionread,
	.tcflsh		= qrc_serial_tcflsh,
};
