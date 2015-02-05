#include <uon/uon.hpp>
#include <boost/algorithm/string/join.hpp>

#include "formatter.hpp"
#include "process.hpp"

void notify(uon::Value report)
{
	// generate set of receivers
	std::set<std::string> receivers = { };

	receivers.insert(report.get("meta.commit.author.email").to_string());
	receivers.insert(report.get("meta.commit.committer.email").to_string());

	for(auto buildgap : report.get("meta.buildgap").to_array())
	{
		receivers.insert(buildgap.get("author.email").to_string());
		receivers.insert(buildgap.get("committer.email").to_string());
	}

	if(receivers.size() == 0)
	{
		std::cout << "no receivers found..." << std::endl;
		return;
	}

	std::cout << "receivers: " << boost::join(receivers, ", ") << std::endl;

	// generate message
	std::stringstream message;

	std::cout << "formatting html..." << std::endl;
	formatter::html(report, message);

	std::string subject = "Project: " + report.get("meta.project.name", uon::null).to_string() + " | Branch: " + report.get("meta.branch", uon::null).to_string() + " | Commit: " + report.get("meta.commit.id.short", uon::null).to_string() + " | Timestamp: " + report.get("meta.commit.timestamp.default", uon::null).to_string();

	// send mail
	std::vector<std::string> mailArgs{
		"-a", "Content-Type: text/html; charset=UTF-8",
		"-s", subject
	};

	mailArgs.insert(mailArgs.end(), receivers.begin(), receivers.end());

	process::TextProcessResult mailResult = process::executeTextProcess("mail", mailArgs, ".", message.str());

	if(mailResult.exitCode != 0)
	{
		throw std::runtime_error("could not send notification");
	}
}
