#include <fstream>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/system/system_error.hpp>
#include <boost/filesystem.hpp>

#include "ptree.utils.hpp"
#include "tasks.hpp"

#include "json_spirit/json_spirit.h"

namespace pt = boost::property_tree;
namespace po = boost::program_options;
namespace js = json_spirit;

const std::string oakVersion = "0.1";
const std::string oakSysConfigDefault = "/etc/oak/defaults.json";

std::string argMode, argInput, argOutput, argSysConfig, argConfig, argResult;
std::string argMachine, argRepository, argBranch, argCommit, argTimestamp;

extern const unsigned char _binary_configs_builtin_defaults_json_start[];
extern const unsigned char _binary_configs_builtin_defaults_json_end[];

extern const unsigned char _binary_configs_builtin_tasks_c___json_start[];
extern const unsigned char _binary_configs_builtin_tasks_c___json_end[];

boost::optional<std::string> environment(std::string name)
{
	auto e = getenv(name.c_str());

	if(e == NULL)
	{
		return boost::optional<std::string>();
	}

	return boost::optional<std::string>(std::string(e));
}

boost::filesystem::path normalize(const boost::filesystem::path &path)
{
    boost::filesystem::path absPath = boost::filesystem::absolute(path);
    boost::filesystem::path::iterator it = absPath.begin();
    boost::filesystem::path result = *it++;

    // Get canonical version of the existing part
    for (; exists(result / *it) && it != absPath.end(); ++it) {
        result /= *it;
    }
    result = boost::filesystem::canonical(result);

    // For the rest remove ".." and "." in a path with no symlinks
    for (; it != absPath.end(); ++it) {
        // Just move back on ../
        if (*it == "..") {
            result = result.parent_path();
        }
        // Ignore "."
        else if (*it != ".") {
            // Just cat other path entries
            result /= *it;
        }
    }

    return result;
}

int main( int argc, const char* const* argv )
{
	po::options_description desc;
	desc.add_options()
		("mode,m"      , po::value<std::string>(&argMode)      , "the operational mode (standard [default], jenkins)")
		("input,i"     , po::value<std::string>(&argInput)     , "the path where the input files are expected (required in standard mode)")
		("output,o"    , po::value<std::string>(&argOutput)    , "the path where the output files shall be generated (required in standard mode)")
		("sysconfig,s" , po::value<std::string>(&argSysConfig) , (std::string("the JSON system config file (optional, default is ") + oakSysConfigDefault + std::string(")")).c_str())
		("config,c"    , po::value<std::string>(&argConfig)    , "the JSON config file (required in standard mode)")
		("result,r"    , po::value<std::string>(&argResult)    , "the JSON result file (required in standard mode)")
		("machine,M"   , po::value<std::string>(&argMachine)   , "the build machine (optional, only for decorating the output file)")
		("repository,R", po::value<std::string>(&argRepository), "the repository of the commit (optional, only for decorating the output file)")
		("branch,B"    , po::value<std::string>(&argBranch)    , "the branch of the commit (optional, only for decorating the output file)")
		("commit,C"    , po::value<std::string>(&argCommit)    , "the id of the commit (optional, only for decorating the output file)")
		("timestamp,T" , po::value<std::string>(&argTimestamp) , "the timestamp of the commit (optional, only for decorating the output file)")
		("help,h", "show this text")
		;

	if(argc < 2)
	{
		std::cerr << desc << std::endl;
		return 1;
	}

	po::variables_map vm;
	po::store( po::command_line_parser(argc, argv).options(desc).run(), vm);
	po::notify(vm);

	if(vm.count("help") > 0)
	{
		std::cout << desc << std::endl;
		return 0;
	}

	if(argMode == "jenkins")
	{
		auto envWorkspace = environment("WORKSPACE");
		auto envNodeName = environment("NODE_NAME");
		auto envGitUrl = environment("GIT_URL");
		auto envGitCommit = environment("GIT_COMMIT");
		auto envGitBranch = environment("GIT_BRANCH");
		auto envBuildId = environment("BUILD_ID");

		if(argMachine.length() == 0)
		{
			if(envNodeName)
				{ argMachine = *envNodeName; }
		}

		if(argRepository.length() == 0)
		{
			if(envGitUrl)
				{ argRepository = *envGitUrl; }
		}

		if(argBranch.length() == 0)
		{
			if(envGitBranch)
			{
				argBranch = *envGitBranch;

				if(argBranch.find("origin/") == 0)
				{
					argBranch = argBranch.substr(7);
				}
			}
		}

		if(argCommit.length() == 0)
		{
			if(envGitCommit)
				argCommit = *envGitCommit;
		}

		if(argTimestamp.length() == 0)
		{
			if(envBuildId)
				{ argTimestamp = *envBuildId; }
		}

		if(argInput.length() == 0)
		{
			if(envWorkspace)
				{ argInput = *envWorkspace; }
		}

		if(argOutput.length() == 0)
		{
			if(argInput.length() > 0 && argBranch.length() > 0 && argTimestamp.length() > 0 && argCommit.length() > 0)
			{
				argOutput = argInput + std::string("/oak/") + argBranch + std::string("/") + argTimestamp + std::string("_") + argCommit.substr(0, 7);
			}
		}

		if(argConfig.length() == 0)
		{
			if(argInput.length() > 0)
				{ argConfig = argInput + std::string("/ci.json"); }
		}

		if(argResult.length() == 0)
		{
			if(argOutput.length() > 0)
			{
				argResult = argOutput + std::string("/result.json");
			}
		}
	}

	if(argSysConfig.length() == 0)
	{
		auto envOakSysConfig = environment("OAK_SYSCONFIG");

		if(envOakSysConfig)
			{ argSysConfig = *envOakSysConfig; }
#ifndef _WIN32
		else if(boost::filesystem::exists(oakSysConfigDefault))
			{ argSysConfig = oakSysConfigDefault; }
#endif
	}

	if(argInput.length() == 0 || argOutput.length() == 0 || argConfig.length() == 0 || argResult.length() == 0)
	{
		std::cout << desc << std::endl;
		return 1;
	}

	argInput = normalize(argInput).string();
	argOutput = normalize(argOutput).string();
	argConfig = normalize(argConfig).string();
	argResult = normalize(argResult).string();

	// read configuration
	pt::ptree config;

	try
	{
		// read project config
		std::cout << "Load project configuration..." << std::endl;

		pt::ptree projectConfig;

		{
			std::ifstream stream;

			stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
			stream.open( argConfig );

			read_json( stream, projectConfig );
		}

		// read builtin defaults
		std::cout << "Load built-in default configuration..." << std::endl;

		pt::ptree defaultsConfig;

		{
			std::istringstream stream(std::string(_binary_configs_builtin_defaults_json_start, _binary_configs_builtin_defaults_json_end));
			stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

			read_json( stream, defaultsConfig );
		}

		// load system defaults
		if(argSysConfig.length() > 0)
		{
			std::cout << "Load system default configuration..." << std::endl;

			{
				std::ifstream configStream;
				configStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
				configStream.open( argSysConfig );

				pt::ptree systemDefaultsConfig;
				read_json( configStream, systemDefaultsConfig );
				ptree_merge( defaultsConfig, systemDefaultsConfig );
			}
		}

		// read variant configuration
		std::string variant = projectConfig.get<std::string>( "variant" );
		std::string variantData;

		if(variant == "c++")
		{
			variantData = std::string(_binary_configs_builtin_tasks_c___json_start, _binary_configs_builtin_tasks_c___json_end);
		}
		else
		{
			std::cerr << "Invalid variant: " << variant << std::endl;
			return 1;
		}

		std::cout << "Load built-in variant configuration for " << variant << "..." << std::endl;

		pt::ptree variantConfig;

		{
			std::istringstream stream(variantData);
			stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

			read_json( stream, variantConfig );
		}

		// merge configurations
		std::cout << "Merge configurations..." << std::endl;

		config.add_child("defaults", defaultsConfig);
		config.add_child("tasks", variantConfig);
		ptree_merge( config, projectConfig );
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << "Error while reading config file (ptree): " << exception.what() << std::endl;
		return 1;
	}
	catch ( const std::ifstream::failure& exception )
	{
		std::cerr << "Error while reading config file (stream): " << exception.what() << std::endl;
		return 1;
	}


	bool task_with_error = false;

	// run configurations
	js::Array outputTasks;
	try
	{
		for ( auto& taskConfig : config.get_child("tasks") )
		{
			std::cout << "*************************************************************************" << std::endl;

			// get task settings
			std::string taskType = taskConfig.second.get<std::string>( "type" );

			boost::optional<pt::ptree&> settings_defaults = config.get_child_optional( std::string( "defaults." ) + taskType );
			boost::optional<pt::ptree&> settings_specific = taskConfig.second.get_child_optional( "settings" );

			pt::ptree settings;

			if(settings_defaults)
				ptree_merge(settings, *settings_defaults);

			if(settings_specific)
				ptree_merge(settings, *settings_specific);

			// expand variables
			ptree_traverse
			(
				settings,
				[] (pt::ptree &parent, const pt::ptree::path_type &childPath, pt::ptree &child)
				{
					boost::replace_all(child.data(), "${repository}", argRepository);
					boost::replace_all(child.data(), "${branch}"    , argBranch);
					boost::replace_all(child.data(), "${commit.id}" , argCommit);
					boost::replace_all(child.data(), "${commit.timestamp}", argTimestamp);
					boost::replace_all(child.data(), "${input}", argInput);
					boost::replace_all(child.data(), "${output}", argOutput);
				}
			);

			// run task

			std::cout << "Running task: " << taskConfig.first << std::endl;
			std::cout << "type: " << taskType << std::endl;

			std::cout << "*************************************************************************" << std::endl;

			auto task = taskTypes.find(taskType);

			if(task != taskTypes.end())
			{
				TaskResult result;

				try
				{
					result = task->second(settings);
				}
				catch(const std::exception& e)
				{
					result.status = TaskResult::STATUS_ERROR;
					result.warnings = 0;
					result.errors = 1;
					result.message = "exception occured";
					result.output.push_back( js::Pair("exception", e.what()));
				}

				if(result.status == TaskResult::STATUS_ERROR)
				{
					task_with_error = true;
				}

				js::Object outputTask;

				outputTask.push_back( js::Pair("type", taskType ));
				outputTask.push_back( js::Pair("name", taskConfig.first ));
				outputTask.push_back( js::Pair("message", result.message ));
				outputTask.push_back( js::Pair("warnings", uint64_t(result.warnings)));
				outputTask.push_back( js::Pair("errors",   uint64_t(result.errors)));
				outputTask.push_back( js::Pair("status", toString(result.status)));
				outputTask.push_back( js::Pair("details", result.output ));

				outputTasks.push_back(outputTask);
			}
			else
				throw std::runtime_error(std::string("invalid task type: ") + taskType);

			std::cout << "*************************************************************************" << std::endl;
		}
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << "Error during run (ptree): " << exception.what() << std::endl;
		return 1;
	}
	catch ( const boost::system::system_error& exception )
	{
		std::cerr << "Error during run (system): " << exception.what() << std::endl;
		return 1;
	}
	catch ( const std::runtime_error& exception )
	{
		std::cerr << "Error during run (runtime): " << exception.what() << std::endl;
		return 1;
	}

	// dump output
	js::Object output;

	try
	{
		output.push_back( js::Pair("oak", oakVersion) );
		output.push_back( js::Pair("title", config.get<std::string>("name")));
		output.push_back( js::Pair("tasks", outputTasks));

		const js::Output_options options = js::Output_options( js::raw_utf8 | js::pretty_print | js::single_line_arrays );

		std::ofstream stream(argResult.c_str());
		stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

		js::write(output, stream, options);
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << "Error on output (ptree): " << exception.what() << std::endl;
		return 1;
	}

	return  task_with_error ? 2 : 0;
}
