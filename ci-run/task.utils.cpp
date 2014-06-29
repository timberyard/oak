
#include <boost/algorithm/string/join.hpp>

#include "task.utils.hpp"

using namespace std;
using namespace boost::property_tree;

ptree createTaskOutput(const std::string& binary, const std::vector<std::string>& arguments, const std::string& workingDirectory, const TextProcessResult& processResult)
{
	ptree header_binary;
	header_binary.add("th", "Binary:");
	header_binary.add("td", binary);

	ptree header_arguments;
	header_arguments.add("th", "Arguments:");
	header_arguments.add("td", boost::algorithm::join(arguments, " "));

	ptree header_wd;
	header_wd.add("th", "Working Directory:");
	header_wd.add("td", workingDirectory);

	ptree header;
	header.put("<xmlattr>.class", string("process-header"));
	header.add_child("tr", header_binary);
	header.add_child("tr", header_arguments);
	header.add_child("tr", header_wd);

	ptree output;
	output.put("<xmlattr>.class", string("process-output"));

	for(auto& i : processResult.output)
	{
		ptree output_line;

		output_line.put("", i.second);
		output_line.put("<xmlattr>.class", string("line ") + (i.first == TextProcessResult::INFO_LINE ? "line-info" : (i.first == TextProcessResult::OK_LINE ? "line-ok" : "line-error")));

		output.add_child("div", output_line);
	}

	{
		ptree output_line;
		
		output_line.put("", processResult.exitCode);
		output_line.put("<xmlattr>.class", string("line line-exitcode ") + (processResult.exitCode != 0 ? "line-error" : ""));

		output.add_child("div", output_line);
	}

	ptree result;
	result.add_child("table", header);
	result.add_child("div", output);

	return result;
}

string createTaskMessage(const TextProcessResult& processResult)
{
	return processResult.output.size() > 0 ? processResult.output.back().second : string("-");
}

