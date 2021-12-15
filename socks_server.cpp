//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <string>
#include <sstream>

using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;

boost::asio::io_service io_context;

class session
: public std::enable_shared_from_this<session>
{
	public:
		char REQUEST_METHOD[100];
		char REQUEST_URI[1124];
		char QUERY_STRING[1024];
		char SERVER_PROTOCOL[100];
		char HTTP_HOST[100];
		unsigned char status_str[200];
		std::string server;
		std::string server_port;
		std::string client;
		std::string client_port;
		session(tcp::socket socket)
			: socket_(std::move(socket)),http_socket(io_context),resolve(io_context),bind_acceptor(io_context)
		{
			memset( domain, '\0', sizeof(char)*200 );
		}
		void BOOST_SETENV(){
			setenv("REQUEST_METHOD",REQUEST_METHOD,1);
			setenv("REQUEST_URI",REQUEST_URI,1);
			setenv("QUERY_STRING",QUERY_STRING,1);
			setenv("SERVER_PROTOCOL",SERVER_PROTOCOL,1);
			setenv("HTTP_HOST",HTTP_HOST,1);
			setenv("SERVER_ADDR",server.c_str(),1);
			setenv("SERVER_PORT",server_port.c_str(),1);
			setenv("REMOTE_ADDR",client.c_str(),1);
			setenv("REMOTE_PORT",client_port.c_str(),1);
		}
		void start()
		{
			do_read();
		}

	private:
		void do_read()
		{
			auto self(shared_from_this());
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
					[this, self](boost::system::error_code ec, std::size_t length){
					if (!ec){
						DSTPORT = (data_[2] << 8) |data_[3];
						//unsigned int DSTIP = (data_[7] << 24) | (data_[6] << 16) | (data_[5] << 8) | data_[4] ;
						boost::asio::ip::tcp::endpoint remote_ep = socket_.remote_endpoint();
						boost::asio::ip::address remote_ad = remote_ep.address();
						string client = remote_ad.to_string();
						std::stringstream ss;
						ss << remote_ep.port();
						string cmd;
						cmd = (data_[1] == 1) ? "CONNECT" : "BIND";
						printf("<S_IP>: %s\n",client.c_str());
						printf("<S_PORT>: %s\n",ss.str().c_str());
						printf("<D_IP>: %u.%u.%u.%u\n",data_[4],data_[5],data_[6],data_[7]);
						sprintf(dip,"%u.%u.%u.%u",data_[4],data_[5],data_[6],data_[7]);
						printf("<D_PORT>: %u\n",DSTPORT);
						printf("<Command>: %s\n",cmd.c_str());
						/* Need to check accept or not */
						printf("<Reply>: Accept\n");
						fflush(stdout);
						/* Extract domain name */
						if(data_[4] == 0){
							int i,j=0;
							for(i = 8;i < max_length;i++){
								if(data_[i] == 0){
									i++;
									break;
								}
							}
							while(data_[i] != 0){
								domain[j++] = data_[i++];
							}
						}
						memset( status_str, '\0', sizeof(unsigned char)*200 );
						status_str[0] = 0;
						status_str[1] = 90;
						if(data_[1] == 2){
							bind_write();
							return;
						}
						bind_acceptor.close();
						do_write(length);
					}
					});
		}
		void bind_write(){
			unsigned short os_free_port(0);
			tcp::endpoint endPoint(tcp::endpoint(tcp::v4(), os_free_port));
			bind_acceptor.open(endPoint.protocol());
			bind_acceptor.set_option(tcp::acceptor::reuse_address(true));
			bind_acceptor.bind(endPoint);
			bind_acceptor.listen();
			bind_port = bind_acceptor.local_endpoint().port();
			status_str[2] = (unsigned char)(bind_port/256);
			status_str[3] = (unsigned char)(bind_port%256);
			auto self(shared_from_this());
			socket_.async_send( boost::asio::buffer(status_str, 8),
				[this,self](boost::system::error_code err, std::size_t length_){
					if(!err) {
						auto self(shared_from_this());
						bind_acceptor.async_accept(http_socket, [this,self](boost::system::error_code err) {
							if (!err)
								socket_.async_send( boost::asio::buffer(status_str, 8),
									[this,self](boost::system::error_code err, std::size_t len){
										if(!err) {
											clientread(3);
										}
										else
											cerr << err.message() << endl;
									});
							else
								cerr << err.message() << endl;
						});
					}
					else{
						cerr << err.message() << endl;
					}
				});
		}
		void do_write(std::size_t length)
		{
			auto self(shared_from_this());
			boost::asio::async_write(socket_, boost::asio::buffer(status_str, sizeof(unsigned char)*200),
					[this, self](boost::system::error_code ec, std::size_t /*length*/){
					if (!ec){
						string domain_name = string(domain);
						if(domain_name.empty()){
							boost::asio::ip::tcp::endpoint dst_endpoint(boost::asio::ip::address::from_string(string(dip)),DSTPORT);
							http_socket.async_connect(dst_endpoint, boost::bind(&session::redirector, self,3,boost::asio::placeholders::error));
						}
						else{
							std::stringstream ss;
							ss << DSTPORT;
							tcp::resolver::query query(domain_name, ss.str());
							resolve.async_resolve(query,boost::bind(&session::connection, self,boost::asio::placeholders::error,boost::asio::placeholders::iterator ));
						}
					}
					});
		}
		void connection(const boost::system::error_code& err,const tcp::resolver::iterator it){
			auto self(shared_from_this());
			if (!err)
			{
				http_socket.async_connect(*it, boost::bind(&session::redirector, self,3,boost::asio::placeholders::error));
			}
		}
		void redirector(int num,const boost::system::error_code& err){
			clientread(num);
		}
		void clientread(int case_num){
			auto self(shared_from_this());
			if (case_num & 1) {
				socket_.async_read_some(boost::asio::buffer(clientdata_, max_length), 
					[this, self](boost::system::error_code err, std::size_t length) {
					if (!err)
					{
						write_data(1, length);
					}
					else if (err == boost::asio::error::eof) {
						http_socket.async_send(boost::asio::buffer(clientdata_, length),
						[this,self,err](boost::system::error_code ec, std::size_t len) {
							if (!ec){}
							else {
								cerr << ec.message() << endl;
							}
							//cerr << "httpsocket = " << clientdata_ << endl;
							http_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
						});
					}
					else {
						cerr << err.message() << endl;
					}
						
				});
			}
			if (case_num & 2) {
				http_socket.async_read_some(boost::asio::buffer(httpdata_, max_length),
				[this, self](boost::system::error_code err, std::size_t length) {
					if (!err){
						//cerr << "http read = " << httpdata_ << endl;
						write_data(2, length);
					}
					else if (err == boost::asio::error::eof) {
						socket_.async_send(boost::asio::buffer(httpdata_, length),
						[this,self,err](boost::system::error_code ec, std::size_t len) {
							if (ec){
								cerr <<  ec.message() << endl;
							}
							//cerr << "socket_ = " << httpdata_ << endl;
							socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
						});
						//cerr << "httpsocket close" << endl;
					}
					else {
						cerr <<  err.message() << endl;
					} 
				});
			}
		}
		void write_data(int case_num, std::size_t len) {
			auto self(shared_from_this());
			switch(case_num) {
			case 1:
				http_socket.async_send(
				boost::asio::buffer(clientdata_, len),
				[this, self,len](boost::system::error_code err, std::size_t length) {
					if (!err){
						memset( clientdata_, '\0', max_length );
						clientread(1);
					}
					else {
						cerr << err.message() << endl;
					} 
				});
				break;
			case 2:
				socket_.async_send(
				boost::asio::buffer(httpdata_, len),
				[this, self,len](boost::system::error_code err, std::size_t length) {
					if (!err) {
						//cerr <<"port = "<<bind_port <<" send to socket = " << httpdata_ << endl;
						memset( httpdata_, '\0', max_length );
						clientread(2);
					}
					else {
						cerr << err.message() << endl;
					} 
				});
				break;
			}
		}
		tcp::socket socket_;
		tcp::socket http_socket;
		tcp::resolver resolve;
		enum { max_length = 1024 };
		unsigned char data_[max_length];
		char clientdata_[max_length];
		char httpdata_[max_length];
		char domain[200];
		char dip[20];
		unsigned int DSTPORT;
		unsigned short bind_port;
		tcp::acceptor bind_acceptor;
};

class server
{
	public:
		server(boost::asio::io_context& io_context, short port)
			: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
		{
			do_accept();
		}

	private:
		void do_accept()
		{
			acceptor_.async_accept(
					[this](boost::system::error_code ec, tcp::socket socket)
					{
					if (!ec)
					{
						io_context.notify_fork(io_service::fork_prepare);
						if (fork() != 0) {
							io_context.notify_fork(io_service::fork_parent);
							socket.close();
						} else {
							io_context.notify_fork(io_service::fork_child);
							acceptor_.close();
							std::make_shared<session>(std::move(socket))->start();
						}
						
					}
					do_accept();
					});
		}

		tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return 1;
		}

		server s(io_context, std::atoi(argv[1]));

		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
