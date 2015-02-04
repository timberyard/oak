#include <uon/uon.hpp>

#include "formatter.hpp"
#include "process.hpp"

void notify(uon::Value report)
{
	// generate set of receivers
	std::set<std::string> receivers = { "niklas.hofmann@everbase.net" };

	if(receivers.size() == 0)
	{
		return;
	}

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
