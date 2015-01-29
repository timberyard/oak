
#include <numeric>
#include <fstream>
#include <set>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "tasks.hpp"
#include "process.hpp"
#include "task_utils.hpp"
#include "formatter.hpp"

namespace tasks {

namespace pt = boost::property_tree;
namespace js = json_spirit;

TaskResult task_build_cmake             ( config::ConfigNode config );
TaskResult task_test_googletest         ( config::ConfigNode config );
TaskResult task_analysis_cppcheck       ( config::ConfigNode config );
TaskResult task_doc_doxygen             ( config::ConfigNode config );
TaskResult task_publish_rsync_ssh       ( config::ConfigNode config );
TaskResult task_publish_mongo_rsync_ssh ( config::ConfigNode config );
TaskResult task_publish_email           ( config::ConfigNode config );

std::map<std::string, std::function<TaskResult(config::ConfigNode)>> taskTypes =
{
	{ "build:cmake",             task_build_cmake },
	{ "test:googletest",         task_test_googletest },
	{ "analysis:cppcheck",       task_analysis_cppcheck },
	{ "doc:doxygen",             task_doc_doxygen },
	{ "publish:rsync+ssh",       task_publish_rsync_ssh },
	{ "publish:mongo+rsync+ssh", task_publish_mongo_rsync_ssh },
	{ "publish:email",           task_publish_email }
};

bool copyDir(
    boost::filesystem::path const & source,
    boost::filesystem::path const & destination
)
{
    namespace fs = boost::filesystem;
    try
    {
        // Check whether the function call is valid
        if(
            !fs::exists(source) ||
            !fs::is_directory(source)
        )
        {
            std::cerr << "Source directory " << source.string()
                << " does not exist or is not a directory." << '\n'
            ;
            return false;
        }
        if(fs::exists(destination))
        {
            std::cerr << "Destination directory " << destination.string()
                << " already exists." << '\n'
            ;
            return false;
        }
        // Create the destination directory
        if(!fs::create_directory(destination))
        {
            std::cerr << "Unable to create destination directory"
                << destination.string() << '\n'
            ;
            return false;
        }
    }
    catch(fs::filesystem_error const & e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
    // Iterate through the source directory
    for(
        fs::directory_iterator file(source);
        file != fs::directory_iterator(); ++file
    )
    {
        try
        {
            fs::path current(file->path());
            if(fs::is_directory(current))
            {
                // Found directory: Recursion
                if(
                    !copyDir(
                        current,
                        destination / current.filename()
                    )
                )
                {
                    return false;
                }
            }
            else
            {
                // Found file: Copy
                fs::copy_file(
                    current,
                    destination / current.filename()
                );
            }
        }
        catch(fs::filesystem_error const & e)
        {
            std:: cerr << e.what() << '\n';
        }
    }
    return true;
}

TaskResult task_build_cmake( config::ConfigNode config )
{
	// create directories
	boost::filesystem::create_directories(config.value("build.output"));
	boost::filesystem::create_directories(config.value("install.output"));

	// run cmake
	TaskResult result;

	std::vector<std::string> cmakeParams {
		std::string("-DCMAKE_C_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.host.c.binary"), "\\", "/"),
		std::string("-DCMAKE_CXX_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.host.c++.binary"), "\\", "/"),
		std::string("-DCMAKE_INSTALL_PREFIX:STRING=") + boost::algorithm::replace_all_copy(config.value("install.output"), "\\", "/"),

		std::string("-DARCH_HOST_COMPILER_C:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.host.c.binary"), "\\", "/"),
		std::string("-DARCH_HOST_COMPILER_CXX:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.host.c++.binary"), "\\", "/"),
		std::string("-DARCH_HOST_OS:STRING=") + config.value("arch.host.os"),
		std::string("-DARCH_HOST_DISTRIBUTION:STRING=") + config.value("arch.host.distribution"),
		std::string("-DARCH_HOST_FAMILY:STRING=") + config.value("arch.host.family"),
		std::string("-DARCH_HOST_BITNESS:STRING=") + config.value("arch.host.bitness"),
		std::string("-DARCH_HOST_MISC:STRING=") + config.value("arch.host.misc"),
		std::string("-DARCH_HOST_DESCRIPTOR:STRING=") + config.value("arch.host.descriptor"),

		std::string("-DARCH_BUILD_COMPILER_C:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.build.c.binary"), "\\", "/"),
		std::string("-DARCH_BUILD_COMPILER_CXX:STRING=") + boost::algorithm::replace_all_copy(config.value("arch.build.c++.binary"), "\\", "/"),
		std::string("-DARCH_BUILD_OS:STRING=") + config.value("arch.build.os"),
		std::string("-DARCH_BUILD_DISTRIBUTION:STRING=") + config.value("arch.build.distribution"),
		std::string("-DARCH_BUILD_FAMILY:STRING=") + config.value("arch.build.family"),
		std::string("-DARCH_BUILD_BITNESS:STRING=") + config.value("arch.build.bitness"),
		std::string("-DARCH_BUILD_MISC:STRING=") + config.value("arch.build.misc"),
		std::string("-DARCH_BUILD_DESCRIPTOR:STRING=") + config.value("arch.build.descriptor")
	};

	if(config.value("verbose") == "yes")
		{ cmakeParams.push_back(std::string("-DCMAKE_VERBOSE_MAKEFILE:BOOLEAN=ON")); }

	for(auto variable : config.node("cmake.variables").children())
	{
		cmakeParams.push_back( std::string("-D") + variable.first + std::string(":")
			+ variable.second.value("type") + std::string("=")
			+ variable.second.value("value") );
	}

	cmakeParams.push_back(boost::algorithm::replace_all_copy(config.value("source.input"), "\\", "/"));

#ifdef _WIN32
	cmakeParams.push_back("-G");
	cmakeParams.push_back(config.value("cmake.generator"));
#endif

	process::TextProcessResult cmakeResult = process::executeTextProcess(
		config.value("cmake.binary"),
		cmakeParams,
		config.value("build.output"));

	result.output.emplace_back("cmake", task_utils::createTaskOutput(
		config.value("cmake.binary"),
		cmakeParams,
		config.value("build.output"),
		cmakeResult));

	result.message = task_utils::createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	// run make
	if(cmakeResult.exitCode == 0)
	{
		std::vector<std::string> makeParams;

		for(auto variable : config.node("make.variables").children())
		{
			makeParams.push_back( variable.first + std::string("=") + variable.second.value() );
		}

		process::TextProcessResult makeResult = process::executeTextProcess(
			config.value("make.binary"),
			makeParams,
			config.value("build.output"));

		json_spirit::Array details;

		auto basePath = boost::algorithm::replace_all_copy(config.value("source.base"), "\\", "/");

		for(auto line : makeResult.output)
		{
			auto i_filename = line.second.find(":");
			auto i_row = line.second.find(":", i_filename+1);
			auto i_column = line.second.find(":", i_row+1);

			auto i_warning = line.second.find("warning:", i_column+1);
			auto i_error = line.second.find("error:", i_column+1);

			if(	   i_filename != std::string::npos
				&& i_row != std::string::npos
				&& i_column != std::string::npos
				&& ( i_warning != std::string::npos || i_error != std::string::npos ) )
			{
				auto filename = line.second.substr(0, i_filename);
				auto row = line.second.substr(i_filename+1, i_row-i_filename-1);
				auto column = line.second.substr(i_row+1, i_column-i_row-1);
				std::string type = (i_error != std::string::npos) ? "error" : "warning";
				auto message = line.second.substr((i_error != std::string::npos) ? i_error+6 : i_warning+8);

				boost::trim(filename);
				boost::trim(row);
				boost::trim(column);
				boost::trim(type);
				boost::trim(message);

				boost::algorithm::replace_all(filename, "\\", "/");

				if(filename.find(basePath) == 0)
				{
					filename = filename.substr(basePath.length());
				}

				boost::trim_left_if(filename, boost::is_any_of("/"));

				js::Object details_row;
				details_row.emplace_back("type", type);
				details_row.emplace_back("message", message);
				details_row.emplace_back("filename", filename);
				details_row.emplace_back("row", row);
				details_row.emplace_back("column", column);

				details.push_back(details_row);
			}
		}

		result.output.emplace(result.output.begin(), "results", details);

		result.output.emplace_back("make", task_utils::createTaskOutput(
			config.value("make.binary"),
			makeParams,
			config.value("build.output"),
			makeResult));

		result.message = task_utils::createTaskMessage(makeResult);

		result.warnings = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<process::TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": warning:") != std::string::npos ? 1 : 0);
			});

		result.errors = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<process::TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": error:") != std::string::npos ? 1 : 0);
			});

		result.status =
			((result.errors > 0 || makeResult.exitCode != 0)
				? TaskResult::STATUS_ERROR
				: (result.warnings > 0
					? TaskResult::STATUS_WARNING
					: TaskResult::STATUS_OK));

		// run install
		if(makeResult.exitCode == 0 && config.value("install.enabled") == "yes")
		{
			std::vector<std::string> installParams{ "install" };

			for(auto variable : config.node("make.variables").children())
			{
				installParams.push_back( variable.first + std::string("=") + variable.second.value() );
			}

			process::TextProcessResult installResult = process::executeTextProcess(
				config.value("make.binary"),
				installParams,
				config.value("install.base"));

			result.output.emplace_back("install", task_utils::createTaskOutput(
				config.value("make.binary"),
				installParams,
				config.value("install.base"),
				installResult));

			result.message = task_utils::createTaskMessage(installResult);
			result.errors += (installResult.exitCode != 0 ? 1 : 0);

			result.status =
				(result.errors
					? TaskResult::STATUS_ERROR
					: (result.warnings > 0
						? TaskResult::STATUS_WARNING
						: TaskResult::STATUS_OK));
		}
	}

	return result;
}


TaskResult task_test_googletest( config::ConfigNode config )
{
	TaskResult result;

	// prepare parameter and dirs
	boost::filesystem::path xmlFilePath = config.value("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	std::vector<std::string> arguments = {
		"--gtest_output=xml:" + xmlFilePath.string(),
		"--gtest_filter=" + config.value("filter")
	};

	// run test
	process::TextProcessResult testResult = process::executeTextProcess(config.value("binary"), arguments, parentPath.string());

	// read XML result file
	pt::ptree xmlTestResult;

	std::ifstream xmlTestResultStream;
	xmlTestResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	xmlTestResultStream.open( xmlFilePath.string() );

	pt::read_xml( xmlTestResultStream, xmlTestResult, pt::xml_parser::trim_whitespace );

	// generate outline table
	js::Object table_outline;

	{
		js::Object row;
		row.emplace_back("name", xmlTestResult.get<std::string>("testsuites.<xmlattr>.name"));
		row.emplace_back("tests", xmlTestResult.get<std::string>("testsuites.<xmlattr>.tests"));
		row.emplace_back("failures", xmlTestResult.get<std::string>("testsuites.<xmlattr>.failures"));
		row.emplace_back("errors", xmlTestResult.get<std::string>("testsuites.<xmlattr>.errors"));
		row.emplace_back("disabled", xmlTestResult.get<std::string>("testsuites.<xmlattr>.disabled"));
		row.emplace_back("time", xmlTestResult.get<std::string>("testsuites.<xmlattr>.time"));
		row.emplace_back("result", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.emplace_back("all", row);
	}

	js::Object testsuites;
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		js::Object row;
		row.emplace_back("name", testsuite.second.get<std::string>("<xmlattr>.name"));
		row.emplace_back("tests", testsuite.second.get<std::string>("<xmlattr>.tests"));
		row.emplace_back("failures", testsuite.second.get<std::string>("<xmlattr>.failures"));
		row.emplace_back("errors", testsuite.second.get<std::string>("<xmlattr>.errors"));
		row.emplace_back("disabled", testsuite.second.get<std::string>("<xmlattr>.disabled"));
		row.emplace_back("time", testsuite.second.get<std::string>("<xmlattr>.time"));
		row.emplace_back("result", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		// testsuites.push_back(row); // if we want the task list as array
		testsuites.emplace_back( testsuite.second.get<std::string>("<xmlattr>.name"), row ); // if we want the task list as object with the task name as key
	}
	table_outline.emplace_back("details", testsuites);
	result.output.emplace_back("testsuites", table_outline);

	// generate details table
	js::Object table_details;
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		for ( auto testcase : testsuite.second )
		{
			if(testcase.first == "<xmlattr>")
				continue;

			std::string name = testcase.second.get<std::string>("<xmlattr>.classname") + "." + testcase.second.get<std::string>("<xmlattr>.name");

			js::Object row;
			row.emplace_back("name", name);
			row.emplace_back("status", testcase.second.get<std::string>("<xmlattr>.status"));
			row.emplace_back("message", (testcase.second.count("failure") > 0 ? testcase.second.get<std::string>("failure.<xmlattr>.message") : ""));
			row.emplace_back("time", testcase.second.get<std::string>("<xmlattr>.time"));
			row.emplace_back("result", (testcase.second.count("failure") > 0 ? "Error" : "Ok"));

			table_details.emplace_back(name, row);
		}
	}

	result.output.emplace_back("tests", table_details);

	// generate console output
	for(auto line : testResult.output)
	{
		if(line.second.find("[  FAILED  ]") != std::string::npos)
		{
			line.first = process::TextProcessResult::ERROR_LINE;
		}
		else
		if(line.second.find("[       OK ]") != std::string::npos || line.second.find("[  PASSED  ]") != std::string::npos)
		{
			line.first = process::TextProcessResult::OK_LINE;
		}
	}

	result.output.emplace_back("googletest", task_utils::createTaskOutput(config.value("binary"), arguments, parentPath.string(), testResult));

	// generate meta data
	result.message = task_utils::createTaskMessage(testResult);
	result.warnings = 0;
	result.errors = xmlTestResult.get<int>("testsuites.<xmlattr>.failures") + xmlTestResult.get<int>("testsuites.<xmlattr>.errors");
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}


TaskResult task_analysis_cppcheck( config::ConfigNode config )
{
	boost::filesystem::path xmlFilePath = config.value("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	TaskResult result;

	std::vector<std::string> arguments { "--xml-version=2", "--enable=all", "--suppress=missingIncludeSystem", "." };

	process::TextProcessResult checkResult = process::executeTextProcess(
		config.value("binary"),
		arguments,
		config.value("source"));

	if(checkResult.exitCode == 0)
	{
		std::string xmlCheckData;

		for(auto line = checkResult.output.begin(); line != checkResult.output.end(); )
		{
			if(line->first == process::TextProcessResult::ERROR_LINE)
			{
				xmlCheckData += line->second + std::string("\n");
				line = checkResult.output.erase(line);
			}
			else
			{
				++line;
			}
		}

		// write xml data
		std::ofstream xmlCheckPersistStream;
		xmlCheckPersistStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		xmlCheckPersistStream.open( config.value("output") );

		xmlCheckPersistStream << xmlCheckData;
		xmlCheckPersistStream.close();

		// read XML result data
		pt::ptree xmlCheckResult;

		std::istringstream xmlCheckResultStream(xmlCheckData);
		xmlCheckResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

		pt::read_xml( xmlCheckResultStream, xmlCheckResult, pt::xml_parser::trim_whitespace );

		// interpret xml data
		js::Array errors;
		for ( auto error : xmlCheckResult.get_child("results.errors") )
		{
			if(error.first == "<xmlattr>")
				continue;

			auto attr_file = error.second.get_optional<std::string>("location.<xmlattr>.file");
			auto attr_line = error.second.get_optional<std::string>("location.<xmlattr>.line");

			js::Object row;
			row.emplace_back("type", error.second.get<std::string>("<xmlattr>.id"));
			row.emplace_back("severity", error.second.get<std::string>("<xmlattr>.severity"));
			row.emplace_back("message", error.second.get<std::string>("<xmlattr>.msg"));
			row.emplace_back("file", attr_file ? *attr_file : std::string(""));
			row.emplace_back("line", attr_line ? *attr_line : std::string(""));

			errors.push_back(row);
		}
		result.output.emplace_back("errors", errors);

		result.warnings = errors.size();
		result.errors = 0;
	}
	else
	{
		result.warnings = 0;
		result.errors = 1;
	}

	result.output.emplace_back("cppcheck", task_utils::createTaskOutput(
		config.value("binary"),
		arguments,
		config.value("source"),
		checkResult));

	result.message = task_utils::createTaskMessage(checkResult);
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : (result.warnings > 0 ?  TaskResult::STATUS_WARNING : TaskResult::STATUS_OK));

	return result;
}


TaskResult task_doc_doxygen( config::ConfigNode config )
{
	TaskResult result;

	// prepare arguments and dirs

	const std::string sourcePath = config.value("source");
	const std::string outputPath = config.value("output");
	const std::string doxyfilePath = (boost::filesystem::path(outputPath) / boost::filesystem::path("doxyfile")).string();

	boost::filesystem::create_directories(outputPath);

	// generate doxyfile

	std::ofstream doxyfileStream;
	doxyfileStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	doxyfileStream.open( doxyfilePath );

	for( auto data : config.node("doxyfile").children() )
	{
		doxyfileStream << data.first << " = " << data.second.value() << '\n';
	}

	doxyfileStream << "INPUT" << " = " << sourcePath << '\n';
	doxyfileStream << "OUTPUT_DIRECTORY" << " = " << outputPath << '\n';

	doxyfileStream.close();

	// run doxygen
	process::TextProcessResult doxygenResult = process::executeTextProcess(config.value("binary"), std::vector<std::string>{doxyfilePath}, outputPath);

	result.output.emplace_back("doxygen", task_utils::createTaskOutput(config.value("binary"), std::vector<std::string>{doxyfilePath}, outputPath, doxygenResult));

	result.message = task_utils::createTaskMessage(doxygenResult);
	result.warnings = 0;
	result.errors = (doxygenResult.exitCode != 0 ? 1 : 0);
	result.status = (doxygenResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}

TaskResult task_publish_rsync_ssh( config::ConfigNode config )
{
	TaskResult result;

	for( auto source : config.node("sources").children() )
	{
		std::string srcdir = source.second.value();
		std::string remote = config.value("destination.user") + std::string("@") + config.value("destination.host");
		std::string destdir = config.value("destination.base") + std::string("/") + config.value("destination.directory") + std::string("/") + source.first;

		// run ssh:mkdir
		std::vector<std::string> sshArgs { "-o", "BatchMode=yes", "-p", config.value("destination.port"), remote, config.value("destination.mkdir.binary"), "-p", destdir };

		process::TextProcessResult sshResult = process::executeTextProcess(config.value("ssh.binary"), sshArgs, srcdir);

		result.output.emplace_back("ssh:mkdir", task_utils::createTaskOutput(config.value("ssh.binary"), sshArgs, srcdir, sshResult));
		result.message = task_utils::createTaskMessage(sshResult);

		if(sshResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}

		// run rsync
		std::vector<std::string> rsyncArgs {
			std::string("--rsh=") + config.value("ssh.binary") + (" -o BatchMode=yes -p ") + config.value("destination.port"), "--archive", "--delete", "--verbose", std::string("./"),
			remote + std::string(":") + destdir + std::string("/")
		};

		process::TextProcessResult rsyncResult = process::executeTextProcess(config.value("rsync.binary"), rsyncArgs, srcdir);

		result.output.emplace_back("rsync", task_utils::createTaskOutput(config.value("rsync.binary"), rsyncArgs, srcdir, rsyncResult));
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

TaskResult task_publish_mongo_rsync_ssh( config::ConfigNode config )
{
	// rsync stuff
	TaskResult result = task_publish_rsync_ssh(config);

	if(result.status == TaskResult::STATUS_ERROR)
	{
		return result;
	}

	// import to mongo database
	for( auto source : config.node("sources").children() )
	{
		std::string srcdir = source.second.value();
		std::string remote = config.value("destination.user") + std::string("@") + config.value("destination.host");
		std::string destfile = config.value("destination.base") + std::string("/") + config.value("destination.directory") + std::string("/") + source.first;

		// run ssh:mongoimport
		std::vector<std::string> mongoArgs { "-o", "BatchMode=yes", "-p", config.value("destination.port"), remote,
			config.value("destination.mongoimport.binary"),
			"--db", config.value("destination.mongoimport.database"),
			"--collection", config.value("destination.mongoimport.collection"),
			destfile };

		process::TextProcessResult mongoResult = process::executeTextProcess(config.value("ssh.binary"), mongoArgs, srcdir);

		result.output.emplace_back("ssh:mongoimport", task_utils::createTaskOutput(config.value("ssh.binary"), mongoArgs, srcdir, mongoResult));
		result.message = task_utils::createTaskMessage(mongoResult);

		if(mongoResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}
	}

	return result;
}

TaskResult task_publish_email( config::ConfigNode config )
{
	TaskResult result;
	result.warnings = 0;
	result.errors = 0;

	// detect authors
	process::TextProcessResult gitAuthors = process::executeTextProcess(
		config.value("git.binary"), {"log", "--pretty=%ae", std::string("--since=")+config.value("receivers.authors")},
		config.value("receivers.repository"));

	result.message = task_utils::createTaskMessage(gitAuthors);

	if(gitAuthors.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	// detect committers
	process::TextProcessResult gitCommitters = process::executeTextProcess(
		config.value("git.binary"), {"log", "--pretty=%ae", std::string("--since=")+config.value("receivers.committers")},
		config.value("receivers.repository"));

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

	// generate message
	std::stringstream message;

	for(auto report : config.node("reports").children())
	{
		// load report
		std::ifstream reportstream;
		reportstream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		reportstream.open( report.second.value() );

		boost::property_tree::ptree reporttree;
		boost::property_tree::read_json( reportstream, reporttree );

		// format report
		formatter::markdown(reporttree, message);
	}

	// send mail
	std::vector<std::string> mailArgs{
		"-a", "Content-Type: text/plain; charset=UTF-8",
		"-s", config.value("subject")
	};
	mailArgs.insert(mailArgs.end(), receivers.begin(), receivers.end());

	process::TextProcessResult mailResult = process::executeTextProcess(
		config.value("mail.binary"), mailArgs,
		config.value("receivers.repository"),
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

std::string toString(TaskResult::Status status)
{
	switch(status)
	{
		case TaskResult::STATUS_OK : return "ok";
		case TaskResult::STATUS_WARNING : return "warning";
		case TaskResult::STATUS_ERROR : return "error";
	}
	throw std::logic_error("Unsupportet TaskResult::Status!");
}

} // namespace: tasks
