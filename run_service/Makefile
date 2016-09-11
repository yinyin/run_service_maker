
all: run_service

run_service: run_service.c run_service.h main.c
	cc -o run_service -Wall run_service.c main.c

clean:
	$(RM) *.o
	$(RM) run_service

