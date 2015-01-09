
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>

#include "process.hpp"

using namespace std;
using namespace boost::process;
using namespace boost::process::initializers;
using namespace boost::iostreams;

TextProcessResult executeTextProcess(string binary, vector<string> arguments, const string& workingDirectory)
{
	if(!boost::filesystem::is_directory(workingDirectory))
		throw runtime_error("working directory does not exist");

	if(binary.find('/') == std::string::npos && binary.find('\\') == std::string::npos)
	{
		binary = boost::process::search_path(binary);
	}

	arguments.insert(arguments.begin(), binary);

	TextProcessResult result;

	boost::process::pipe pipeOut = create_pipe();
	boost::process::pipe pipeErr = create_pipe();

	shared_ptr<child> process;

	{
		file_descriptor_sink pipeOutSink(pipeOut.sink, close_handle);
		file_descriptor_sink pipeErrSink(pipeErr.sink, close_handle);

		process = make_shared<child>(execute(
			run_exe(binary),
		    set_args(arguments),
		    start_in_dir(workingDirectory),
		    inherit_env(),
		    bind_stdout(pipeOutSink),
		    bind_stderr(pipeErrSink),
		    throw_on_error()
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

    function<void(pipe_end&,boost::asio::streambuf&,TextProcessResult::LineType)> readLine =
	    [&readLine, &result](pipe_end& pipeEnd, boost::asio::streambuf& lineBuf, TextProcessResult::LineType lineType)
	    {
		    boost::asio::async_read_until(pipeEnd, lineBuf, "\n",
		        [&readLine, &result, &pipeEnd, &lineBuf, lineType](const boost::system::error_code& error, std::size_t)
		        {
		        	if(!error)
		        	{
		        		string line;
		        		std::istream lineStream(&lineBuf);
		        		std::getline(lineStream, line);
		        		result.output.push_back(pair<TextProcessResult::LineType, string>(lineType, line));
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
