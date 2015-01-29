#include "formatter.hpp"

#include <vector>
#include <boost/algorithm/string/replace.hpp>

namespace formatter { namespace _markdown {

std::string utf8_substr(std::string originalString, int maxLength)
{
    std::string resultString = originalString;

    int len = 0;
    int byteCount = 0;

    const char* aStr = originalString.c_str();

    while(*aStr)
    {
        if( (*aStr & 0xc0) != 0x80 )
            len += 1;

        if(len>maxLength)
        {
            resultString = resultString.substr(0, byteCount);
            break;
        }
        byteCount++;
        aStr++;
    }

    return resultString;
}

std::size_t utf8_length(std::string str)
{
	auto s = str.c_str();
	std::size_t len = 0;
	while (*s) len += (*s++ & 0xc0) != 0x80;
	return len;
}

namespace table {

	void row(std::vector<std::size_t> widths, std::vector<std::string> content, std::ostream& output)
	{
		output << "|";

		auto i = widths.begin();
		auto j = content.begin();

		for(; i != widths.end() && j != content.end(); ++i, ++j)
		{
			auto minus = i == widths.begin() ? 4 : 3;

			output << " ";

			auto col = *j;

			boost::replace_all(col, "\r\n", "\n");
			boost::replace_all(col, "\r", "\n");
			boost::replace_all(col, "\n", " ");

			col = utf8_substr(col, *i - minus);
			output << col;

			if(utf8_length(col) < (*i - minus))
			{
				output << std::string(*i - minus - utf8_length(col), ' ');
			}

			output << " |";
		}

		output << std::endl;
	}

	void line(std::vector<std::size_t> widths, std::ostream& output)
	{
		output << "|";

		for(auto i = widths.begin(); i != widths.end(); ++i)
		{
			auto minus = i == widths.begin() ? 2 : 1;

			output << std::string(*i - minus, '-');
			output << "|";
		}

		output << std::endl;
	}

} // namespace: table

} // namespace: _markdown

void markdown(boost::property_tree::ptree input, std::ostream& output)
{
	using namespace _markdown;

	output << "# Meta" << std::endl << std::endl;

	output << "* Project: " << input.get<std::string>("meta.project.name") << std::endl;
	output << "* Repository: " << input.get<std::string>("meta.repository") << std::endl;
	output << "* Branch: " << input.get<std::string>("meta.branch") << std::endl;
	output << "* Commit: " << input.get<std::string>("meta.commit.id.long") << std::endl;
	output << "* Timestamp: " << input.get<std::string>("meta.commit.timestamp.default") << std::endl;
	output << "* Architecture" << std::endl;
	output << "    * Build: " << input.get<std::string>("meta.arch.build.descriptor") << std::endl;
	output << "    * Host: " << input.get<std::string>("meta.arch.host.descriptor") << std::endl;
	output << std::endl;
	output << std::endl;

	for(auto section : input.get_child("tasks"))
	{
		output << "# Tasks: " << section.first << std::endl << std::endl;

		if(section.second.size() > 0)
		{
			std::vector<std::size_t> tabset { 20, 30, 55, 10, 10, 15 };

			table::row(tabset, std::vector<std::string>{ "Name", "Type", "Message", "Warnings", "Errors", "Status" }, output);
			table::line(tabset, output);

			for(auto task : section.second)
			{
				table::row(tabset, std::vector<std::string>{
					task.second.get<std::string>("name"),
					task.second.get<std::string>("type"),
					task.second.get<std::string>("message"),
					task.second.get<std::string>("warnings"),
					task.second.get<std::string>("errors"),
					task.second.get<std::string>("status") },
					output);
			}

			output << std::endl;
		}

		output << std::endl;

		for(auto task : section.second)
		{
			output << "## Task: " << task.first << std::endl << std::endl;

			auto taskType = task.second.get<std::string>("type");

			output << "* Type: " << taskType << std::endl;
			output << "* Message: " << task.second.get<std::string>("message") << std::endl;
			output << "* Warnings: " << task.second.get<std::string>("warnings") << std::endl;
			output << "* Errors: " << task.second.get<std::string>("errors") << std::endl;
			output << "* Status: " << task.second.get<std::string>("status") << std::endl;
			output << std::endl;

			if(taskType == "analysis:cppcheck")
			{
				auto errors = task.second.get_child("details.errors");

				if(errors.size() > 0)
				{
					std::vector<std::size_t> tabset { 20, 20, 45, 45, 10 };

					table::row(tabset, std::vector<std::string>{ "Type", "Severity", "Message", "File", "Line" }, output);
					table::line(tabset, output);

					for(auto error : errors)
					{
						table::row(tabset, std::vector<std::string>{
							error.second.get<std::string>("type"),
							error.second.get<std::string>("severity"),
							error.second.get<std::string>("message"),
							error.second.get<std::string>("file"),
							error.second.get<std::string>("line") },
							output);
					}

					output << std::endl;
				}
			}
			else
			if(taskType == "build:cmake")
			{
				auto results = task.second.get_child("details.results");

				if(results.size() > 0)
				{
					std::vector<std::size_t> tabset { 15, 40, 65, 10, 10 };

					table::row(tabset, std::vector<std::string>{ "Type", "Message", "File", "Row", "Column" }, output);
					table::line(tabset, output);

					for(auto result : results)
					{
						table::row(tabset, std::vector<std::string>{
							result.second.get<std::string>("type"),
							result.second.get<std::string>("message"),
							result.second.get<std::string>("filename"),
							result.second.get<std::string>("row"),
							result.second.get<std::string>("column") },
							output);
					}

					output << std::endl;
				}
			}
			else
			if(taskType == "test:googletest")
			{
				{
					std::vector<std::size_t> tabset { 45, 15, 15, 15, 15, 15, 20 };

					table::row(tabset, std::vector<std::string>{ "Name", "Tests", "Failures", "Errors", "Disabled", "Time", "Result" }, output);
					table::line(tabset, output);

					table::row(tabset, std::vector<std::string>{
						task.second.get<std::string>("details.testsuites.all.name"),
						task.second.get<std::string>("details.testsuites.all.tests"),
						task.second.get<std::string>("details.testsuites.all.failures"),
						task.second.get<std::string>("details.testsuites.all.errors"),
						task.second.get<std::string>("details.testsuites.all.disabled"),
						task.second.get<std::string>("details.testsuites.all.time"),
						task.second.get<std::string>("details.testsuites.all.result") },
						output);

					for(auto testsuite : task.second.get_child("details.testsuites.details"))
					{
						table::row(tabset, std::vector<std::string>{
							testsuite.second.get<std::string>("name"),
							testsuite.second.get<std::string>("tests"),
							testsuite.second.get<std::string>("failures"),
							testsuite.second.get<std::string>("errors"),
							testsuite.second.get<std::string>("disabled"),
							testsuite.second.get<std::string>("time"),
							testsuite.second.get<std::string>("result") },
							output);
					}

					output << std::endl;
				}

				auto tests = task.second.get_child("details.tests");

				if(tests.size() > 0)
				{
					std::vector<std::size_t> tabset { 45, 15, 45, 15, 20 };

					table::row(tabset, std::vector<std::string>{ "Name", "Status", "Message", "Time", "Result" }, output);
					table::line(tabset, output);

					for(auto test : tests)
					{
						table::row(tabset, std::vector<std::string>{
							test.second.get<std::string>("name"),
							test.second.get<std::string>("status"),
							test.second.get<std::string>("message"),
							test.second.get<std::string>("time"),
							test.second.get<std::string>("result") },
							output);
					}

					output << std::endl;
				}
			}

			output << std::endl;
		}
	}
}

} // namespace: formatter
