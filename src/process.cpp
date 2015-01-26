
#if defined(BOOST_WINDOWS_API)
#include <Windows.h>
#endif

#include <cstdlib>
#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>

#include "process.hpp"

namespace process {

namespace bp  = boost::process;
namespace bpi = boost::process::initializers;
namespace bio = boost::iostreams;

boost::process::pipe create_async_pipe()
{
#if defined(BOOST_WINDOWS_API)
	std::string name = "\\\\.\\pipe\\ci_run_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(std::rand());
	HANDLE handle1 = ::CreateNamedPipeA(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, 0, 1, 8192, 8192, 0, NULL);
	HANDLE handle2 = ::CreateFileA(name.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	return bp::make_pipe(handle1, handle2);
#elif defined(BOOST_POSIX_API)
	return bp::create_pipe();
#endif
}

TextProcessResult executeTextProcess(boost::filesystem::path binary, std::vector<std::string> arguments, boost::filesystem::path workingDirectory)
{
	std::cout << "-------------------------------------------------------------------------" << std::endl;
	std::cout << "Running process: " << binary.string() << std::endl;
	std::cout << "arguments: " << boost::algorithm::join(arguments, " ") << std::endl;
	std::cout << "working_dir: " << workingDirectory.string() << std::endl;
	std::cout << "-------------------------------------------------------------------------" << std::endl;

	if(!boost::filesystem::is_directory(workingDirectory))
		throw std::runtime_error("working directory does not exist");

	if(binary.is_relative())
	{
		auto found = boost::process::search_path(binary.string(), workingDirectory.string());

		if(found.length() > 0)
		{
			binary = found;
			std::cout << "binary found via working directory: " << binary.string() << std::endl;
		}
		else
		{
			found = boost::process::search_path(binary.string());

			if(found.length() > 0)
			{
				binary = found;
				std::cout << "binary found via path: " << binary.string() << std::endl;
			}
			else
			{
				std::cout << "could not find binary via relative path: " << binary.string() << std::endl;
				throw std::runtime_error(std::string("could not find binary via relative path: ") + binary.string());
			}
		}
	}
	else
	{
		auto found = boost::process::search_path(binary.relative_path().string(), workingDirectory.root_path().string());

		if(found.length() > 0)
		{
			binary = found;
			std::cout << "binary found via absolute path: " << binary.string() << std::endl;
		}
		else
		{
			std::cout << "could not find binary via absolute path: " << binary.string() << std::endl;
			throw std::runtime_error(std::string("could not find binary via absolute path: ") + binary.string());
		}
	}

	binary = boost::filesystem::canonical(binary);
	std::cout << "canonical path to binary: " << binary.string() << std::endl;

	std::cout << "-------------------------------------------------------------------------" << std::endl;

	arguments.insert(arguments.begin(), binary.string());

	TextProcessResult result;

	boost::process::pipe pipeOut = create_async_pipe();
	boost::process::pipe pipeErr = create_async_pipe();

	std::shared_ptr<bp::child> process;

	{
		bio::file_descriptor_sink pipeOutSink(pipeOut.sink, bio::close_handle);
		bio::file_descriptor_sink pipeErrSink(pipeErr.sink, bio::close_handle);

		process = std::make_shared<bp::child>( bp::execute(
			bpi::run_exe(binary.string()),
			bpi::set_args(arguments),
			bpi::start_in_dir(workingDirectory.string()),
			bpi::inherit_env(),
			bpi::bind_stdout(pipeOutSink),
			bpi::bind_stderr(pipeErrSink),
			bpi::throw_on_error()
		));
	}

#if defined(BOOST_WINDOWS_API)
	typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
	typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

	boost::asio::io_service io;

	pipe_end pipeOutEnd(io, pipeOut.source);
	pipe_end pipeErrEnd(io, pipeErr.source);

	boost::asio::streambuf lineBufOut;
	boost::asio::streambuf lineBufErr;

	std::function<void(pipe_end&,boost::asio::streambuf&,TextProcessResult::LineType)> readLine =
		[&readLine, &result](pipe_end& pipeEnd, boost::asio::streambuf& lineBuf, TextProcessResult::LineType lineType)
		{
			boost::asio::async_read_until(pipeEnd, lineBuf, "\n",
				[&readLine, &result, &pipeEnd, &lineBuf, lineType](const boost::system::error_code& error, std::size_t)
				{
					if(!error)
					{
						std::string line;
						std::istream lineStream(&lineBuf);
						std::getline(lineStream, line);

						boost::replace_all(line, "\033", "");
						result.output.push_back(std::make_pair(lineType, line));

						if(lineType != TextProcessResult::ERROR_LINE)
						{
							std::cout << line << std::endl;
						}
						else
						{
							std::cerr << line << std::endl;
						}

						readLine(pipeEnd, lineBuf, lineType);
					}
				});
		};

	readLine(pipeOutEnd, lineBufOut, TextProcessResult::INFO_LINE);
	readLine(pipeErrEnd, lineBufErr, TextProcessResult::ERROR_LINE);

	io.run();

	result.exitCode = wait_for_exit(*process);

	std::cout << "-------------------------------------------------------------------------" << std::endl;

	return result;
}


std::string toString(TextProcessResult::LineType lineType)
{
	switch(lineType)
	{
		case TextProcessResult::INFO_LINE : return "info";
		case TextProcessResult::ERROR_LINE : return "error";
		case TextProcessResult::OK_LINE    : return "ok";
	}
	throw std::logic_error( "Unknown TextProcessResult::LineType!" );
}

} // namespace: process
