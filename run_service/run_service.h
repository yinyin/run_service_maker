#ifndef _X_RUN_SERVICE_
#define _X_RUN_SERVICE_ 1

#include <unistd.h>
#include <time.h>


typedef struct _T_ServiceDefinition {
	pid_t process_id;
	time_t started_at;
	const char * service_name;
	const char * work_directory;
	const char * executable_path;
	char * const * execute_argv;
} ServiceDefinition;


void run_services(ServiceDefinition * const services[]);


#endif /* _X_RUN_SERVICE_ */
