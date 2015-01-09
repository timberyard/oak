
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>

#include "process.hpp"

namespace bp  = boost::process;
namespace bpi = boost::process::initializers;
namespace bio = boost::iostreams;


TextProcessResult executeTextProcess(const std::string& binary, std::vector<std::string> arguments, const std::string& workingDirectory)
{
	if(!boost::filesystem::is_directory(workingDirectory))
		throw std::runtime_error("working directory does not exist");

	arguments.insert(arguments.begin(), binary);

	TextProcessResult result;

	boost::process::pipe pipeOut = bp::create_pipe();
	boost::process::pipe pipeErr = bp::create_pipe();

	std::shared_ptr<bp::child> process;

	{
		bio::file_descriptor_sink pipeOutSink(pipeOut.sink, bio::close_handle);
		bio::file_descriptor_sink pipeErrSink(pipeErr.sink, bio::close_handle);

		process = std::make_shared<bp::child>( bp::execute(
			bpi::run_exe(binary),
			bpi::set_args(arguments),
			bpi::start_in_dir(workingDirectory),
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
						result.output.push_back(std::make_pair(lineType, line));
						readLine(pipeEnd, lineBuf, lineType);
					}
				});
		};

	readLine(pipeOutEnd, lineBufOut, TextProcessResult::INFO_LINE);
	readLine(pipeErrEnd, lineBufErr, TextProcessResult::ERROR_LINE);

	io.run();

	result.exitCode = wait_for_exit(*process);

	return result;
}
