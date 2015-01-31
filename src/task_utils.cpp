
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "task_utils.hpp"

namespace task_utils {

uon::Value createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const process::TextProcessResult& processResult)
{
	uon::Value result;
	result.set("binary", binary);
	result.set("arguments", uon::Array(arguments.begin(), arguments.end()) );
	result.set("working_dir", workingDirectory);

	uon::Array output_lines;
	for(auto& i : processResult.output)
	{
		uon::Array line;
		line.push_back( toString(i.first));  // LineStatus
		line.push_back( i.second);           // line's content
		output_lines.push_back( line );
	}

	result.set("output", output_lines );
	result.set("exitcode", static_cast<uon::Number>(processResult.exitCode));

	return result;
}

std::string createTaskMessage(const process::TextProcessResult& processResult)
{
	return boost::trim_copy(processResult.output.size() > 0 ? processResult.output.back().second : "-");
}

} // namespace: task_utils
