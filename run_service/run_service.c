#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "run_service.h"


#define MIN_RESTART_SECOND 30

#ifndef KILL_SUBJECT_PID
	#define KILL_SUBJECT_PID 0
#endif

#ifdef WITH_SYSLOG
	#include <syslog.h>

	#define RECORD_OPEN() { openlog(WITH_SYSLOG, LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER); }

	#define RECORD_INFO(srcfile, srcline, msg, ...) {	\
		syslog(LOG_NOTICE, msg " @[%s:%d]\n", ##__VA_ARGS__, srcfile, srcline);	\
	}

	#define RECORD_WARN(srcfile, srcline, msg, ...) {	\
		syslog(LOG_WARNING, msg " @[%s:%d]\n", ##__VA_ARGS__, srcfile, srcline);	\
	}

	#define RECORD_ERR(srcfile, srcline, msg, ...) {	\
		int errnum;	\
		errnum = errno;	\
		syslog(LOG_ERR, msg ": %s @[%s:%d]\n", ##__VA_ARGS__, strerror(errnum), srcfile, srcline);	\
	}

	#define RECORD_CLOSE() { closelog(); }
#else
	#define RECORD_OPEN() { }

	#define RECORD_INFO(srcfile, srcline, msg, ...) {	\
		fprintf(stderr, "INFO: " msg " @[%s:%d]\n", ##__VA_ARGS__, srcfile, srcline);	\
	}

	#define RECORD_WARN(srcfile, srcline, msg, ...) {	\
		fprintf(stderr, "WARN: " msg " @[%s:%d]\n", ##__VA_ARGS__, srcfile, srcline);	\
	}

	#define RECORD_ERR(srcfile, srcline, msg, ...) {	\
		int errnum;	\
		errnum = errno;	\
		fprintf(stderr, "ERR: " msg ": %s @[%s:%d]\n", ##__VA_ARGS__, strerror(errnum), srcfile, srcline);	\
	}

	#define RECORD_CLOSE() { }
#endif



static volatile int flag_stop_service = 0;

static void signal_handler_to_stop(int signum, siginfo_t *info, void *ptr) {
	flag_stop_service = 1;
	fprintf(stderr, "INFO: received signal %d\n", signum);
}


#if 0
static void print_service(const ServiceDefinition * serv) {
	fprintf(stderr, "ProcessID: %d\n"
					"StartAt: %d\n"
					"ServiceName: %s\n"
					"WorkDirectory: %s\n"
					"ExecutablePath: %s\n",
	(int)(serv->process_id), (int)(serv->started_at), serv->service_name, serv->work_directory, serv->executable_path);
}
#endif

static int close_fd_impl_procfs_fdfolder(const char *fdfolder_path)
{
	int dirfd_val;
	DIR *dirp;
	struct dirent *p;

	if(NULL == (dirp = opendir(fdfolder_path))) {
		fprintf(stderr, "WARN: cannot open file descriptor folder @[%s:%d]", __FILE__, __LINE__);
		return 1;
	}
	dirfd_val = dirfd(dirp);
	while(NULL != (p = readdir(dirp))) {
		int fd_val;
		char *endp;
		fd_val = (int)(strtol(p->d_name, &endp, 10));
		if( (p->d_name != endp) && ('\0' == *endp) && (fd_val != dirfd_val) && (fd_val > 2) ) {
			if(0 != close(fd_val)) {
				fprintf(stderr, "WARN: failed on close file descriptor (fd=%d) @[%s:%d]", fd_val, __FILE__, __LINE__);
			}
		}
	}

	if(0 != closedir(dirp)) {
		fprintf(stderr, "WARN: failed on close procfs file descriptor folder @[%s:%d]", __FILE__, __LINE__);
	}

	return 0;
}

static void start_service(ServiceDefinition * serv) {
	pid_t child_pid;
	time_t now_tstamp;

	time(&now_tstamp);
	if ((now_tstamp - serv->started_at) < MIN_RESTART_SECOND) {
		RECORD_WARN(__FILE__, __LINE__, "restart too frequently (service-name: %s)", serv->service_name);
		return;
	}
	serv->started_at = now_tstamp;
	if((pid_t)(0) != serv->process_id) {
		RECORD_WARN(__FILE__, __LINE__, "cannot start service with process-id == 0");
		return;
	}
	child_pid = fork();
	if((pid_t)(-1) == child_pid) {
		RECORD_ERR(__FILE__, __LINE__, "cannot fork child process");
		return;
	} else if(0 != child_pid) {
		serv->process_id = child_pid;
		return;
	}
	/* child process */
#if __APPLE__
	close_fd_impl_procfs_fdfolder("/dev/fd");
#elif __linux__ || __linux
	close_fd_impl_procfs_fdfolder("/proc/self/fd");
#endif
	if(0 != chdir(serv->work_directory)) {
		RECORD_ERR(__FILE__, __LINE__, "failed on changing work directory");
		exit(17);
	}
	if(NULL != serv->prepare_function) {
		int prepare_result = (*(serv->prepare_function))();
		if(0 != prepare_result) {
			RECORD_ERR(__FILE__, __LINE__, "cannot prepare runtime environment");
			exit(18);
		}
	}
	execv(serv->executable_path, serv->execute_argv);
	RECORD_ERR(__FILE__, __LINE__, "cannot execute target program");
	exit(20);
}

static void check_child_process(ServiceDefinition * const services[]) {
	pid_t kid_pid;
	do {
		int prg_exitcode;
		kid_pid = waitpid((pid_t)(-1), &prg_exitcode, WNOHANG);
		if((pid_t)(0) == kid_pid) {
			return;
		} else if((pid_t)(-1) == kid_pid) {
			RECORD_ERR(__FILE__, __LINE__, "failed on waitpid");
			return;
		} else  {
			int i;
			for (i = 0; (NULL != services[i]); i++) {
				ServiceDefinition * serv = services[i];
				if (kid_pid == serv->process_id) {
					serv->process_id = (pid_t)(0);
					if (WIFEXITED(prg_exitcode)) {
						RECORD_WARN(__FILE__, __LINE__, "service stopped (ret-code=%d, service-name: %s)", WEXITSTATUS(prg_exitcode), serv->service_name);
					} else {
						RECORD_WARN(__FILE__, __LINE__, "service stopped (terminate-signal=%d, service-name: %s)", WTERMSIG(prg_exitcode), serv->service_name);
					}
					break;
				}
			}
		}
	} while(kid_pid > 0);
}

static void start_idle_services(ServiceDefinition * const services[]) {
	int i;
	for (i = 0; (NULL != services[i]); i++) {
		ServiceDefinition * serv = services[i];
		if ((pid_t)(0) == serv->process_id) {
			start_service(serv);
		}
	}
}

static int check_services_stopped(ServiceDefinition * const services[]) {
	int i;
	for (i = 0; (NULL != services[i]); i++) {
		ServiceDefinition * serv = services[i];
		if ((pid_t)(0) != serv->process_id) {
			return 1;
		}
	}
	return 0;
}

static void stop_services(ServiceDefinition * const services[]) {
	int i;
	kill(KILL_SUBJECT_PID, SIGTERM);
	for (i = 100; i > 0; i--) {
		check_child_process(services);
		if (0 == check_services_stopped(services)) {
			return;
		}
		sleep(2);
	}
	kill(KILL_SUBJECT_PID, SIGKILL);
	for (i = 10; i > 0; i--) {
		check_child_process(services);
		if (0 == check_services_stopped(services)) {
			return;
		}
		sleep(1);
	}
}


void run_services(ServiceDefinition * const services[]) {
	struct sigaction signal_action;
	struct sigaction prev_sigint_signal_action;
	struct sigaction prev_sigterm_signal_action;
	RECORD_OPEN();
	memset(&signal_action, 0, sizeof(signal_action));
	memset(&prev_sigint_signal_action, 0, sizeof(prev_sigint_signal_action));
	memset(&prev_sigterm_signal_action, 0, sizeof(prev_sigterm_signal_action));
	signal_action.sa_sigaction = signal_handler_to_stop;
	signal_action.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &signal_action, &prev_sigint_signal_action);
	sigaction(SIGTERM, &signal_action, &prev_sigterm_signal_action);
	while (0 == flag_stop_service) {
		start_idle_services(services);
		check_child_process(services);
		sleep(10);
	}
	RECORD_INFO(__FILE__, __LINE__, "stopping services");
	sigaction(SIGINT, &prev_sigint_signal_action, NULL);
	sigaction(SIGTERM, &prev_sigterm_signal_action, NULL);
	stop_services(services);
	RECORD_CLOSE();
	return;
}
