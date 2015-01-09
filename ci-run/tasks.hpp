#pragma once

#include <string>
#include <map>
#include <functional>
#include <boost/property_tree/ptree.hpp>

#include <json_spirit/json_spirit.h>

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
	json_spirit::Object output;

	TaskResult() : status(STATUS_ERROR), warnings(0), errors(0) { }

	TaskResult(const TaskResult &o) = default;
	TaskResult(TaskResult&& o) = default;
};

std::string toString(TaskResult::Status status);

extern std::map<std::string, std::function<TaskResult(const boost::property_tree::ptree&)>> taskTypes;
