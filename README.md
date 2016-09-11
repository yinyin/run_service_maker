# Service Runner Maker

This project aimed at making a service runner for any long running program.

The generated service runner can be use as PID-1 of docker application.
(if you really need to package several processes into one container)

## Usage

There are a few step to get service runner built:

1. prepare service definition file
2. generate main function
3. build `run_service` executable

The service definition file is a JSON file. Each process will need one service definition file.
Please refer to `service/example_sleep-10.json` for example of service definition file.

After service definition file is prepared you can run `maker/make_main.py` as follow to generate main function of service runner.

~~~
python maker/make_main.py services/example_sleep-10.json services/example_ls-tmp.json
~~~

The code file of main function will be put at `run_service/main.c`.

The service runner executable can be build with `make.sh` script or the `Makefile` in the `run_service/` folder.
Result binary will be placed at `run_service/run_service`.

Run the result binary will start configured programs. The program will be restarted when it is stopped.
SIGTERM signal will be emit to running program when SIGTERM or SIGINT are received.


## Run as PID-1 in Container

Define `KILL_SUBJECT_PID` as -1 to make service runner stop *every* process inside the container might be a good idea
for building a service runner to running as PID-1 of a container.


## License

This project is licensed under `The MIT License`.

> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
