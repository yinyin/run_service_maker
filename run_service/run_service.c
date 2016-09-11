#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "run_service.h"


#define MIN_RESTART_SECOND 30

#define KILL_SUBJECT 0

#define RECORD_ERR(msg, srcfile, srcline) {	\
	int errnum;	\
	errnum = errno;	\
	fprintf(stderr, "ERR: %s: %s @[%s:%d]\n", msg, strerror(errnum), srcfile, srcline);	\
}


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

static void start_service(ServiceDefinition * serv) {
	pid_t child_pid;
	time_t now_tstamp;

	time(&now_tstamp);
	if ((now_tstamp - serv->started_at) < MIN_RESTART_SECOND) {
		fprintf(stderr, "ERR: restart too frequently (service-name: %s) @[%s:%d]\n", serv->service_name, __FILE__, __LINE__);
		return;
	}
	serv->started_at = now_tstamp;
	if((pid_t)(0) != serv->process_id) {
		fprintf(stderr, "WARN: cannot start service with process-id == 0 @[%s:%d]\n", __FILE__, __LINE__);
		return;
	}
	child_pid = fork();
	if((pid_t)(-1) == child_pid) {
		RECORD_ERR("cannot fork child process", __FILE__, __LINE__);
		return;
	} else if(0 != child_pid) {
		serv->process_id = child_pid;
		return;
	}
	/* child process */
	if(0 != chdir(serv->work_directory)) {
		RECORD_ERR("failed on changing work directory", __FILE__, __LINE__);
		exit(17);
	}
	execv(serv->executable_path, serv->execute_argv);
	RECORD_ERR("cannot execute target program", __FILE__, __LINE__);
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
			RECORD_ERR("failed on waitpid", __FILE__, __LINE__);
			return;
		} else  {
			int i;
			for (i = 0; (NULL != services[i]); i++) {
				ServiceDefinition * serv = services[i];
				if (kid_pid == serv->process_id) {
					serv->process_id = (pid_t)(0);
					if (WIFEXITED(prg_exitcode)) {
						fprintf(stderr, "WARN: service stopped (ret-code=%d, service-name: %s) @[%s:%d]\n", WEXITSTATUS(prg_exitcode), serv->service_name, __FILE__, __LINE__);
					} else {
						fprintf(stderr, "WARN: service stopped (terminate-signal=%d, service-name: %s) @[%s:%d]\n", WTERMSIG(prg_exitcode), serv->service_name, __FILE__, __LINE__);
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
	kill(KILL_SUBJECT, SIGTERM);
	for (i = 100; i > 0; i--) {
		check_child_process(services);
		if (0 == check_services_stopped(services)) {
			return;
		}
		sleep(2);
	}
	kill(KILL_SUBJECT, SIGKILL);
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
	fprintf(stderr, "INFO: stopping services");
	sigaction(SIGINT, &prev_sigint_signal_action, NULL);
	sigaction(SIGTERM, &prev_sigterm_signal_action, NULL);
	stop_services(services);
	return;
}
