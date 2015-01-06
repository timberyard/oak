#pragma once

#include <boost/property_tree/ptree.hpp>

#include "process.hpp"

boost::property_tree::ptree createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const TextProcessResult& processResult);

std::string createTaskMessage(const TextProcessResult& processResult);
