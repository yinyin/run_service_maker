#include "run_service.h"

int main(int argc, char ** argv) {
	char * const SERVICE_SLEEP_ARGV[] = {"sleep", "10", NULL};
	ServiceDefinition SERVICE_SLEEP = {.process_id=0,
			.started_at=0,
			.service_name="sleep-10",
			.work_directory="/tmp",
			.executable_path="/bin/sleep",
			.execute_argv=SERVICE_SLEEP_ARGV};
	ServiceDefinition * const services[] = {
		&SERVICE_SLEEP, NULL};
	run_services(services);
	return 0;
}

