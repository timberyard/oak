#pragma once

#include <string>
#include <map>
#include <functional>
#include <uon/uon.hpp>

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
	uon::Value output;

	TaskResult()
	: status(STATUS_ERROR)
	, warnings(0)
	, errors(0)
	{ }
};

std::string toString(TaskResult::Status status);

extern std::map<std::string, std::function<TaskResult(uon::Value)>> taskTypes;

} // namespace: tasks
