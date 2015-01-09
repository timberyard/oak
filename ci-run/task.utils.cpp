
#include <boost/algorithm/string/join.hpp>

#include "task.utils.hpp"

using namespace std;
using namespace boost::property_tree;

ptree createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const TextProcessResult& processResult)
{
/*
	ptree header_binary;
	header_binary.add("th", "Binary:");
	header_binary.add("binary", binary);

	ptree header_arguments;
	header_arguments.add("th", "Arguments:");
	header_arguments.add("td", boost::algorithm::join(arguments, " "));

	ptree header_wd;
	header_wd.add("th", "Working Directory:");
	header_wd.add("td", workingDirectory);
*/
	ptree header;
	header.add("binary", binary);
	header.add("arguments", boost::algorithm::join(arguments, " "));
	header.add("working_dir", workingDirectory);

	ptree output;

	for(auto& i : processResult.output)
	{
		ptree output_line;

		output_line.put("", i.second);
		output.add_child( std::string("ol") + (i.first == TextProcessResult::INFO_LINE ? "-info" : (i.first == TextProcessResult::OK_LINE ? "-ok" : "-error")), output_line);
	}

	{
		ptree output_line;
		
		output_line.put("", processResult.exitCode);
		output.add_child("exitcode", output_line);
	}

	ptree result;
	result.add_child("process-header", header);
	result.add_child("div-output", output);

	return result;
}

string createTaskMessage(const TextProcessResult& processResult)
{
	return processResult.output.size() > 0 ? processResult.output.back().second : string("-");
}

