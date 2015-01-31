#pragma once

#include <uon/uon.hpp>
#include "process.hpp"

namespace task_utils {

uon::Value createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const process::TextProcessResult& processResult);
std::string createTaskMessage(const process::TextProcessResult& processResult);

} // namespace: task_utils
