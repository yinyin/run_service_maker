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

def _c_get_function_pointer(f):
	if f:
		return "&" + f
	return "NULL"
# ### def _c_get_function_pointer

def _indent_code(code_lines, prepend="", indent="\t"):
	for l in code_lines:
		if isinstance(l, basestring):
			if l:
				yield prepend + l
			else:
				yield None
		else:
			for ll in _indent_code(l, prepend + indent, indent):
				yield ll
# ### def _indent_code

class ServiceDefinition(object):
	def __init__(self, name, workdirectory, command, resourcelimit, runasuser, *args, **kwds):
		super(ServiceDefinition, self).__init__(*args, **kwds)
		self.name = name
		self.workdirectory = workdirectory
		self.command = command
		self.resourcelimit = resourcelimit
		self.runasuser = runasuser
	# ### def __init__

	def get_sanitized_name(self):
		return "".join(map(lambda c: "_" if (c not in "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_") else c, self.name.upper()))
	# ### def get_sanitized_name
# ### class ServiceDefinition

def load_resource_limit_definition(cmap):
	if not cmap:
		return None
	result = []
	for resource_name, limit_value, in cmap.iteritems():
		resource_name = str(resource_name).upper()
		if "INF" == limit_value:
			limit_value = "RLIM_INFINITY"
		else:
			limit_value = int(limit_value)
		result.append((resource_name, limit_value,))
	return result
# ### def load_resource_limit_definition

def load_service_definition(file_path):
	with open(file_path, "r") as fp:
		c = json_load(fp)
	serv_name = str(c["name"])
	serv_workdirectory = str(c["work_directory"])
	serv_command = [str(a) for a in c["command"]]
	if "/" != serv_command[0][0]:
		print "WARN: 1st argument of command must be absolute path of executable - %r" (serv_command[0],)
	serv_resourcelimit = load_resource_limit_definition(c.get("limit"))
	serv_runasuser = c.get("run_as")
	return ServiceDefinition(serv_name, serv_workdirectory, serv_command, serv_resourcelimit, serv_runasuser)
# ### def load_service_definition

def make_error_verbose_code_block(message_text, return_code):
	t = message_text + ': %s @[%s:%d]\n'
	return (
	"int errnum;",
	"errnum = errno;"
	"fprintf(stderr, " + _c_string(t) + ", strerror(errnum), __FILE__, __LINE__);",
	"return " + str(return_code) + ";", )
# ### def make_error_verbose_code_block

def make_prepare_resource_limit(serv):
	if not serv.resourcelimit:
		return None
	required_include = ("sys/time.h", "sys/resource.h", "errno.h", "string.h", )
	err_return_code = 21
	struct_code = []
	runset_code = []
	for resource_name, resource_value, in serv.resourcelimit:
		n = "res_" + resource_name.lower()
		v = str(resource_value)
		struct_code.append("struct rlimit " + n + " = {.rlim_cur=" + v + ", .rlim_max=" + v + "};")
		runset_code.append("if(0 != setrlimit(" + resource_name + ", &" + n + ")) {")
		runset_code.append(make_error_verbose_code_block("ERR: cannot set resource " + resource_name, err_return_code))
		runset_code.append("}")
		err_return_code = err_return_code + 1
	result_code = struct_code + runset_code
	return (required_include, result_code, )
# ### def make_prepare_resource_limit

def make_prepare_run_as_user(serv):
	if not serv.runasuser:
		return None
	required_include = ("sys/types.h", "pwd.h", "errno.h", "string.h", "unistd.h", )
	result_code = [
		"struct passwd *p;",
		"uid_t runner_uid;",
		"gid_t runner_gid;",
		"if(NULL == (p = getpwnam(" + _c_string(serv.runasuser) + "))) {",
		make_error_verbose_code_block("ERR: cannot have run-as-user", 11),
		"} else {", (
			"runner_uid = p->pw_uid;",
			"runner_gid = p->pw_gid;",),
		"}",
		"if(0 != setgid(runner_gid)) {",
		make_error_verbose_code_block("ERR: cannot set GID", 12),
		"}",
		"if(0 != setegid(runner_gid)) {",
		make_error_verbose_code_block("ERR: cannot set EGID", 13),
		"}",
		"if(0 != setuid(runner_uid)) {",
		make_error_verbose_code_block("ERR: cannot set UID", 14),
		"}",
		"if(0 != seteuid(runner_uid)) {",
		make_error_verbose_code_block("ERR: cannot set EUID", 15),
		"}", ]
	return (required_include, result_code, )
# ### def make_prepare_run_as_user

def generate_service_prepare_function(serv):
	prepare_func_name = "_prepare_" + serv.get_sanitized_name().lower()
	required_include = set()
	result_code = []
	for aux in (make_prepare_resource_limit(serv), make_prepare_run_as_user(serv), ):
		if not aux:
			continue
		using_include, prepare_code, = aux
		required_include.update(using_include)
		result_code.append(("{", _indent_code(prepare_code), "}", ))
	if result_code:
		result_code = [
			"static int " + prepare_func_name + "() {",
			result_code,
			"\treturn 0;",
			"}", ]
	else:
		prepare_func_name = None
	return (prepare_func_name, required_include, result_code, )
# ### def generate_service_prepare_function

def generate_service_definition_structure(serv):
	serv_struct_name = "SERVICE_"+ serv.get_sanitized_name()
	serv_argv_name = serv_struct_name + "_ARGV"

	prepare_function_name, prepare_function_include, prepare_function_code, = generate_service_prepare_function(serv)

	argv_content = list(map(_c_string, serv.command))
	argv_content.append("NULL")
	result_code = [
		"char * const " + serv_argv_name + "[] = {" + ", ".join(argv_content) + "};",
		"ServiceDefinition " + serv_struct_name + " = {.process_id=0,",
		"\t\t.started_at=0,",
		"\t\t.service_name=" + _c_string(serv.name) + ",",
		"\t\t.work_directory=" + _c_string(serv.workdirectory) + ",",
		"\t\t.prepare_function=" + _c_get_function_pointer(prepare_function_name) + ",",
		"\t\t.executable_path=" + argv_content[0] + ",",
		"\t\t.execute_argv=" + serv_argv_name + "};", ]
	return (prepare_function_name, prepare_function_include, prepare_function_code,
			serv_struct_name, result_code,)
# ### def generate_service_definition_structure

def generate_main_function(service_structs):
	prepare_func_code = []
	main_func_code = ["int main(int argc, char ** argv) {", ]
	required_includes = set()
	serv_struct_ptrs = []
	for prepare_function_name, prepare_function_include, prepare_function_code, serv_struct_name, serv_def_code, in service_structs:
		if prepare_function_name:
			required_includes.update(prepare_function_include)
			prepare_func_code.append(None)
			prepare_func_code.extend(prepare_function_code)
		serv_struct_ptrs.append("&" + serv_struct_name)
		main_func_code.extend(_indent_code(serv_def_code, prepend="\t"))
	serv_struct_ptrs.append("NULL")
	main_func_code.append("\tServiceDefinition * const services[] = {")
	main_func_code.append("\t\t" + ", ".join(serv_struct_ptrs) + "};")
	main_func_code.append("\trun_services(services);")
	main_func_code.append("\treturn 0;")
	main_func_code.append("}")
	result_code = []
	if required_includes:
		result_code.extend(map(lambda l: "#include<" + l + ">", sorted(required_includes)))
		result_code.append(None)
	result_code.append("#include \"run_service.h\"")
	result_code.append(None)
	result_code.append("/* This is generated file */")
	result_code.append(None)
	result_code.extend(main_func_code)
	result_code.append(None)
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
			if l:
				fp.write(l)
			fp.write("\n")
	return 0
# ### def _main

if "__main__" == __name__:
	sys.exit(_main())
# >>> if "__main__" == __name__:

# vim: ts=4 sw=4 ai nowrap
