#pragma once

#include <boost/property_tree/ptree.hpp>
#include <json_spirit/json_spirit.h>
#include "process.hpp"

namespace task_utils {

json_spirit::Object createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const process::TextProcessResult& processResult);
std::string createTaskMessage(const process::TextProcessResult& processResult);

} // namespace: task_utils
