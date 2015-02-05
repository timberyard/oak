#include "formatter.hpp"

#include <vector>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>
#include <boost/optional.hpp>

namespace formatter { namespace _html {

std::string escape(std::string data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&#39;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

std::string github_url(std::string repo_url, std::string commit, std::string type, std::string path, boost::optional<std::size_t> line)
{
	boost::regex regex("git@([\\w\\.]+):([\\w\\-\\/]+)\\.git");
	boost::smatch what;

	if(!boost::regex_match(repo_url, what, regex) || what.size() < 3)
	{
		throw std::invalid_argument("not a repo url");
	}

	return "https://" + what[1] + "/" + what[2] + "/" + type + "/" + commit + "/" + path + (line ? "#L" + std::to_string(*line) : "");
}

std::string github_link(std::string repo_url, std::string commit, std::string type, std::string path, boost::optional<std::size_t> line, std::string label)
{
	try
	{
		return "<a href=\"" + github_url(repo_url, commit, type, path, line) + "\" target=\"_blank\">" + escape(label) + "</a>";
	}
	catch(const std::invalid_argument& e)
	{
		return escape(label);
	}
}

} // namespace: _html

void html(uon::Value input, std::ostream& output)
{
	using namespace _html;

	auto meta = input.get("meta", uon::null);

	output << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">" << std::endl;
	output << "<html>" << std::endl
		<< "<head>" << std::endl
			<< "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">" << std::endl
			<< "<title>" << _html::escape("Project: " + meta.get("project.name", uon::null).to_string() + " | Branch: " + meta.get("branch", uon::null).to_string() + " | Commit: " + meta.get("commit.id.short", uon::null).to_string() + " | Timestamp: " + meta.get("commit.timestamp.default", uon::null).to_string()) << "</title>" << std::endl
			<< "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.2/css/bootstrap.min.css\">" << std::endl
			<< "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.2/css/bootstrap-theme.min.css\">" << std::endl
			<< "<script src=\"https://code.jquery.com/jquery-1.11.2.min.js\" type=\"text/javascript\"></script>" << std::endl
			<< "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.2/js/bootstrap.min.js\" type=\"text/javascript\"></script>" << std::endl
			<< "<style type=\"text/css\">" << std::endl
			<< "body { font-family: \"Courier New\", Courier, monospace; font-size: 12px; }" << std::endl
			<< "h1 { font-size: 32px; margin-top: 32px; }" << std::endl
			<< "h2 { font-size: 24px; margin-top: 24px; }" << std::endl
			<< ".task-messages td:first-child { padding-right: 10px; }" << std::endl
			<< ".task-test-googletest-messages td:first-child { padding-right: 10px; }" << std::endl
			<< "</style>" << std::endl
		<< "</head>" << std::endl
		<< "<body>" << std::endl
		<< "<div class=\"container\">" << std::endl;

	output << "<h1>Meta</h1>" << std::endl << std::endl;

	output << "<table class=\"meta table table-condensed table-hover table-bordered\">" << std::endl;
	output << "<tr><th>Project:</th><td>" << _html::escape(meta.get("project.name", uon::null).to_string()) << "</td></tr>" << std::endl;
	output << "<tr><th>Repository:</th><td>" << _html::escape(meta.get("repository", uon::null).to_string()) << "</td></tr>" << std::endl;
	output << "<tr><th>Branch:</th><td>" << _html::github_link(meta.get("repository", uon::null).to_string(), meta.get("branch", uon::null).to_string(), "tree", "", boost::optional<std::size_t>(), meta.get("branch", uon::null).to_string()) << "</td></tr>" << std::endl;
	output << "<tr><th>Commit:</th><td>" << _html::github_link(meta.get("repository", uon::null).to_string(), meta.get("commit.id.long", uon::null).to_string(), "tree", "", boost::optional<std::size_t>(), meta.get("commit.id.long", uon::null).to_string()) << "</td></tr>" << std::endl;
	output << "<tr><th>Timestamp:</th><td>" << _html::escape(meta.get("commit.timestamp.default", uon::null).to_string()) << "</td></tr>" << std::endl;
	output << "<tr><th>Architectures:</th><td><ul>" << std::endl;
	for(auto arch : meta.get("archs").to_array())
	{
		output << "<li>" << _html::escape(arch.get("host.descriptor", uon::null).to_string()) << "</li>" << std::endl;
	}
	output << "</ul></td></tr>" << std::endl;
	output << "</table>" << std::endl;
	output << std::endl;
	output << std::endl;

	output << "<h1>Tasks</h1>" << std::endl << std::endl;

	auto tasks = input.get("tasks", uon::null).to_object();

	if(tasks.size() > 0)
	{
		output << "<table class=\"tasks table table-condensed table-hover table-bordered\">" << std::endl;
		output << "<tr><th>Name</th><th>Type</th><th>Warnings</th><th>Errors</th><th>Status</th></tr>" << std::endl;

		for(auto task : tasks)
		{
			auto status = task.second.get("status.consolidated", uon::null).to_string();

			if(status == "ok")
			{
				output << "<tr class=\"success\">";
			}
			else
			if(status == "warning")
			{
				output << "<tr class=\"warning\">";
			}
			else
			{
				output << "<tr class=\"danger\">";
			}

			output << "<td><a href=\"#" << task.first << "\">"
				<< _html::escape(task.second.get("name", uon::null).to_string()) << "</a></td><td>"
				<< _html::escape(task.second.get("type", uon::null).to_string()) << "</td><td>"
				<< _html::escape(task.second.get("warnings", uon::null).to_string()) << "</td><td>"
				<< _html::escape(task.second.get("errors", uon::null).to_string()) << "</td><td>"
				<< _html::escape(status) << "</td></tr>" << std::endl;
		}

		output << "</table>" << std::endl;
		output << std::endl;
	}

	output << std::endl;

	for(auto task : tasks)
	{
		output << "<h2><a name=\"" << task.first << "\">Task: " << _html::escape(task.first) << "</a></h2>" << std::endl << std::endl;

		auto taskType = task.second.get("type", uon::null).to_string();

		output << "<table class=\"task-meta table table-condensed table-hover table-bordered\">" << std::endl;
		output << "<tr><th>Type:</th><td>" << _html::escape(taskType) << "</td></tr>" << std::endl;
		output << "<tr><th>Messages:</th><td>";

		auto messages = task.second.get("message", uon::null).to_object();

		if(messages.size() > 0)
		{
			output << "<table class=\"task-messages\">";

			for(auto message : messages)
			{
				output << "<tr><td>" << _html::escape(message.first) << ":</td><td>" << _html::escape(message.second.to_string()) << "</td></tr>";
			}

			output << "</table>";
		}

		output << "</td></tr>" << std::endl;
		output << "<tr><th>Warnings:</th><td>" << _html::escape(task.second.get("warnings", uon::null).to_string()) << "</td></tr>" << std::endl;
		output << "<tr><th>Errors:</th><td>" << _html::escape(task.second.get("errors", uon::null).to_string()) << "</td></tr>" << std::endl;

		auto status = task.second.get("status.consolidated", uon::null).to_string();

		if(status == "ok")
		{
			output << "<tr class=\"success\">";
		}
		else
		if(status == "warning")
		{
			output << "<tr class=\"warning\">";
		}
		else
		{
			output << "<tr class=\"danger\">";
		}

		output << "<th>Status:</th><td>" << _html::escape(status) << "</td></tr>" << std::endl;
		output << "</table>" << std::endl;
		output << std::endl;

		if(taskType == "analysis:cppcheck")
		{
			auto errors = task.second.get("details", uon::null).to_array();

			if(errors.size() > 0)
			{
				output << "<table class=\"task-analysis-cppcheck table table-condensed table-hover table-bordered\">" << std::endl;
				output << "<tr><th>Type</th><th>Severity</th><th>Message</th><th>File</th><th>Line</th></tr>" << std::endl;

				for(auto error : errors)
				{
					output << "<tr class=\"warning\"><td>"
						<< _html::escape(error.get("type", uon::null).to_string()) << "</td><td>"
						<< _html::escape(error.get("severity", uon::null).to_string()) << "</td><td>"
						<< _html::escape(error.get("message", uon::null).to_string()) << "</td><td>"
						<< _html::github_link(meta.get("repository", uon::null).to_string(), meta.get("commit.id.long", uon::null).to_string(), "blob", error.get("file", uon::null).to_string(), (std::size_t)error.get("line", uon::null).to_number(), error.get("file", uon::null).to_string()) << "</td><td>"
						<< _html::escape(error.get("line", uon::null).to_string()) << "</td></tr>";

					output << "<tr class=\"warning\"><td></td><td colspan=\"4\" style=\"font-size: 0.7em;\">"
						<< _html::escape(boost::algorithm::join(error.get("hosts", uon::null).to_string_array(), ", ")) << "</td></tr>" << std::endl;
				}

				output << "</table>" << std::endl;
				output << std::endl;
			}
		}
		else
		if(taskType == "build:cmake")
		{
			auto results = task.second.get("details", uon::null).to_array();

			if(results.size() > 0)
			{
				output << "<table class=\"task-build-cmake table table-condensed table-hover table-bordered\">" << std::endl;
				output << "<tr><th>Type</th><th>Message</th><th>File</th><th>Row</th><th>Column</th></tr>" << std::endl;

				for(auto result : results)
				{
					if(result.get("type", uon::null).to_string() == "warning")
					{
						output << "<tr class=\"warning\">";
					}
					else
					{
						output << "<tr class=\"danger\">";
					}

					output << "<td>"
						<< _html::escape(result.get("type", uon::null).to_string()) << "</td><td>"
						<< _html::escape(result.get("message", uon::null).to_string()) << "</td><td>"
						<< _html::github_link(meta.get("repository", uon::null).to_string(), meta.get("commit.id.long", uon::null).to_string(), "blob", result.get("filename", uon::null).to_string(), (std::size_t)result.get("row", uon::null).to_number(), result.get("filename", uon::null).to_string()) << "</td><td>"
						<< _html::escape(result.get("row", uon::null).to_string()) << "</td><td>"
						<< _html::escape(result.get("column", uon::null).to_string()) << "</td></tr>";

					if(result.get("type", uon::null).to_string() == "warning")
					{
						output << "<tr class=\"warning\">";
					}
					else
					{
						output << "<tr class=\"danger\">";
					}

					output
						<< "<td></td><td colspan=\"4\" style=\"font-size: 0.7em;\">"
						<< _html::escape(boost::algorithm::join(result.get("hosts", uon::null).to_string_array(), ", ")) << "</td></tr>" << std::endl;
				}

				output << "</table>" << std::endl;
				output << std::endl;
			}
		}
		else
		if(taskType == "test:googletest")
		{
			auto tests = task.second.get("details", uon::null).to_object();

			if(tests.size() > 0)
			{
				output << "<table class=\"task-test-googletest table table-condensed table-hover table-bordered\">" << std::endl;
				output << "<tr><th>Name</th><th>Status</th><th>Result</th></tr>" << std::endl;

				for(auto testsuite : tests)
				{
					for(auto test : testsuite.second.to_object())
					{
						if(test.second.get("result", uon::null).to_string() == "Ok")
						{
							output << "<tr class=\"success\">";
						}
						else
						{
							output << "<tr class=\"danger\">";
						}

						output << "<td>"
							<< _html::escape(test.second.get("name", uon::null).to_string()) << "</td><td>"
							<< _html::escape(test.second.get("status", uon::null).to_string()) << "</td><td>"
							<< _html::escape(test.second.get("result", uon::null).to_string()) << "</td></tr>" << std::endl;

						auto messages = test.second.get("message", uon::null).to_object();

						if(messages.size() > 0)
						{
							if(test.second.get("result", uon::null).to_string() == "Ok")
							{
								output << "<tr class=\"success\">";
							}
							else
							{
								output << "<tr class=\"danger\">";
							}

							output << "<td colspan=\"3\"><table class=\"task-test-googletest-messages\">";

							for(auto message : messages)
							{
								output << "<tr><td style=\"font-size: 0.7em;\">" << _html::escape(message.first) << ":</td><td style=\"font-size: 0.7em;\">" << _html::escape(message.second.to_string()) << "</td></tr>";
							}

							output << "</table></td></tr>";
						}
					}
				}

				output << "</table>" << std::endl;
				output << std::endl;
			}
		}

		output << std::endl;
	}

	output
		<< "</div>" << std::endl
		<< "</body>" << std::endl
		<< "</html>" << std::endl;
}

} // namespace: formatter
