
TaskResult task_publish_birch( uon::Value config )
{
	TaskResult result;

	for( auto source : config.get("sources").as_object() )
	{
		std::string srcdir = source.second.to_string();
		std::string remote = config.get("destination.user").to_string() + std::string("@") + config.get("destination.host").to_string();
		std::string destdir = config.get("destination.base").to_string() + std::string("/") + config.get("destination.directory").to_string() + std::string("/") + source.first;

		// run ssh:mkdir
		std::vector<std::string> sshArgs { "-o", "BatchMode=yes", "-p", config.get("destination.port").to_string(), remote, config.get("destination.mkdir.binary").to_string(), "-p", destdir };

		process::TextProcessResult sshResult = process::executeTextProcess(config.get("ssh.binary").to_string(), sshArgs, srcdir);

		result.output.set("ssh:mkdir", task_utils::createTaskOutput(config.get("ssh.binary").to_string(), sshArgs, srcdir, sshResult));
		result.message = task_utils::createTaskMessage(sshResult);

		if(sshResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}

		// run rsync
		std::vector<std::string> rsyncArgs {
			std::string("--rsh=") + config.get("ssh.binary").to_string() + (" -o BatchMode=yes -p ") + config.get("destination.port").to_string(), "--archive", "--delete", "--verbose", std::string("./"),
			remote + std::string(":") + destdir + std::string("/")
		};

		process::TextProcessResult rsyncResult = process::executeTextProcess(config.get("rsync.binary").to_string(), rsyncArgs, srcdir);

		result.output.set("rsync", task_utils::createTaskOutput(config.get("rsync.binary").to_string(), rsyncArgs, srcdir, rsyncResult));
		result.message = task_utils::createTaskMessage(rsyncResult);

		if(rsyncResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}
	}

	result.warnings = 0;
	result.errors = 0;
	result.status = TaskResult::STATUS_OK;

	return result;
}

TaskResult task_publish_email( uon::Value config )
{
	TaskResult result;
	result.warnings = 0;
	result.errors = 0;

	// detect authors
	process::TextProcessResult gitAuthors = process::executeTextProcess(
		config.get("git.binary").to_string(), {"log", "--pretty=%ae", std::string("--since=")+config.get("receivers.authors").to_string()},
		config.get("receivers.repository").to_string());

	result.message = task_utils::createTaskMessage(gitAuthors);

	if(gitAuthors.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	// detect committers
	process::TextProcessResult gitCommitters = process::executeTextProcess(
		config.get("git.binary").to_string(), {"log", "--pretty=%ae", std::string("--since=")+config.get("receivers.committers").to_string()},
		config.get("receivers.repository").to_string());

	result.message = task_utils::createTaskMessage(gitCommitters);

	if(gitCommitters.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	// generate set of receivers
	std::set<std::string> receivers;

	for(auto receiver : gitAuthors.output)
	{
		if(    receiver.second.length() > 0
			&& receiver.first == process::TextProcessResult::LineType::INFO_LINE
			&& receivers.find(receiver.second) == receivers.end())
		{
			receivers.insert(receiver.second);
		}
	}

	for(auto receiver : gitCommitters.output)
	{
		if(    receiver.second.length() > 0
			&& receiver.first == process::TextProcessResult::LineType::INFO_LINE
			&& receivers.find(receiver.second) == receivers.end())
		{
			receivers.insert(receiver.second);
		}
	}

	if(receivers.size() == 0)
	{
		result.status = TaskResult::STATUS_OK;
		return result;
	}

	// generate message
	std::stringstream message;

	for(auto report : config.get("reports").as_object())
	{
		std::cout << "reading " << report.second.to_string() << "..." << std::endl;

		// load report
		std::ifstream reportstream;
		reportstream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		reportstream.open( report.second.to_string() );

		uon::Value reporttree = uon::read_json( reportstream );

		// format report
		std::cout << "formatting markdown..." << std::endl;
		formatter::markdown(reporttree, message);
	}

	// send mail
	std::vector<std::string> mailArgs{
		"-a", "Content-Type: text/plain; charset=UTF-8",
		"-s", config.get("subject").to_string()
	};
	mailArgs.insert(mailArgs.end(), receivers.begin(), receivers.end());

	process::TextProcessResult mailResult = process::executeTextProcess(
		config.get("mail.binary").to_string(), mailArgs,
		config.get("receivers.repository").to_string(),
		message.str());

	result.message = task_utils::createTaskMessage(mailResult);

	if(mailResult.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	result.status = TaskResult::STATUS_OK;
	return result;
}
