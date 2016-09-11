#!/usr/bin/env python2
# -*- coding: utf-8 -*-

""" Maker for Main Function of Run-Service """

import sys
from json import load as json_load

def _c_char_escape(c):
	if "\t" == c:
		return "\\t"
	if "\\" == c:
		return "\\\\"
	if "\"" == c:
		return "\\\""
	if "'" == c:
		return "\\'"
	return c
# ### def _c_char_escape

def _c_string(s):
	return '"' + "".join(map(_c_char_escape, s)) + '"'
# ### def _c_string

class ServiceDefinition(object):
	def __init__(self, name, workdirectory, command, *args, **kwds):
		super(ServiceDefinition, self).__init__(*args, **kwds)
		self.name = name
		self.workdirectory = workdirectory
		self.command = command
	# ### def __init__

	def get_sanitized_name(self):
		return "".join(map(lambda c: "_" if (c not in "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_") else c, self.name.upper()))
	# ### def get_sanitized_name
# ### class ServiceDefinition

def load_service_definition(file_path):
	with open(file_path, "r") as fp:
		c = json_load(fp)
	serv_name = str(c["name"])
	serv_workdirectory = str(c["work_directory"])
	serv_command = [str(a) for a in c["command"]]
	if "/" != serv_command[0][0]:
		print "WARN: 1st argument of command must be absolute path of executable - %r" (serv_command[0],)
	return ServiceDefinition(serv_name, serv_workdirectory, serv_command)
# ### def load_service_definition

def generate_service_definition_structure(serv):
	serv_struct_name = "SERVICE_"+ serv.get_sanitized_name()
	serv_argv_name = serv_struct_name + "_ARGV"
	
	argv_content = list(map(_c_string, serv.command))
	argv_content.append("NULL")
	result_code = [
		"char * const " + serv_argv_name + "[] = {" + ", ".join(argv_content) + "};",
		"ServiceDefinition " + serv_struct_name + " = {.process_id=0,",
		"\t\t.started_at=0,",
		"\t\t.service_name=" + _c_string(serv.name) + ",",
		"\t\t.work_directory=" + _c_string(serv.workdirectory) + ",",
		"\t\t.executable_path=" + argv_content[0] + ",",
		"\t\t.execute_argv=" + serv_argv_name + "};", ]
	return (serv_struct_name, result_code,)
# ### def generate_service_definition_structure

def generate_main_function(service_structs):
	result_code = [
		"#include \"run_service.h\"",
		"",
		"/* This is generated file */",
		"",
		"int main(int argc, char ** argv) {", ]
	serv_struct_ptrs = []
	for serv_struct_name, serv_def_code, in service_structs:
		serv_struct_ptrs.append("&" + serv_struct_name)
		result_code.extend(map(lambda l: "\t" + l, serv_def_code))
	serv_struct_ptrs.append("NULL")
	result_code.append("\tServiceDefinition * const services[] = {")
	result_code.append("\t\t" + ", ".join(serv_struct_ptrs) + "};")
	result_code.append("\trun_services(services);")
	result_code.append("\treturn 0;")
	result_code.append("}")
	result_code.append("")
	return result_code
# ### def generate_main_function


def _main():
	if len(sys.argv) < 2:
		print "Argument [SERVICE_DEFINITION_FILES ...]"
		return 1
	service_file = sys.argv[1:]
	service_definitions = map(load_service_definition, service_file)
	service_structs = map(generate_service_definition_structure, service_definitions)
	main_code = generate_main_function(service_structs)
	with open("run_service/main.c", "w") as fp:
		for l in main_code:
			fp.write(l)
			fp.write("\n")
	return 0
# ### def _main

if "__main__" == __name__:
	sys.exit(_main())
# >>> if "__main__" == __name__:

# vim: ts=4 sw=4 ai nowrap
