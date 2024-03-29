/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * unixsocket.cpp - Unix socket IPC test
 */

#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libcamera/camera_manager.h>
#include <libcamera/event_dispatcher.h>
#include <libcamera/timer.h>

#include "ipc_unixsocket.h"
#include "test.h"
#include "utils.h"

#define CMD_CLOSE	0
#define CMD_REVERSE	1
#define CMD_LEN_CALC	2
#define CMD_LEN_CMP	3
#define CMD_JOIN	4

using namespace std;
using namespace libcamera;

int calculateLength(int fd)
{
	lseek(fd, 0, 0);
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, 0);

	return size;
}

class UnixSocketTestSlave
{
public:
	UnixSocketTestSlave()
		: exitCode_(EXIT_FAILURE), exit_(false)
	{
		dispatcher_ = CameraManager::instance()->eventDispatcher();
		ipc_.readyRead.connect(this, &UnixSocketTestSlave::readyRead);
	}

	int run(int fd)
	{
		if (ipc_.bind(fd)) {
			cerr << "Failed to connect to IPC channel" << endl;
			return EXIT_FAILURE;
		}

		while (!exit_)
			dispatcher_->processEvents();

		ipc_.close();

		return exitCode_;
	}

private:
	void readyRead(IPCUnixSocket *ipc)
	{
		IPCUnixSocket::Payload message, response;
		int ret;

		ret = ipc->receive(&message);
		if (ret) {
			cerr << "Receive message failed: " << ret << endl;
			return;
		}

		const uint8_t cmd = message.data[0];

		switch (cmd) {
		case CMD_CLOSE:
			stop(0);
			break;

		case CMD_REVERSE: {
			response.data = message.data;
			std::reverse(response.data.begin() + 1, response.data.end());

			ret = ipc_.send(response);
			if (ret < 0) {
				cerr << "Reverse failed" << endl;
				stop(ret);
			}
			break;
		}

		case CMD_LEN_CALC: {
			int size = 0;
			for (int fd : message.fds)
				size += calculateLength(fd);

			response.data.resize(1 + sizeof(size));
			response.data[0] = cmd;
			memcpy(response.data.data() + 1, &size, sizeof(size));

			ret = ipc_.send(response);
			if (ret < 0) {
				cerr << "Calc failed" << endl;
				stop(ret);
			}
			break;
		}

		case CMD_LEN_CMP: {
			int size = 0;
			for (int fd : message.fds)
				size += calculateLength(fd);

			int cmp;
			memcpy(&cmp, message.data.data() + 1, sizeof(cmp));

			if (cmp != size) {
				cerr << "Compare failed" << endl;
				stop(-ERANGE);
			}
			break;
		}

		case CMD_JOIN: {
			int outfd = open("/tmp", O_TMPFILE | O_RDWR,
					 S_IRUSR | S_IWUSR);
			if (outfd < 0) {
				cerr << "Create out file failed" << endl;
				stop(outfd);
				return;
			}

			for (int fd : message.fds) {
				while (true) {
					char buf[32];
					ssize_t num = read(fd, &buf, sizeof(buf));

					if (num < 0) {
						cerr << "Read failed" << endl;
						stop(-EIO);
						return;
					} else if (!num)
						break;

					if (write(outfd, buf, num) < 0) {
						cerr << "Write failed" << endl;
						stop(-EIO);
						return;
					}
				}

				close(fd);
			}

			lseek(outfd, 0, 0);
			response.data.push_back(CMD_JOIN);
			response.fds.push_back(outfd);

			ret = ipc_.send(response);
			if (ret < 0) {
				cerr << "Join failed" << endl;
				stop(ret);
			}

			close(outfd);

			break;
		}

		default:
			cerr << "Unknown command " << cmd << endl;
			stop(-EINVAL);
			break;
		}
	}

	void stop(int code)
	{
		exitCode_ = code;
		exit_ = true;
	}

	IPCUnixSocket ipc_;
	EventDispatcher *dispatcher_;
	int exitCode_;
	bool exit_;
};

class UnixSocketTest : public Test
{
protected:
	int slaveStart(int fd)
	{
		pid_ = fork();

		if (pid_ == -1)
			return TestFail;

		if (!pid_) {
			std::string arg = std::to_string(fd);
			execl("/proc/self/exe", "/proc/self/exe",
			      arg.c_str(), nullptr);

			/* Only get here if exec fails. */
			exit(TestFail);
		}

		return TestPass;
	}

	int slaveStop()
	{
		int status;

		if (pid_ < 0)
			return TestFail;

		if (waitpid(pid_, &status, 0) < 0)
			return TestFail;

		if (!WIFEXITED(status) || WEXITSTATUS(status))
			return TestFail;

		return TestPass;
	}

	int testReverse()
	{
		IPCUnixSocket::Payload message, response;
		int ret;

		message.data = { CMD_REVERSE, 1, 2, 3, 4, 5 };

		ret = call(message, &response);
		if (ret)
			return ret;

		std::reverse(response.data.begin() + 1, response.data.end());
		if (message.data != response.data)
			return TestFail;

		return 0;
	}

	int testEmptyFail()
	{
		IPCUnixSocket::Payload message;

		return ipc_.send(message) != -EINVAL;
	}

	int testCalc()
	{
		IPCUnixSocket::Payload message, response;
		int sizeOut, sizeIn, ret;

		sizeOut = prepareFDs(&message, 2);
		if (sizeOut < 0)
			return sizeOut;

		message.data.push_back(CMD_LEN_CALC);

		ret = call(message, &response);
		if (ret)
			return ret;

		memcpy(&sizeIn, response.data.data() + 1, sizeof(sizeIn));
		if (sizeOut != sizeIn)
			return TestFail;

		return 0;
	}

	int testCmp()
	{
		IPCUnixSocket::Payload message;
		int size;

		size = prepareFDs(&message, 7);
		if (size < 0)
			return size;

		message.data.resize(1 + sizeof(size));
		message.data[0] = CMD_LEN_CMP;
		memcpy(message.data.data() + 1, &size, sizeof(size));

		if (ipc_.send(message))
			return TestFail;

		return 0;
	}

	int testFdOrder()
	{
		IPCUnixSocket::Payload message, response;
		int ret;

		static const char *strings[2] = {
			"Foo",
			"Bar",
		};
		int fds[2];

		for (unsigned int i = 0; i < ARRAY_SIZE(strings); i++) {
			unsigned int len = strlen(strings[i]);

			fds[i] = open("/tmp", O_TMPFILE | O_RDWR,
				      S_IRUSR | S_IWUSR);
			if (fds[i] < 0)
				return TestFail;

			ret = write(fds[i], strings[i], len);
			if (ret < 0)
				return TestFail;

			lseek(fds[i], 0, 0);
			message.fds.push_back(fds[i]);
		}

		message.data.push_back(CMD_JOIN);

		ret = call(message, &response);
		if (ret)
			return ret;

		for (unsigned int i = 0; i < ARRAY_SIZE(strings); i++) {
			unsigned int len = strlen(strings[i]);
			char buf[len];

			close(fds[i]);

			if (read(response.fds[0], &buf, len) <= 0)
				return TestFail;

			if (memcmp(buf, strings[i], len))
				return TestFail;
		}

		close(response.fds[0]);

		return 0;
	}

	int init()
	{
		callResponse_ = nullptr;
		return 0;
	}

	int run()
	{
		int slavefd = ipc_.create();
		if (slavefd < 0)
			return TestFail;

		if (slaveStart(slavefd)) {
			cerr << "Failed to start slave" << endl;
			return TestFail;
		}

		ipc_.readyRead.connect(this, &UnixSocketTest::readyRead);

		/* Test reversing a string, this test sending only data. */
		if (testReverse()) {
			cerr << "Reveres array test failed" << endl;
			return TestFail;
		}

		/* Test that an empty message fails. */
		if (testEmptyFail()) {
			cerr << "Empty message test failed" << endl;
			return TestFail;
		}

		/* Test offloading a calculation, this test sending only FDs. */
		if (testCalc()) {
			cerr << "Calc test failed" << endl;
			return TestFail;
		}

		/* Test fire and forget, this tests sending data and FDs. */
		if (testCmp()) {
			cerr << "Cmp test failed" << endl;
			return TestFail;
		}

		/* Test order of file descriptors. */
		if (testFdOrder()) {
			cerr << "fd order test failed" << endl;
			return TestFail;
		}

		/* Close slave connection. */
		IPCUnixSocket::Payload close;
		close.data.push_back(CMD_CLOSE);
		if (ipc_.send(close)) {
			cerr << "Closing IPC channel failed" << endl;
			return TestFail;
		}

		ipc_.close();
		if (slaveStop()) {
			cerr << "Failed to stop slave" << endl;
			return TestFail;
		}

		return TestPass;
	}

private:
	int call(const IPCUnixSocket::Payload &message, IPCUnixSocket::Payload *response)
	{
		Timer timeout;
		int ret;

		callDone_ = false;
		callResponse_ = response;

		ret = ipc_.send(message);
		if (ret)
			return ret;

		timeout.start(200);
		while (!callDone_) {
			if (!timeout.isRunning()) {
				cerr << "Call timeout!" << endl;
				callResponse_ = nullptr;
				return -ETIMEDOUT;
			}

			CameraManager::instance()->eventDispatcher()->processEvents();
		}

		callResponse_ = nullptr;

		return 0;
	}

	void readyRead(IPCUnixSocket *ipc)
	{
		if (!callResponse_) {
			cerr << "Read ready without expecting data, fail." << endl;
			return;
		}

		if (ipc->receive(callResponse_)) {
			cerr << "Receive message failed" << endl;
			return;
		}

		callDone_ = true;
	}

	int prepareFDs(IPCUnixSocket::Payload *message, unsigned int num)
	{
		int fd = open("/proc/self/exe", O_RDONLY);
		if (fd < 0)
			return fd;

		int size = 0;
		for (unsigned int i = 0; i < num; i++) {
			int clone = dup(fd);
			if (clone < 0)
				return clone;

			size += calculateLength(clone);
			message->fds.push_back(clone);
		}

		close(fd);

		return size;
	}

	pid_t pid_;
	IPCUnixSocket ipc_;
	bool callDone_;
	IPCUnixSocket::Payload *callResponse_;
};

/*
 * Can't use TEST_REGISTER() as single binary needs to act as both proxy
 * master and slave.
 */
int main(int argc, char **argv)
{
	if (argc == 2) {
		int ipcfd = std::stoi(argv[1]);
		UnixSocketTestSlave slave;
		return slave.run(ipcfd);
	}

	return UnixSocketTest().execute();
}
