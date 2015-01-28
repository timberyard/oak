#pragma once

#include <string>
#include <map>
#include <functional>

#include <json_spirit/json_spirit.h>

#include "config.hpp"

namespace tasks {

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
	std::string emailOutput;

	TaskResult()
	: status(STATUS_ERROR)
	, warnings(0)
	, errors(0)
	{ }
};

std::string toString(TaskResult::Status status);

extern std::map<std::string, std::function<TaskResult(config::ConfigNode)>> taskTypes;

} // namespace: tasks
