
#include <boost/algorithm/string/join.hpp>
#include <json_spirit/json_spirit.h>
#include "task.utils.hpp"

//using namespace boost::property_tree;

#define OUTPUT_LINES_ARE_ARRAYS 1

namespace js = json_spirit;

json_spirit::Object createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const TextProcessResult& processResult)
{
	json_spirit::Object result;
	result.push_back( js::Pair("binary", binary) );
	result.push_back( js::Pair("arguments", js::Value(arguments.begin(), arguments.end()) ));
	result.push_back( js::Pair("working_dir", workingDirectory) );

	js::Array output_lines;
	output_lines.reserve( processResult.output.size());
	for(auto& i : processResult.output)
	{
#if OUTPUT_LINES_ARE_ARRAYS
		js::Array line;
		line.push_back( toString(i.first));  // LineStatus
		line.push_back( i.second);           // line's content
#else
		js::Object line;
		line.emplace_back( "status", toString(i.first));
		line.emplace_back( "c", i.second );  // line's content
#endif
		output_lines.push_back( line );
	}

	result.push_back( js::Pair("output", output_lines ) );
	result.push_back( js::Pair("exitcode", processResult.exitCode));

	return result;
}


std::string createTaskMessage(const TextProcessResult& processResult)
{
	return processResult.output.size() > 0 ? processResult.output.back().second : "-";
}

