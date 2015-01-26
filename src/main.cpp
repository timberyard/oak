#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/system/system_error.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <unistd.h>
#endif

#include <json_spirit/json_spirit.h>

#include "tasks.hpp"
#include "process.hpp"

namespace pt = boost::property_tree;
namespace po = boost::program_options;
namespace js = json_spirit;

namespace environment
{
	boost::optional<std::string> variable(std::string name)
	{
		auto value = getenv(name.c_str());

		if(value == NULL)
		{
			return boost::optional<std::string>();
		}

		return boost::optional<std::string>(value);
	}
}

namespace fs_utils
{
	boost::filesystem::path normalize(const boost::filesystem::path& path, const boost::filesystem::path& base = boost::filesystem::current_path())
	{
		boost::filesystem::path absolute = boost::filesystem::absolute(path, base);
		boost::filesystem::path::iterator it = absolute.begin();
		boost::filesystem::path result = *it++;

		// Get canonical version of the existing part
		for (; exists(result / *it) && it != absolute.end(); ++it)
		{
			result /= *it;
		}

		result = boost::filesystem::canonical(result);

		// For the rest remove ".." and "." in a path with no symlinks
		for (; it != absolute.end(); ++it)
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
}

#ifdef _WIN32

#undef __CRT__NO_INLINE
#include <Strsafe.h>
#define __CRT__NO_INLINE

namespace fs_utils
{
	void remove_all(std::string directory)
	{
		boost::replace_all(directory, "/", "\\");

	    char cdir[MAX_PATH+1];  // +1 for the double null terminate
	    SHFILEOPSTRUCTA fos = {0};

	    StringCchCopyA(cdir, MAX_PATH, directory.c_str());
	    int len = lstrlenA(cdir);
	    cdir[len+1] = 0; // double null terminate for SHFileOperation

	    // delete the folder and everything inside
	    fos.wFunc = FO_DELETE;
	    fos.pFrom = cdir;
	    fos.fFlags = FOF_NO_UI;
	    if( SHFileOperationA( &fos ) != 0 )
	    {
	    	throw std::runtime_error("could not delete directory");
	    }
	}
}

#endif

int main( int argc, const char* const* argv )
{
	config::Config conf;
	boost::filesystem::path input, output;

	try
	{
		bool commitTimestampParsed = false;

		// read base configuration
		std::cout << "Load builtin base configuration..." << std::endl;
		conf.apply(config::Config::Priority::Base, config::builtin::base);

		// read arguments
		std::cout << "Reading arguments..." << std::endl;

		std::string argMode = "standard", argInput, argOutput;
		std::vector<std::string> argOptions;

		{
			po::options_description optdescr;

			optdescr.add_options()
				("mode,m", po::value<std::string>(&argMode), "mode: standard [default], jenkins")
				("input,in", po::value<std::string>(&argInput), "input directory")
				("output,out", po::value<std::string>(&argOutput), "output directory")
				("options,opt", po::value<std::vector<std::string>>(&argOptions)->multitoken(), "options: key=value ...")
				("help,h", "show this text")
				;

			po::variables_map vm;
			po::store( po::command_line_parser(argc, argv).options(optdescr).run(), vm);
			po::notify(vm);

			if(vm.count("help") > 0)
			{
				std::cout << optdescr << "\n";
				return 0;
			}
		}

		{
			char hostname[256] = {};

			if(gethostname(hostname, sizeof(hostname)) != 0)
			{
				std::cerr << "could not retrieve hostname" << std::endl;
				return 1;
			}

			conf.apply(config::Config::Priority::Arguments, "meta.node.name", hostname);
		}

		if(argMode == "jenkins")
		{
			if(auto nodeName = environment::variable("NODE_NAME"))
			{
				conf.apply(config::Config::Priority::Arguments, "meta.node.name", *nodeName);
			}

			if(auto workspace = environment::variable("WORKSPACE"))
			{
				conf.apply(config::Config::Priority::Arguments, "meta.input", *workspace);
			}
		}

		if(auto env = environment::variable("OAK_SYSCONFIG"))
		{
			conf.apply(config::Config::Priority::Environment, "meta.configs.system", *env);
		}

		if(!argInput.empty())
		{
			conf.apply(config::Config::Priority::Arguments, "meta.input", argInput);
		}

		if(!argOutput.empty())
		{
			conf.apply(config::Config::Priority::Arguments, "meta.output", argOutput);
		}

		conf.apply(config::Config::Priority::Arguments, argOptions);

		// apply system configuration
		boost::filesystem::path sysconf = conf.value("meta.configs.system");

		if(boost::filesystem::exists(sysconf))
		{
			std::cout << "Load system configuration..." << std::endl;
			conf.apply(config::Config::Priority::System, sysconf);
		}

		// apply project configuration
		boost::filesystem::path projectconf = conf.value("meta.configs.project");

		if(boost::filesystem::exists(projectconf))
		{
			std::cout << "Load project configuration..." << std::endl;
			conf.apply(config::Config::Priority::Project, projectconf);
		}

		// apply checkout variant configuration
		std::cout << "Load checkout variant configuration..." << std::endl;
		conf.apply(config::Config::Priority::Variant, config::builtin::variants::checkout.at(conf.value("meta.variants.checkout")));

		// apply integrate variant configuration
		std::cout << "Load integrate variant configuration..." << std::endl;
		conf.apply(config::Config::Priority::Variant, config::builtin::variants::integrate.at(conf.value("meta.variants.integrate")));

		// apply publish variant configuration
		std::cout << "Load publish variant configuration..." << std::endl;
		conf.apply(config::Config::Priority::Variant, config::builtin::variants::publish.at(conf.value("meta.variants.publish")));

		// detect meta data
		std::cout << "Detecting meta data..." << std::endl;

		if(argMode == "jenkins")
		{
			if(auto branch = environment::variable("GIT_BRANCH"))
			{
				auto b = *branch;

				if(b.find("origin/") == 0)
				{
					b = b.substr(7);
				}

				conf.apply(config::Config::Priority::Environment, "meta.branch", b);
			}

			if(auto repo = environment::variable("GIT_URL"))
			{
				conf.apply(config::Config::Priority::Environment, "meta.repository", *repo);
			}

			if(auto commit = environment::variable("GIT_COMMIT"))
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.id.long", *commit);
			}

			if(auto timestampstr = environment::variable("BUILD_ID"))
			{
				// parse timestamp
				boost::posix_time::ptime timestamp;

				std::stringstream ps(*timestampstr);
				auto input_facet = new boost::posix_time::time_input_facet("%Y-%m-%d_%H-%M-%S");
				ps.imbue(std::locale(ps.getloc(), input_facet));
				ps >> timestamp;

				if(timestamp.is_not_a_date_time())
				{
					std::cerr << "Could not parse timestamp" << std::endl;
					return 1;
				}

				// default format
				std::stringstream fs1;
				fs1 << timestamp;

				conf.apply(config::Config::Priority::Computed, "meta.commit.timestamp.default", fs1.str());

				// compact format
				std::stringstream fs2;

				auto output_facet = new boost::posix_time::time_facet("%Y%m%d-%H%M%S");
				fs2.imbue(std::locale(fs2.getloc(), output_facet));
				fs2 << timestamp;

				conf.apply(config::Config::Priority::Computed, "meta.commit.timestamp.compact", fs2.str());

				// set state to parsed
				commitTimestampParsed = true;
			}
		}

		input = fs_utils::normalize(conf.value("meta.input"));

		if( boost::filesystem::exists(input / ".git") )
		{
			// meta.repository
			process::TextProcessResult gitRepository = process::executeTextProcess(conf.value("tasks.defaults.checkout:git.binary"), {"config", "--get", "remote.origin.url"}, input);

			if(gitRepository.exitCode == 0 && gitRepository.output.size() == 1 && gitRepository.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.repository", gitRepository.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git repository" << std::endl;
				return 1;
			}

			// meta.branch
			process::TextProcessResult gitBranch = process::executeTextProcess(conf.value("tasks.defaults.checkout:git.binary"), {"symbolic-ref", "--short", "-q", "HEAD"}, input);

			if(gitBranch.exitCode == 0 && gitBranch.output.size() == 1 && gitBranch.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.branch", gitBranch.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git branch" << std::endl;
				return 1;
			}

			// meta.commit.id.long
			process::TextProcessResult gitCommit = process::executeTextProcess(conf.value("tasks.defaults.checkout:git.binary"), {"rev-parse", "--verify", "-q", "HEAD"}, input);

			if(gitCommit.exitCode == 0 && gitCommit.output.size() == 1 && gitCommit.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.id.long", gitCommit.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git commit id" << std::endl;
				return 1;
			}

			// meta.commit.timestamp.default
			process::TextProcessResult gitTimestamp = process::executeTextProcess(conf.value("tasks.defaults.checkout:git.binary"), {"show", "-s", "--format=%ci", conf.value("meta.commit.id.long")}, input);

			if(gitTimestamp.exitCode == 0 && gitTimestamp.output.size() >= 1 && gitTimestamp.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.default", gitTimestamp.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git commit timestamp" << std::endl;
				return 1;
			}
		}

		// computing values
		std::cout << "Computing additional configuration parameter..." << std::endl;

		// ids
		conf.apply(config::Config::Priority::Environment, "meta.commit.id.short", conf.value("meta.commit.id.long").substr(0, 7));

		// timestamps
		if(!commitTimestampParsed)
		{
			// parse timestamp
			boost::posix_time::ptime timestamp;

			std::stringstream ps(conf.value("meta.commit.timestamp.default"));
			auto input_facet = new boost::posix_time::time_input_facet("%Y-%m-%d %H:%M:%S");
			ps.imbue(std::locale(ps.getloc(), input_facet));
			ps >> timestamp;

			if(timestamp.is_not_a_date_time())
			{
				std::cerr << "Could not parse timestamp" << std::endl;
				return 1;
			}

			// default format
			std::stringstream fs1;
			fs1 << timestamp;

			conf.apply(config::Config::Priority::Computed, "meta.commit.timestamp.default", fs1.str());

			// compact format
			std::stringstream fs2;

			auto output_facet = new boost::posix_time::time_facet("%Y%m%d-%H%M%S");
			fs2.imbue(std::locale(fs2.getloc(), output_facet));
			fs2 << timestamp;

			conf.apply(config::Config::Priority::Computed, "meta.commit.timestamp.compact", fs2.str());
		}

		// paths
		input = fs_utils::normalize(conf.value("meta.input"));
		output = fs_utils::normalize(conf.value("meta.output"));

		conf.apply(config::Config::Priority::Computed, "meta.input",  input.string());
		conf.apply(config::Config::Priority::Computed, "meta.output", output.string());

		conf.apply(config::Config::Priority::Computed, "meta.configs.system",  fs_utils::normalize(conf.value("meta.configs.system") ).string());
		conf.apply(config::Config::Priority::Computed, "meta.configs.project", fs_utils::normalize(conf.value("meta.configs.project")).string());

		conf.apply(config::Config::Priority::Computed, "meta.results.checkout",  fs_utils::normalize(conf.value("meta.results.checkout") ).string());
		conf.apply(config::Config::Priority::Computed, "meta.results.integrate", fs_utils::normalize(conf.value("meta.results.integrate")).string());
		conf.apply(config::Config::Priority::Computed, "meta.results.publish",   fs_utils::normalize(conf.value("meta.results.publish")  ).string());

		// task defaults
		for(auto section : std::vector<std::string>{"checkout", "integrate", "publish"})
		{
			for( auto task : conf.node(std::string("tasks.") + section).children() )
			{
				conf.apply(config::Config::Priority::Variant,
					std::string("tasks.") + section + std::string(".") + task.first,
					task.second
				);
			}
		}
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error while preparing configuration: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error while preparing configuration: unknown" << std::endl;
		return 1;
	}

	// print configuration
	std::cout << "Printing configuration..." << std::endl;
	conf.print(std::cout);

	// clean output
	std::cout << "Prepare output directory..." << std::endl;
	try
	{
#ifdef _WIN32
		fs_utils::remove_all(output);
#else
		boost::filesystem::remove_all(output);
#endif
		boost::filesystem::create_directories(output);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error while preparing output directory: " << e.what() << std::endl;
		return 1;
	}
	catch(...)
	{
		std::cerr << "Error while preparing output directory: unknown" << std::endl;
		return 1;
	}

	// collect task sets
	std::vector< std::pair<config::ConfigNode, std::string> > tasksets {
		std::pair<config::ConfigNode, std::string> { conf.node("tasks.checkout"), conf.value("meta.results.checkout") },
		std::pair<config::ConfigNode, std::string> { conf.node("tasks.integrate"), conf.value("meta.results.integrate") },
		std::pair<config::ConfigNode, std::string> { conf.node("tasks.publish"), conf.value("meta.results.publish") }
	};

	// run tasks
	bool task_with_error = false;

	for(auto taskset : tasksets)
	{
		js::Object outputTasks;

		try
		{
			for ( auto& taskConfig : taskset.first.children() )
			{
				auto taskType = taskConfig.second.value( "type" );

				std::cout << "*************************************************************************" << std::endl;

				// check if it is enabled/disabled
				if(taskConfig.second.value("enabled") != "yes")
				{
					std::cout << "Task disabled: " << taskConfig.first << std::endl;
					std::cout << "type: " << taskType << std::endl;

					std::cout << "config: " << std::endl;
					taskConfig.second.print(std::cout);
					std::cout << std::endl;

					std::cout << "*************************************************************************" << std::endl;
					continue;
				}

				// run task

				std::cout << "Running task: " << taskConfig.first << std::endl;
				std::cout << "type: " << taskType << std::endl;

				std::cout << "config: " << std::endl;
				taskConfig.second.print(std::cout);
				std::cout << std::endl;

				std::cout << "*************************************************************************" << std::endl;

				auto task = tasks::taskTypes.find(taskType);

				if(task != tasks::taskTypes.end())
				{
					tasks::TaskResult result;

					try
					{
						result = task->second(taskConfig.second);
					}
					catch(const std::exception& e)
					{
						result.status = tasks::TaskResult::STATUS_ERROR;
						result.warnings = 0;
						result.errors = 1;
						result.message = "exception occured";
						result.output.push_back( js::Pair("exception", e.what()));

						std::cout << "An exception occured: " << e.what() << std::endl;
					}
					catch(...)
					{
						result.status = tasks::TaskResult::STATUS_ERROR;
						result.warnings = 0;
						result.errors = 1;
						result.message = "exception occured";
						result.output.push_back( js::Pair("exception", "unknown exception"));

						std::cout << "An exception occured: unknown" << std::endl;
					}

					if(result.status == tasks::TaskResult::STATUS_ERROR)
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
		catch ( const std::exception& e )
		{
			std::cerr << "Error during task execution: " << e.what() << std::endl;
			return 1;
		}
		catch ( ... )
		{
			std::cerr << "Error during task execution: unknown" << std::endl;
			return 1;
		}

		// dump output
		js::Object output;

		try
		{
			//output.push_back( js::Pair("oak", oakVersion) );
			//output.push_back( js::Pair("title", conf.value("name")));
			//output.emplace_back( "buildnode", argMachine);
			//output.emplace_back( "repository", argRepository);
			//output.emplace_back( "branch"    , argBranch);
			//output.emplace_back( "commit"    , argCommit);
			//output.emplace_back( "timestamp" , argTimestamp);
			output.push_back( js::Pair("tasks", outputTasks));

			const js::Output_options options = js::Output_options( js::raw_utf8 | js::pretty_print | js::single_line_arrays );

			std::ofstream stream(taskset.second.c_str());
			stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

			js::write(output, stream, options);
		}
		catch ( const std::exception& e )
		{
			std::cerr << "Error on output: " << e.what() << std::endl;
			return 1;
		}
		catch ( ... )
		{
			std::cerr << "Error on output: unknown" << std::endl;
			return 1;
		}
	}

	return task_with_error ? 2 : 0;
}
