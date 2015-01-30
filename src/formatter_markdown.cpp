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

void markdown(uon::Value input, std::ostream& output)
{
	using namespace _markdown;

	output << "# Meta" << std::endl << std::endl;

	auto meta = input.get("meta", uon::null);

	output << "* Project: " << meta.get("project.name", uon::null).to_string() << std::endl;
	output << "* Repository: " << meta.get("repository", uon::null).to_string() << std::endl;
	output << "* Branch: " << meta.get("branch", uon::null).to_string() << std::endl;
	output << "* Commit: " << meta.get("commit.id.long", uon::null).to_string() << std::endl;
	output << "* Timestamp: " << meta.get("commit.timestamp.default", uon::null).to_string() << std::endl;
	output << "* Architecture" << std::endl;
	output << "    * Build: " << meta.get("arch.build.descriptor", uon::null).to_string() << std::endl;
	output << "    * Host: " << meta.get("arch.host.descriptor", uon::null).to_string() << std::endl;
	output << std::endl;
	output << std::endl;

	for(auto section : input.get("tasks", uon::null).to_object())
	{
		output << "# Tasks: " << section.first << std::endl << std::endl;

		auto tasks = section.second.to_object();

		if(tasks.size() > 0)
		{
			std::vector<std::size_t> tabset { 20, 30, 55, 10, 10, 15 };

			table::row(tabset, std::vector<std::string>{ "Name", "Type", "Message", "Warnings", "Errors", "Status" }, output);
			table::line(tabset, output);

			for(auto task : tasks)
			{
				table::row(tabset, std::vector<std::string>{
					task.second.get("name", uon::null).to_string(),
					task.second.get("type", uon::null).to_string(),
					task.second.get("message", uon::null).to_string(),
					task.second.get("warnings", uon::null).to_string(),
					task.second.get("errors", uon::null).to_string(),
					task.second.get("status", uon::null).to_string() },
					output);
			}

			output << std::endl;
		}

		output << std::endl;

		for(auto task : tasks)
		{
			output << "## Task: " << task.first << std::endl << std::endl;

			auto taskType = task.second.get("type", uon::null).to_string();

			output << "* Type: " << taskType << std::endl;
			output << "* Message: " << task.second.get("message", uon::null).to_string() << std::endl;
			output << "* Warnings: " << task.second.get("warnings", uon::null).to_string() << std::endl;
			output << "* Errors: " << task.second.get("errors", uon::null).to_string() << std::endl;
			output << "* Status: " << task.second.get("status", uon::null).to_string() << std::endl;
			output << std::endl;

			if(taskType == "analysis:cppcheck")
			{
				auto errors = task.second.get("details.errors", uon::null).to_array();

				if(errors.size() > 0)
				{
					std::vector<std::size_t> tabset { 20, 20, 45, 45, 10 };

					table::row(tabset, std::vector<std::string>{ "Type", "Severity", "Message", "File", "Line" }, output);
					table::line(tabset, output);

					for(auto error : errors)
					{
						table::row(tabset, std::vector<std::string>{
							error.get("type", uon::null).to_string(),
							error.get("severity", uon::null).to_string(),
							error.get("message", uon::null).to_string(),
							error.get("file", uon::null).to_string(),
							error.get("line", uon::null).to_string() },
							output);
					}

					output << std::endl;
				}
			}
			else
			if(taskType == "build:cmake")
			{
				auto results = task.second.get("details.results", uon::null).to_array();

				if(results.size() > 0)
				{
					std::vector<std::size_t> tabset { 15, 40, 65, 10, 10 };

					table::row(tabset, std::vector<std::string>{ "Type", "Message", "File", "Row", "Column" }, output);
					table::line(tabset, output);

					for(auto result : results)
					{
						table::row(tabset, std::vector<std::string>{
							result.get("type", uon::null).to_string(),
							result.get("message", uon::null).to_string(),
							result.get("filename", uon::null).to_string(),
							result.get("row", uon::null).to_string(),
							result.get("column", uon::null).to_string() },
							output);
					}

					output << std::endl;
				}
			}
			else
			if(taskType == "test:googletest")
			{
				auto all = task.second.get("details.testsuites.all", uon::null);
				auto testsuites = task.second.get("details.testsuites.details", uon::null).to_array();

				if(!all.is_null() || testsuites.size() > 0)
				{
					std::vector<std::size_t> tabset { 45, 15, 15, 15, 15, 15, 20 };

					table::row(tabset, std::vector<std::string>{ "Name", "Tests", "Failures", "Errors", "Disabled", "Time", "Result" }, output);
					table::line(tabset, output);

					if(!all.is_null())
					{
						table::row(tabset, std::vector<std::string>{
							all.get("name", uon::null).to_string(),
							all.get("tests", uon::null).to_string(),
							all.get("failures", uon::null).to_string(),
							all.get("errors", uon::null).to_string(),
							all.get("disabled", uon::null).to_string(),
							all.get("time", uon::null).to_string(),
							all.get("result", uon::null).to_string() },
							output);
					}


					if(testsuites.size() > 0)
					{
						for(auto testsuite : testsuites)
						{
							table::row(tabset, std::vector<std::string>{
								testsuite.get("name", uon::null).to_string(),
								testsuite.get("tests", uon::null).to_string(),
								testsuite.get("failures", uon::null).to_string(),
								testsuite.get("errors", uon::null).to_string(),
								testsuite.get("disabled", uon::null).to_string(),
								testsuite.get("time", uon::null).to_string(),
								testsuite.get("result", uon::null).to_string() },
								output);
						}
					}

					output << std::endl;
				}

				auto tests = task.second.get("details.tests", uon::null).to_array();

				if(tests.size() > 0)
				{
					std::vector<std::size_t> tabset { 45, 15, 45, 15, 20 };

					table::row(tabset, std::vector<std::string>{ "Name", "Status", "Message", "Time", "Result" }, output);
					table::line(tabset, output);

					for(auto test : tests)
					{
						table::row(tabset, std::vector<std::string>{
							test.get("name", uon::null).to_string(),
							test.get("status", uon::null).to_string(),
							test.get("message", uon::null).to_string(),
							test.get("time", uon::null).to_string(),
							test.get("result", uon::null).to_string() },
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
