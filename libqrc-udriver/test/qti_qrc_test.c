/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <string.h>
#include "qti_qrc_udriver.h"
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_DATA_LEN  100

static struct option long_options[] = {
	{"dev", required_argument, 0, 'd'},
	{"gpiochip", required_argument, 0, 'g'},
	{"reset_gpio", required_argument, 0, 'r'},
	{"size", required_argument, 0, 's'},
	{"times", required_argument, 0, 't'},
	{"help", required_argument, 0, 'h'},
	{0, 0, 0, 0}
};

void usage() {
	printf("Usage: ./qti_qrc_test [options]\n");
	printf("Options:\n");
	printf("  -d, --dev=DEVICE          Serial device node\n");
	printf("  -g, --gpiochip=GPIOCHIP   GPIO chip device node\n");
	printf("  -r, --reset_gpio=PIN      Reset GPIO pin\n");
	printf("  -s, --size=SIZE           The size of buffer sent or received at a time\n");
	printf("  -t, --times=NUMBER        Test repeat times\n");
}

void generate_random_string(char *str, size_t len) {
	const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const size_t charset_len = strlen(charset);

	for (size_t i = 0; i < len - 1; i++) {
		str[i] = charset[rand() % charset_len];
	}
	str[len - 1] = '\0';
}

int main(int argc, char** argv)
{
	const char *dev = NULL;
	const char *gpiochip = NULL;
	int reset_gpio = -1;
	int size = -1;
	int times = -1;
	int ret;
	int option_index = 0;

	while ((ret = getopt_long(argc, argv, "d:g:r:s:t:", long_options, &option_index)) != -1) {
		switch (ret) {
			case 'd':
				dev = optarg;
				break;
			case 'g':
				gpiochip = optarg;
				break;
			case 'r':
				reset_gpio = atoi(optarg);
				break;
			case 's':
				size = atoi(optarg);
				break;
			case 't':
				times = atoi(optarg);
				break;
			case '?':
				usage();
				exit(EXIT_FAILURE);
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}

	if (!dev || !gpiochip || (reset_gpio <= 0) || (size <= 0) || (times <= 0)) {
		fprintf(stderr, "Error: Missing required options.\n");
		usage();
		exit(EXIT_FAILURE);
	}

	bool initialized = false;
	int fd = qrc_udriver_open(dev);
	if (fd < 0) {
		fprintf(stderr, "Failed to open device\n");
		return -1;
	}

	initialized = true;
	size_t length = size;
	char* rx_buffer = (char*)malloc(length);
	ssize_t num = -1;
	char* tx_buffer = (char*)malloc(length);
	char str[10] = {0};
	char option;
	size_t sum;
	int count;

	srand(time(NULL));

	while (1) {
		printf("Please select an operation:\n");
		printf("1 :Write and Read test\n");
		printf("2 :FIONREAD and TCFLSH test\n");
		printf("3 :Stability test\n");
		printf("4 :Reset test\n");
		printf("5 :Speed test\n");
		printf("c :Exit\n");
		printf("Enter your choice: ");
		option = 0;
		if (fgets(str, sizeof(str), stdin) == NULL) {
			fprintf(stderr, "Input failure\n");
			free(rx_buffer);
			free(tx_buffer);
			exit(EXIT_FAILURE);
		}
		sscanf(str, "%c", &option);

		if (initialized == false) {
			fd = qrc_udriver_open(dev);
			if (fd < 0) {
				fprintf(stderr, "Failed to open device\n");
				free(rx_buffer);
				free(tx_buffer);
				exit(EXIT_FAILURE);
			}
			initialized = true;
		}
		if (qrc_udriver_tcflsh(fd) == -1) {
			printf("Fail to TCIOFLUSH\n");
			free(rx_buffer);
			free(tx_buffer);
			qrc_udriver_close(fd);
			return -1;
		}
		switch (option) {
			case '1':
				printf("\nstart write and read test\n");

				generate_random_string(tx_buffer, length);
				num = qrc_udriver_write(fd, tx_buffer, length);
				if (num == -1) {
					fprintf(stderr, "Failed to write data\n");
					break;
				} else {
					printf("tx_buffer:%s, num = %zd\n", tx_buffer, num);
				}
				num = -1;
				sum = 0;
				while (num == -1 || num == 0) {		//Wait to receive data
					num = qrc_udriver_read(fd, rx_buffer, length);
					if (num > 0) {
						printf("read_buffer:%s, num = %zd\n", rx_buffer, num);
						if (strncmp(rx_buffer, &tx_buffer[sum], num)) {
							fprintf(stderr, "Failed! The sent and received buffers are different\n");
							break;
						}
						sum += num;
						num = -1;
						memset(rx_buffer, 0, length);
						printf("The total number of bytes received:%zd\n",sum);
					}
					if (sum == length)
						break;
				}
				printf("Successful!Pass write and read test\n\n");
				break;
			case '2':
				printf("\nstart FIONREAD and TCFLSH test\n");
				int bytes_available = 0;
				generate_random_string(tx_buffer, length);
				num = qrc_udriver_write(fd, tx_buffer, length);
				if (num == -1) {
					fprintf(stderr, "Failed to write data\n");
					break;
				} else {
					printf("tx_buffer:%s, num = %zd\n", tx_buffer, num);
				}
				while (bytes_available != (int)length){	//Wait to receive data
					int temp = bytes_available;
					qrc_udriver_fionread(fd, &bytes_available);
					if (bytes_available != temp)
						printf("bytes_available: %d\n", bytes_available);
				}
				printf("execute tcflsh\n");
				if (qrc_udriver_tcflsh(fd) == -1) {
					printf("Fail to TCIOFLUSH\n");
					free(rx_buffer);
					free(tx_buffer);
					return -1;
				}
				if (qrc_udriver_fionread(fd, &bytes_available) == -1) {
					printf("Fail to get available bytes\n");
					free(rx_buffer);
					free(tx_buffer);
					return -1;
				}
				printf("bytes_available: %d\n", bytes_available);
				if (bytes_available == 0)
					printf("Successful!Pass FIONREAD and TCFLSH test\n\n");
				break;
			case '3':
				printf("\nstart stability test\n");
				if (initialized == true) {
					qrc_udriver_close(fd);
					initialized = false;
				}
				count = 0;
				while(count < times) {
					sum = 0;
					fd = qrc_udriver_open(dev);
					generate_random_string(tx_buffer, length);
					num = qrc_udriver_write(fd, tx_buffer, length);
					if (num == -1) {
						fprintf(stderr, "Failed to write data\n");
						break;
					}
					num = -1;
					while (num == -1 || num == 0) {		//Wait to receive data
						num = qrc_udriver_read(fd, rx_buffer, length);
						if (num > 0) {
							if (strncmp(rx_buffer, &tx_buffer[sum], num)) {
								fprintf(stderr, "Failed! The sent and received buffers are different, count:%d\n", count);
								break;
							}
							sum += num;
							num = -1;
						}
						if (sum == length)
							break;
					}
					printf("The total number of bytes received:%zd, count:%d\n", sum, count);
					qrc_udriver_close(fd);
					usleep(500000);
					count ++;
				}
				printf("Successful! Pass stability test\n\n");
				break;
			case '4':
				printf("\nstart reset test\n");
				if (initialized == true) {
					qrc_udriver_close(fd);
					initialized = false;
				}
				qrc_mcb_reset(gpiochip, reset_gpio);
				printf("reset\n");
				break;
			case '5':
				printf("\nstart speed test\n");
				printf("Calculate the speed of writing %zu bytes and reading back %zu bytes\n", length, length);
				struct timespec start, end;
				double execution_time, total_execution_time = 0;
				count = 0;
				while(count < times) {
					generate_random_string(tx_buffer, length);
					sum = 0;
					clock_gettime(CLOCK_MONOTONIC, &start);
					num = qrc_udriver_write(fd, tx_buffer, length);
					if (num == -1) {
						fprintf(stderr, "Failed to write data\n");
						break;
					}
					num = -1;
					while (num == -1 || num == 0) {		//Wait to receive data
						num = qrc_udriver_read(fd, rx_buffer, length);
						if (num > 0) {
							sum += num;
							num = -1;
						}
						if (sum == length)
							break;
					}
					clock_gettime(CLOCK_MONOTONIC, &end);
					execution_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
					total_execution_time += execution_time;
					count ++;
				}
				double average_execution_time = total_execution_time / count;
				printf("The average execution time is %f seconds\n", average_execution_time);
				printf("Successful!Pass speed test\n\n");
				break;
			case 'c':
				printf("Exiting program...\n");
				free(rx_buffer);
				free(tx_buffer);
				exit(EXIT_SUCCESS);
			default:
				printf("Invalid option! Please try again.\n\n");
		}

	}

	return 0;
}


