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

const std::string oakVersion = "0.4";
const std::string oakSysConfigDefault = "/etc/oak/defaults.json";

std::string argMode, argInput, argOutput, argSysConfig, argConfig, argResult;
std::string argMachine, argRepository, argBranch, argCommit, argTimestamp;

// GNU ld generates these funny symbols when generating .o files from arbitrary binary files:
extern const unsigned char _binary_configs_builtin_defaults_json_start[];
extern const unsigned char _binary_configs_builtin_defaults_json_end[];

extern const unsigned char _binary_configs_builtin_tasks_c___json_start[];
extern const unsigned char _binary_configs_builtin_tasks_c___json_end[];


struct CompileTimeData
{
	const unsigned char* begin;
	const unsigned char* end;
};

// currently we support only one variant. But for future use... :-)
const std::map<std::string, CompileTimeData> variants =
	{
		{"c++", { _binary_configs_builtin_tasks_c___json_start, _binary_configs_builtin_tasks_c___json_end } }
	};


// if 'variable' is "", fetch it from environment, if it exists there
void getFromEnvironment(std::string& variable, const char* const environ_name)
{
	if( not variable.empty() )
		return;
	
	auto e = getenv(environ_name);
	
	if(e == NULL)
	{
		return;
	}
	
	variable = std::string(e);
}


int complainIfNotSet(const std::string& variable, const char* const complainText)
{
	if(variable.empty())
	{
		std::cerr << "Problem: No " << complainText << " was given.\n";
		return 1;
	}
	return 0;
}


boost::filesystem::path normalize(const boost::filesystem::path& path)
{
	boost::filesystem::path absPath = boost::filesystem::absolute(path);
	boost::filesystem::path::iterator it = absPath.begin();
	boost::filesystem::path result = *it++;
	
	// Get canonical version of the existing part
	for (; exists(result / *it) && it != absPath.end(); ++it)
	{
		result /= *it;
	}
	result = boost::filesystem::canonical(result);
	
	// For the rest remove ".." and "." in a path with no symlinks
	for (; it != absPath.end(); ++it)
	{
		// Just move back on ../
		if (*it == "..")
		{
			result = result.parent_path();
		}
		// Ignore "."
		else if (*it != ".")
		{
			// Just cat other path entries
			result /= *it;
		}
	}
	return result;
}


int main( int argc, const char* const* argv )
{
	po::options_description desc;
	
	const std::string sysconfigDesc = "the JSON system config file (optional. compile-time default is " + oakSysConfigDefault + ")";
	desc.add_options()
		("mode,m"      , po::value<std::string>(&argMode)      , "the operational mode (standard [default], jenkins)")
		("input,i"     , po::value<std::string>(&argInput)     , "the path where the input files are expected (required in standard mode)")
		("output,o"    , po::value<std::string>(&argOutput)    , "the path where the output files shall be generated (required in standard mode)")
		("sysconfig,s" , po::value<std::string>(&argSysConfig) , sysconfigDesc.c_str() )
		("config,c"    , po::value<std::string>(&argConfig)    , "the JSON config file (required in standard mode)")
		("result,r"    , po::value<std::string>(&argResult)    , "the JSON result file (required in standard mode)")
		("machine,M"   , po::value<std::string>(&argMachine)   , "the build machine (required in standard mode)")
		("repository,R", po::value<std::string>(&argRepository), "the repository of the commit (required in standard mode)")
		("branch,B"    , po::value<std::string>(&argBranch)    , "the branch of the commit (required in standard mode")
		("commit,C"    , po::value<std::string>(&argCommit)    , "the id of the commit (required in standard mode")
		("timestamp,T" , po::value<std::string>(&argTimestamp) , "the timestamp of the commit (required in standard mode")
		("help,h", "show this text")
		;

	if(argc < 2)
	{
		std::cerr << desc << "\n"
		"This is oak version " << oakVersion << "\n\n";
		return 1;
	}

	po::variables_map vm;
	po::store( po::command_line_parser(argc, argv).options(desc).run(), vm);
	po::notify(vm);

	if(vm.count("help") > 0)
	{
		std::cout << desc << "\n"
		"This is oak version " << oakVersion << "\n\n";
		return 0;
	}

	if(argMode == "jenkins")
	{
		getFromEnvironment(argMachine   , "NODE_NAME" );
		getFromEnvironment(argRepository, "GIT_URL"   );
		getFromEnvironment(argBranch    , "GIT_BRANCH");
		if(argBranch.find("origin/") == 0)
		{
			argBranch = argBranch.substr(7);
		}
		
		getFromEnvironment(argCommit   , "GIT_COMMIT");
		getFromEnvironment(argTimestamp, "BUILD_ID"  );
		getFromEnvironment(argInput    , "WORKSPACE" );

		// build output directory from inpit directory + git infos:
		if(argOutput.empty())
		{
			if(argInput.length() > 0 && argBranch.length() > 0 && argTimestamp.length() > 0 && argCommit.length() > 0)
			{
				argOutput = argInput + "/oak/" + argBranch + "/" + argTimestamp + "_" + argCommit.substr(0, 7);
			}
		}

		// no config file given? search a default config in the input path:
		if(argConfig.empty())
		{
			if(argInput.length() > 0)
			{
				argConfig = argInput + "/ci.json";
			}
		}

		// not output JSON file given? put it into the output path:
		if(argResult.empty())
		{
			if(argOutput.length() > 0)
			{
				argResult = argOutput + "/result.json";
			}
		}
	}

	getFromEnvironment(argSysConfig, "OAK_SYSCONFIG");
#ifndef _WIN32
	if(argSysConfig.empty())
	{
		if(boost::filesystem::exists(oakSysConfigDefault))
		{
			argSysConfig = oakSysConfigDefault;
		}
	}
#endif

	if(argInput.empty() || argOutput.empty() || argConfig.empty() || argResult.empty())
	{
		std::cout << desc << "\n"
		"This is oak version " << oakVersion << "\n\n";
		return 1;
	}

	argInput = normalize(argInput).string();
	argOutput = normalize(argOutput).string();
	argConfig = normalize(argConfig).string();
	argResult = normalize(argResult).string();

	// use + instead of || to avoid short-circtuiting and report all errors at once
	if( complainIfNotSet(argInput, "input path")
		+ complainIfNotSet(argOutput, "output path" )
		+ complainIfNotSet(argMachine, "node name")
		+ complainIfNotSet(argRepository, "repository URL")
		+ complainIfNotSet(argBranch, "branch name")
		+ complainIfNotSet(argCommit, "commit ID")
		+ complainIfNotSet(argTimestamp, "commit timestamp")
		)
	{
		std::cerr << desc << "\n"
			"This is oak version " << oakVersion << "\n\n";
		return 1;
	}

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
			std::ifstream configStream;
			configStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
			configStream.open( argSysConfig );
			
			pt::ptree systemDefaultsConfig;
			read_json( configStream, systemDefaultsConfig );
			ptree_merge( defaultsConfig, systemDefaultsConfig );
		}

		// read variant configuration
		const std::string variantName = projectConfig.get<std::string>( "variant" );
		const auto v = variants.find( variantName );
		if(v==variants.end())
		{
			std::cerr << "Unknown variant: \"" << variantName << "\"!\nKnown variants are:\n";
			for(auto q : variants)
			{
				std::cerr << "\t\"" << q.first << "\"\n";
			}
			return 1;
		}

		const std::string variantData = std::string(v->second.begin, v->second.end);

		std::cout << "Load built-in variant configuration for \"" << variantName << "\"..." << std::endl;

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

	// clean output
	boost::filesystem::remove_all(argOutput);
	boost::filesystem::create_directories(argOutput);

	// run configurations
	bool task_with_error = false;

	js::Object outputTasks;
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
					boost::replace_all(child.data(), "${machine}", argMachine);
					boost::replace_all(child.data(), "${repository}", argRepository);
					boost::replace_all(child.data(), "${branch}"    , argBranch);
					boost::replace_all(child.data(), "${commit.id}" , argCommit);
					boost::replace_all(child.data(), "${commit.short-id}" , argCommit.substr(0, 7));
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

				outputTasks.emplace_back(taskConfig.first, outputTask);
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
		output.emplace_back( "buildnode", argMachine);
		output.emplace_back( "repository", argRepository);
		output.emplace_back( "branch"    , argBranch);
		output.emplace_back( "commit"    , argCommit);
		output.emplace_back( "timestamp" , argTimestamp);
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
