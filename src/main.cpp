#include <fstream>
#include <iostream>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/system/system_error.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#include "tasks.hpp"
#include "process.hpp"

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

void publish( const config::Config& config );

int main( int argc, const char* const* argv )
{
	config::Config conf;
	boost::filesystem::path inputPath, outputPath;
	boost::program_options::variables_map vm;

	try
	{
		bool commitTimestampParsed = false;

		// read base configuration
		std::cout << "Load builtin base configuration..." << std::endl;
		conf.apply(config::Config::Priority::Base, config::builtin::base);

		// generate report id
		conf.apply(config::Config::Priority::Computed, "meta.id", boost::lexical_cast<std::string>(boost::uuids::random_generator()()));

		// read arguments
		std::cout << "Reading arguments..." << std::endl;

		std::string argMode = "standard", argInput, argOutput;
		std::vector<std::string> argOptions;

		{
			boost::program_options::options_description optdescr;

			optdescr.add_options()
				("mode,m", boost::program_options::value<std::string>(&argMode), "mode: standard [default], jenkins")
				("input,i", boost::program_options::value<std::string>(&argInput), "input directory")
				("output,o", boost::program_options::value<std::string>(&argOutput), "output directory")
				("options,O", boost::program_options::value<std::vector<std::string>>(&argOptions)->multitoken(), "options: key=value ...")
				("printconf,p", "print configuration and exit")
				("help,h", "show this text")
				;

			boost::program_options::store( boost::program_options::command_line_parser(argc, argv).options(optdescr).run(), vm);
			boost::program_options::notify(vm);

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

			conf.apply(config::Config::Priority::Environment, "meta.system.name", std::string(hostname));
		}

		if(argMode == "jenkins")
		{
			if(auto nodeName = environment::variable("NODE_NAME"))
			{
				conf.apply(config::Config::Priority::Environment, "meta.system.name", *nodeName);
			}

			if(auto workspace = environment::variable("WORKSPACE"))
			{
				conf.apply(config::Config::Priority::Environment, "meta.input", *workspace);
			}
		}

		if(auto env = environment::variable("OAK_SYSCONFIG"))
		{
			conf.apply(config::Config::Priority::Environment, "meta.configs.system", *env);
		}

#if defined(__x86_64__) or defined(_M_X64)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.family", std::string("x86"));
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", (uon::Number)64);
#elif defined(_X86_) or defined(__i386__) or defined(_M_IX86)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.family", std::string("x86"));
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", (uon::Number)32);
#elif defined(__aarch64__)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.family", std::string("arm"));
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", (uon::Number)64);
#elif defined(__arm__) or defined(_M_ARM)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.family", std::string("arm"));
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", (uon::Number)32);
#endif

#if defined(__linux__) or defined(__APPLE__)
		{
			// lsb release id
			process::TextProcessResult getConfLongBit = process::executeTextProcess("getconf", {"LONG_BIT"}, boost::filesystem::current_path());

			if(getConfLongBit.exitCode == 0 && getConfLongBit.output.size() == 1 && getConfLongBit.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", std::stold(getConfLongBit.output[0].second));
			}
			else
			{
				std::cerr << "Could not detect long bit value via getconf" << std::endl;
				return 1;
			}
		}
#elif defined(_WIN64)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", (uon::Number)64);
#elif defined(__MINGW32__) or defined(_WIN32)
		{
		    BOOL f64 = FALSE;
		    if(IsWow64Process(GetCurrentProcess(), &f64) == 0)
		    {
				std::cerr << "Could not detect bitness of current windows" << std::endl;
				return 1;
		    }

		    conf.apply(config::Config::Priority::Environment, "meta.system.arch.bitness", f64 ? (uon::Number)64 : (uon::Number)32);
		}
#endif

#if defined(__linux__)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.os", std::string("linux"));
#elif defined(__MINGW32__) or defined(_WIN32)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.os", std::string("windows"));
#elif defined(__APPLE__)
		conf.apply(config::Config::Priority::Environment, "meta.system.arch.os", std::string("macos"));
#endif

#if defined(__linux__)
		{
			std::string distribution;

			// lsb release id
			process::TextProcessResult lsbReleaseId = process::executeTextProcess("lsb_release", {"-s", "-i"}, boost::filesystem::current_path());

			if(lsbReleaseId.exitCode == 0 && lsbReleaseId.output.size() == 1 && lsbReleaseId.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				distribution += lsbReleaseId.output[0].second;
			}
			else
			{
				std::cerr << "Could not detect lsb release id" << std::endl;
				return 1;
			}

			// lsb release version
			process::TextProcessResult lsbReleaseVersion = process::executeTextProcess("lsb_release", {"-s", "-r"}, boost::filesystem::current_path());

			if(lsbReleaseVersion.exitCode == 0 && lsbReleaseVersion.output.size() == 1 && lsbReleaseVersion.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				distribution += std::string("-") + lsbReleaseVersion.output[0].second;
			}
			else
			{
				std::cerr << "Could not detect lsb release version" << std::endl;
				return 1;
			}

			// meta.system.arch.distribution
			std::transform(distribution.begin(), distribution.end(), distribution.begin(), ::tolower);
			conf.apply(config::Config::Priority::Environment, "meta.system.arch.distribution", distribution);
		}
#elif defined(__MINGW32__) or defined(_WIN32)
		{
			std::string distribution;

			OSVERSIONINFO vi;
			memset(&vi, 0, sizeof(vi));
			vi.dwOSVersionInfoSize = sizeof(vi);
			GetVersionEx(&vi);

			distribution = std::to_string(vi.dwMajorVersion) + "." + std::to_string(vi.dwMinorVersion) + "." + std::to_string(vi.dwBuildNumber);
			conf.apply(config::Config::Priority::Environment, "meta.system.arch.distribution", distribution);
		}
#elif defined(__APPLE__)
		{
			std::string distribution;

			SInt32 major, minor, bugfix;
			Gestalt(gestaltSystemVersionMajor, &major);
			Gestalt(gestaltSystemVersionMinor, &minor);
			Gestalt(gestaltSystemVersionBugFix, &bugfix);

			distribution = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(bugfix);
			conf.apply(config::Config::Priority::Environment, "meta.system.arch.distribution", distribution);
		}
#endif

#if defined(__linux__)
		{
			uid_t uid = geteuid();
			struct passwd *pw = getpwuid(uid);
			if (pw)
			{
				conf.apply(config::Config::Priority::Environment, "meta.system.user", std::string(pw->pw_name));
			}
		}
#endif

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
		boost::filesystem::path sysconf = conf.get("meta.configs.system").to_string();

		if(boost::filesystem::exists(sysconf))
		{
			std::cout << "Load system configuration..." << std::endl;
			conf.apply(config::Config::Priority::System, sysconf);
		}

		// apply project configuration
		boost::filesystem::path projectconf = conf.get("meta.configs.project").to_string();

		if(boost::filesystem::exists(projectconf))
		{
			std::cout << "Load project configuration..." << std::endl;
			conf.apply(config::Config::Priority::Project, projectconf);
		}

		// apply variant configuration
		std::cout << "Load variant configuration..." << std::endl;
		conf.apply(config::Config::Priority::Variant, config::builtin::variants.at(conf.get("meta.variant").to_string()));

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

			/*
			if(auto timestampstr = environment::variable("BUILD_ID"))
			{
				commitTimestampParsed = true;

				// parse timestamp
				boost::posix_time::ptime timestamp;

				std::cout << "Trying to parse timestamp " << *timestampstr << std::endl;

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

				auto output_facet = new boost::posix_time::time_facet("%Y-%m-%d %H:%M:%S");
				fs1.imbue(std::locale(fs1.getloc(), output_facet));
				fs1 << timestamp;

				conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.default", fs1.str());

				// compact format
				std::stringstream fs2;

				output_facet = new boost::posix_time::time_facet("%Y%m%d-%H%M%S");
				fs2.imbue(std::locale(fs2.getloc(), output_facet));
				fs2 << timestamp;

				conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.compact", fs2.str());
			}
			*/
		}

		inputPath = fs_utils::normalize(conf.get("meta.input").to_string());

		if( boost::filesystem::exists(inputPath / ".git") )
		{
			// meta.repository
			process::TextProcessResult gitRepository = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"config", "--get", "remote.origin.url"}, inputPath);

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
			if(argMode != "jenkins")
			{
				process::TextProcessResult gitBranch = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"symbolic-ref", "--short", "-q", "HEAD"}, inputPath);

				if(gitBranch.exitCode == 0 && gitBranch.output.size() == 1 && gitBranch.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					conf.apply(config::Config::Priority::Environment, "meta.branch", gitBranch.output[0].second);
				}
				else
				{
					std::cerr << "Could not detect git branch" << std::endl;
					return 1;
				}
			}

			// meta.commit.id.long
			process::TextProcessResult gitCommit = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"rev-parse", "--verify", "-q", "HEAD"}, inputPath);

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
			process::TextProcessResult gitTimestamp = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ci", conf.get("meta.commit.id.long").to_string()}, inputPath);

			if(gitTimestamp.exitCode == 0 && gitTimestamp.output.size() >= 1 && gitTimestamp.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.default", gitTimestamp.output[0].second);
				commitTimestampParsed = false;
			}
			else
			{
				std::cerr << "Could not detect git commit timestamp" << std::endl;
				return 1;
			}

			// meta.commit.committer.name
			process::TextProcessResult gitCommitterName = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%cn", conf.get("meta.commit.id.long").to_string()}, inputPath);

			if(gitCommitterName.exitCode == 0 && gitCommitterName.output.size() >= 1 && gitCommitterName.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.committer.name", gitCommitterName.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git committer name" << std::endl;
				return 1;
			}

			// meta.commit.committer.email
			process::TextProcessResult gitCommitterEmail = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ce", conf.get("meta.commit.id.long").to_string()}, inputPath);

			if(gitCommitterEmail.exitCode == 0 && gitCommitterEmail.output.size() >= 1 && gitCommitterEmail.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.committer.email", gitCommitterEmail.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git committer email" << std::endl;
				return 1;
			}

			// meta.commit.author.name
			process::TextProcessResult gitAuthorName = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%an", conf.get("meta.commit.id.long").to_string()}, inputPath);

			if(gitAuthorName.exitCode == 0 && gitAuthorName.output.size() >= 1 && gitAuthorName.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.author.name", gitAuthorName.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git author name" << std::endl;
				return 1;
			}

			// meta.commit.author.email
			process::TextProcessResult gitAuthorEmail = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ae", conf.get("meta.commit.id.long").to_string()}, inputPath);

			if(gitAuthorEmail.exitCode == 0 && gitAuthorEmail.output.size() >= 1 && gitAuthorEmail.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
			{
				conf.apply(config::Config::Priority::Environment, "meta.commit.author.email", gitAuthorEmail.output[0].second);
			}
			else
			{
				std::cerr << "Could not detect git author email" << std::endl;
				return 1;
			}

			// detect build gap begin
			boost::optional<std::string> buildGapBeginId;
			process::TextProcessResult gitBuildGapBegin = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"log", "--walk-reflogs", "--format=%H",  (argMode == "jenkins" ? "origin/" : "") + conf.get("meta.branch").to_string()}, inputPath);

			if(gitBuildGapBegin.exitCode == 0)
			{
				for(auto line = gitBuildGapBegin.output.begin(); line != gitBuildGapBegin.output.end(); ++line)
				{
					if(line->first != process::TextProcessResult::LineType::INFO_LINE)
					{
						std::cerr << "Could not detect begin of build gap" << std::endl;
						return 1;
					}

					if(line->second != conf.get("meta.commit.id.long").to_string())
						continue;

					++line;

					if(line == gitBuildGapBegin.output.end() || line->second.length() == 0)
						break;

					buildGapBeginId = line->second;
				}
			}
			else
			{
				std::cerr << "Could not detect begin of build gap" << std::endl;
				return 1;
			}

			std::vector<std::string> buildGapIds;

			if(buildGapBeginId)
			{
				process::TextProcessResult gitBuildGap = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"log", "--format=%H", (*buildGapBeginId)+".."+conf.get("meta.commit.id.long").to_string()}, inputPath);

				if(gitBuildGapBegin.exitCode != 0)
				{
					std::cerr << "Could not detect build gap" << std::endl;
					return 1;
				}

				for(auto line : gitBuildGap.output)
				{
					if(line.first != process::TextProcessResult::LineType::INFO_LINE)
					{
						std::cerr << "Could not detect build gap" << std::endl;
						return 1;
					}

					if(line.second == conf.get("meta.commit.id.long").to_string())
						continue;

					if(line.second.length() == 0)
						continue;

					buildGapIds.push_back(line.second);
				}
			}

			uon::Array buildGapEntries;

			for(auto buildGapId : buildGapIds)
			{
				uon::Value buildGapEntry;

				buildGapEntry.set("id.long", buildGapId);
				buildGapEntry.set("id.short", buildGapId.substr(0, 7));

				// timestamp.default
				process::TextProcessResult gitTimestamp = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ci", buildGapId}, inputPath);

				if(gitTimestamp.exitCode == 0 && gitTimestamp.output.size() >= 1 && gitTimestamp.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					// parse timestamp
					boost::posix_time::ptime timestamp;

					std::cout << "Trying to parse timestamp " << gitTimestamp.output[0].second << std::endl;

					std::stringstream ps(gitTimestamp.output[0].second);
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

					auto output_facet = new boost::posix_time::time_facet("%Y-%m-%d %H:%M:%S");
					fs1.imbue(std::locale(fs1.getloc(), output_facet));
					fs1 << timestamp;

					buildGapEntry.set("timestamp.default", fs1.str());

					// compact format
					std::stringstream fs2;

					output_facet = new boost::posix_time::time_facet("%Y%m%d-%H%M%S");
					fs2.imbue(std::locale(fs2.getloc(), output_facet));
					fs2 << timestamp;

					buildGapEntry.set("timestamp.compact", fs2.str());
				}
				else
				{
					std::cerr << "Could not detect git commit timestamp" << std::endl;
					return 1;
				}

				// committer.name
				process::TextProcessResult gitCommitterName = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%cn", buildGapId}, inputPath);

				if(gitCommitterName.exitCode == 0 && gitCommitterName.output.size() >= 1 && gitCommitterName.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					buildGapEntry.set("committer.name", gitCommitterName.output[0].second);
				}
				else
				{
					std::cerr << "Could not detect git committer name" << std::endl;
					return 1;
				}

				// committer.email
				process::TextProcessResult gitCommitterEmail = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ce", buildGapId}, inputPath);

				if(gitCommitterEmail.exitCode == 0 && gitCommitterEmail.output.size() >= 1 && gitCommitterEmail.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					buildGapEntry.set("committer.email", gitCommitterEmail.output[0].second);
				}
				else
				{
					std::cerr << "Could not detect git committer email" << std::endl;
					return 1;
				}

				// author.name
				process::TextProcessResult gitAuthorName = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%an", buildGapId}, inputPath);

				if(gitAuthorName.exitCode == 0 && gitAuthorName.output.size() >= 1 && gitAuthorName.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					buildGapEntry.set("author.name", gitAuthorName.output[0].second);
				}
				else
				{
					std::cerr << "Could not detect git author name" << std::endl;
					return 1;
				}

				// author.email
				process::TextProcessResult gitAuthorEmail = process::executeTextProcess(conf.get("tools.git.binary").to_string(), {"show", "-s", "--format=%ae", buildGapId}, inputPath);

				if(gitAuthorEmail.exitCode == 0 && gitAuthorEmail.output.size() >= 1 && gitAuthorEmail.output[0].first == process::TextProcessResult::LineType::INFO_LINE)
				{
					buildGapEntry.set("author.email", gitAuthorEmail.output[0].second);
				}
				else
				{
					std::cerr << "Could not detect git author email" << std::endl;
					return 1;
				}

				buildGapEntries.push_back(buildGapEntry);
			}

			conf.apply(config::Config::Priority::Environment, "meta.buildgap", uon::Value(buildGapEntries));
		}

		// trigger
		if(auto buildUserName = environment::variable("BUILD_USER"))
		{
			conf.apply(config::Config::Priority::Environment, "meta.trigger.name", *buildUserName);
		}

		if(auto buildUserEmail = environment::variable("BUILD_USER_EMAIL"))
		{
			conf.apply(config::Config::Priority::Environment, "meta.trigger.email", *buildUserEmail);
		}

		// computing values
		std::cout << "Computing additional configuration parameter..." << std::endl;

		// ids
		conf.apply(config::Config::Priority::Environment, "meta.commit.id.short", conf.get("meta.commit.id.long").to_string().substr(0, 7));

		// timestamps
		if(!commitTimestampParsed)
		{
			commitTimestampParsed = true;

			// parse timestamp
			boost::posix_time::ptime timestamp;

			std::cout << "Trying to parse timestamp " << conf.get("meta.commit.timestamp.default").to_string() << std::endl;

			std::stringstream ps(conf.get("meta.commit.timestamp.default").to_string());
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

			auto output_facet = new boost::posix_time::time_facet("%Y-%m-%d %H:%M:%S");
			fs1.imbue(std::locale(fs1.getloc(), output_facet));
			fs1 << timestamp;

			conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.default", fs1.str());

			// compact format
			std::stringstream fs2;

			output_facet = new boost::posix_time::time_facet("%Y%m%d-%H%M%S");
			fs2.imbue(std::locale(fs2.getloc(), output_facet));
			fs2 << timestamp;

			conf.apply(config::Config::Priority::Environment, "meta.commit.timestamp.compact", fs2.str());
		}

		// paths
		inputPath = fs_utils::normalize(conf.get("meta.input").to_string());
		outputPath = fs_utils::normalize(conf.get("meta.output").to_string());

		conf.apply(config::Config::Priority::Computed, "meta.input",  inputPath.string());
		conf.apply(config::Config::Priority::Computed, "meta.output", outputPath.string());

		conf.apply(config::Config::Priority::Computed, "meta.configs.system",  fs_utils::normalize(conf.get("meta.configs.system").to_string() ).string());
		conf.apply(config::Config::Priority::Computed, "meta.configs.project", fs_utils::normalize(conf.get("meta.configs.project").to_string()).string());

		conf.apply(config::Config::Priority::Computed, "meta.report",  fs_utils::normalize(conf.get("meta.report").to_string() ).string());

		// task defaults
		for( auto task : conf.get("tasks").as_object() )
		{
			conf.apply(config::Config::Priority::Base,
				std::string("tasks.") + task.first,
				conf.get( std::string("taskdefs.") + task.second.get("type").to_string() )
			);
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
	uon::write_json(conf.resolved(), std::cout);

	if(vm.count("printconf") > 0)
	{
		return 0;
	}

	// clean output
	std::cout << "Prepare output directory..." << std::endl;
	try
	{
#ifdef _WIN32
		fs_utils::remove_all(outputPath.string());
#else
		boost::filesystem::remove_all(outputPath);
#endif
		boost::filesystem::create_directories(outputPath);
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error while preparing output directory " << outputPath.string() << ": " << e.what() << std::endl;
		return 1;
	}
	catch(...)
	{
        std::cerr << "Error while preparing output directory " << outputPath.string() << ": unknown" << std::endl;
		return 1;
	}

	// run tasks
	bool task_with_error = false;

	boost::filesystem::path resultPath( conf.get("meta.report").to_string() );
	uon::Value taskResults;

	try
	{
		std::cout << "Task order: ";

		std::vector<std::string> taskOrder;
		std::set<std::string> taskResolved;

		std::function<void(std::string)> resolve = [&resolve, &conf, &taskOrder, &taskResolved] (std::string task)
		{
			if(taskResolved.find(task) != taskResolved.end())
				return;

			auto deps = conf.get(std::string("tasks.")+task+std::string(".dependencies")).as_object();

			for(auto dep : deps)
			{
				if(dep.second.to_boolean())
				{
					resolve(dep.first);
				}
			}

			taskOrder.push_back(task);
			taskResolved.insert(task);
			std::cout << task << " ";
		};

		for ( auto task : conf.get("tasks").as_object() )
		{
			resolve(task.first);
		}

		std::cout << std::endl;

		for ( auto task : taskOrder )
		{
			auto taskConfig = conf.get(std::string("tasks.")+task);
			auto taskType = taskConfig.get( "type" ).to_string();

			std::cout << "*************************************************************************" << std::endl;

			// check if it is enabled/disabled
			if(!taskConfig.get("enabled").to_boolean())
			{
				std::cout << "Task disabled: " << task << std::endl;
				std::cout << "type: " << taskType << std::endl;

				std::cout << "config: " << std::endl;
				uon::write_json(taskConfig, std::cout);
				std::cout << std::endl;

				std::cout << "*************************************************************************" << std::endl;
				continue;
			}

			// run task

			std::cout << "Running task: " << task << std::endl;
			std::cout << "type: " << taskType << std::endl;

			std::cout << "config: " << std::endl;
			uon::write_json(taskConfig, std::cout);
			std::cout << std::endl;

			std::cout << "*************************************************************************" << std::endl;

			auto taskFunc = tasks::taskTypes.find(taskType);

			if(taskFunc != tasks::taskTypes.end())
			{
				tasks::TaskResult result;

				try
				{
					result = taskFunc->second(taskConfig);
				}
				catch(const std::exception& e)
				{
					result.status = tasks::TaskResult::STATUS_ERROR;
					result.warnings = 0;
					result.errors = 1;
					result.message = std::string("exception occured: ") + e.what();
					result.output.set("exception", e.what());

					std::cerr << "An exception occured: " << e.what() << std::endl;
				}
				catch(...)
				{
					result.status = tasks::TaskResult::STATUS_ERROR;
					result.warnings = 0;
					result.errors = 1;
					result.message = "unknown exception occured";
					result.output.set("exception", "unknown exception");

					std::cerr << "An exception occured: unknown" << std::endl;
				}

				if(result.status == tasks::TaskResult::STATUS_ERROR)
				{
					task_with_error = true;
				}

				uon::Value taskResult;

				taskResult.set("type", taskType );
				taskResult.set("name", task );
				taskResult.set("message", result.message );
				taskResult.set("warnings", uon::Number(result.warnings));
				taskResult.set("errors",   uon::Number(result.errors));
				taskResult.set("status", toString(result.status));
				taskResult.set("details", result.output );
				taskResult.set("config",  taskConfig);

				taskResults.set(task, taskResult);
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
	uon::Value output;

	try
	{
		// assemble result
		output.set("meta", conf.get("meta"));
		output.set("tasks", taskResults);

		// ensure parent directory is created
		boost::filesystem::create_directories(resultPath.branch_path());

		// write file
		std::ofstream stream(resultPath.string());
		stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		uon::write_json(output, stream, false);
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

	// publish
	try
	{
		publish(conf);
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error on publish: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error on publish: unknown" << std::endl;
		return 1;
	}

	return task_with_error ? 2 : 0;
}

void publish( const config::Config& conf )
{
	if(!conf.get("publish.enabled").to_boolean())
	{
		std::cout << "Skipping publishing..." << std::endl;
		return;
	}

	std::cout << "Publishing" << std::endl;

	std::string remote = conf.get("publish.destination.remote").to_string();

	for( auto source : conf.get("publish.sources").as_object() )
	{
		std::string srcpath = source.second.to_string();
		std::string destpath = conf.get("publish.destination.path").to_string() + std::string("/") + source.first;

		// run ssh:mkdir
		std::vector<std::string> sshMkdirArgs { "-o", "BatchMode=yes", "-p", conf.get("publish.destination.port").to_string(), remote, conf.get("publish.destination.mkdir.binary").to_string(), "-p", destpath };

		process::TextProcessResult sshMkdirResult = process::executeTextProcess(conf.get("tools.ssh.binary").to_string(), sshMkdirArgs, srcpath);

		if(sshMkdirResult.exitCode != 0)
		{
			throw std::runtime_error("could not create directory on remote");
		}

		// run rsync
		std::vector<std::string> rsyncArgs {
			std::string("--rsh=") + conf.get("tools.ssh.binary").to_string() + (" -o BatchMode=yes -p ") + conf.get("publish.destination.port").to_string(), "--archive", "--delete", "--verbose", std::string("./"),
			remote + std::string(":") + destpath + std::string("/")
		};

		process::TextProcessResult rsyncResult = process::executeTextProcess(conf.get("tools.rsync.binary").to_string(), rsyncArgs, srcpath);

		if(rsyncResult.exitCode != 0)
		{
			throw std::runtime_error("could not sync files to remote");
		}
	}

	// run ssh:birch
	/*
	std::vector<std::string> sshBirchArgs { "-o", "BatchMode=yes", "-p", conf.get("publish.destination.port").to_string(), remote, conf.get("publish.destination.birch.binary").to_string(), "--letterbox", "--input=" + conf.get("publish.destination.path").to_string() };

	process::TextProcessResult sshBirchResult = process::executeTextProcess(conf.get("tools.ssh.binary").to_string(), sshBirchArgs, ".");

	if(sshBirchResult.exitCode != 0)
	{
		throw std::runtime_error("could not run birch on remote");
	}
	*/
}
