/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * log_process.cpp - Logging in isolated child process test
 */

#include <fcntl.h>
#include <iostream>
#include <random>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <libcamera/camera_manager.h>
#include <libcamera/event_dispatcher.h>
#include <libcamera/logging.h>
#include <libcamera/timer.h>

#include "log.h"
#include "process.h"
#include "test.h"
#include "utils.h"

using namespace std;
using namespace libcamera;

static const string message("hello from the child");

LOG_DEFINE_CATEGORY(LogProcessTest)

class LogProcessTestChild
{
public:
	int run(int status, int num)
	{
		usleep(50000);

		string logPath = "/tmp/libcamera.worker.test." +
				 to_string(num) + ".log";
		if (logSetFile(logPath.c_str()) < 0)
			return TestSkip;

		LOG(LogProcessTest, Warning) << message;

		return status;
	}
};

class LogProcessTest : public Test
{
protected:
	int init()
	{
		random_device random;
		num_ = random();
		logPath_ = "/tmp/libcamera.worker.test." +
			   to_string(num_) + ".log";

		proc_.finished.connect(this, &LogProcessTest::procFinished);
		return 0;
	}

	int run()
	{
		EventDispatcher *dispatcher = CameraManager::instance()->eventDispatcher();
		Timer timeout;

		int exitCode = 42;
		vector<std::string> args;
		args.push_back(to_string(exitCode));
		args.push_back(to_string(num_));
		int ret = proc_.start("/proc/self/exe", args);
		if (ret) {
			cerr << "failed to start process" << endl;
			return TestFail;
		}

		timeout.start(200);
		while (timeout.isRunning())
			dispatcher->processEvents();

		if (exitStatus_ != Process::NormalExit) {
			cerr << "process did not exit normally" << endl;
			return TestFail;
		}

		if (exitCode_ == TestSkip)
			return TestSkip;

		if (exitCode_ != exitCode) {
			cerr << "exit code should be " << exitCode
			     << ", actual is " << exitCode_ << endl;
			return TestFail;
		}

		int fd = open(logPath_.c_str(), O_RDONLY, S_IRUSR);
		if (fd < 0) {
			cerr << "failed to open tmp log file" << endl;
			return TestFail;
		}

		char buf[200];
		memset(buf, 0, sizeof(buf));
		if (read(fd, buf, sizeof(buf)) < 0) {
			cerr << "Failed to read tmp log file" << endl;
			return TestFail;
		}
		close(fd);

		string str(buf);
		if (str.find(message) == string::npos)
			return TestFail;

		return TestPass;
	}

	void cleanup()
	{
		unlink(logPath_.c_str());
	}

private:
	void procFinished(Process *proc, enum Process::ExitStatus exitStatus, int exitCode)
	{
		exitStatus_ = exitStatus;
		exitCode_ = exitCode;
	}

	Process proc_;
	Process::ExitStatus exitStatus_;
	string logPath_;
	int exitCode_;
	int num_;
};

/*
 * Can't use TEST_REGISTER() as single binary needs to act as both
 * parent and child processes.
 */
int main(int argc, char **argv)
{
	if (argc == 3) {
		int status = std::stoi(argv[1]);
		int num = std::stoi(argv[2]);
		LogProcessTestChild child;
		return child.run(status, num);
	}

	return LogProcessTest().execute();
}
