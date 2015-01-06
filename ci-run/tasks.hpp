#pragma once

#include <string>
#include <map>
#include <functional>
#include <boost/property_tree/ptree.hpp>

struct TaskResult
{
	enum Status
	{
		STATUS_OK = 0,
		STATUS_WARNING = 1,
		STATUS_ERROR = 2
	}
	status;

	unsigned int warnings;
	unsigned int errors;

	std::string message;
	boost::property_tree::ptree output;

	TaskResult() : status(STATUS_ERROR), warnings(0), errors(0) { }

	TaskResult(const TaskResult &o)
	{
		status = o.status;
		warnings = o.warnings;
		errors = o.errors;
		message = o.message;
		output = o.output;
	}
};

extern std::map<std::string, std::function<TaskResult(const boost::property_tree::ptree&)>> taskTypes;
