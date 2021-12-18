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
#include <fstream>
#include <sstream>

using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;

boost::asio::io_service io_context;

class session
: public std::enable_shared_from_this<session>
{
	public:
		unsigned char status_str[200];
		std::string SrcIP;
		std::string SrcPORT;
		std::string DstIP;
		std::string DstPORT; //printf("<D_PORT>: %u\n",DSTPORT);
		std::string cmd;
		ifstream file_input;
		bool accept;
		bool print_console;
		session(tcp::socket socket)
			: socket_(std::move(socket)),http_socket(io_context),resolve(io_context),bind_acceptor(io_context,tcp::endpoint(tcp::v4(), 0))
		{
			memset( domain, '\0', sizeof(char)*200 );
			print_console = false;
			file_input.open("socks.conf",ios::in);
		}
		void start()
		{
			do_read();
		}

	private:
		void checkfirewall(){
			string input;
			while(getline(file_input,input)){
				if(input[7] == 'c' && cmd == "CONNECT"){
					input = input.substr(9);
					stringstream X(input);
					stringstream Y(DstIP);
					//cerr << DstIP << endl;
					string permit,dst;
					int times = 0;
					while (getline(X, permit, '.') && getline(Y,dst,'.')) { 
						//cerr << "test" << endl;
						if(permit == "*"){
							accept = true;
							break;
						}
						else if(permit == dst){
							times++;
							continue;
						}else{
							break;
						}
					}
					if(times == 4) accept = true;
				}
				else if(input[7] == 'b' && cmd == "BIND"){
					input = input.substr(9);
					stringstream X(input);
					stringstream Y(DstIP);
					string permit,dst;
					int times = 0;
					while (getline(X, permit, '.') && getline(Y,dst,'.')) { 
						if(permit == "*"){
							accept = true;
							break;
						}
						else if(permit == dst){
							times++;
							continue;
						}else{
							break;
						}
					}
					if(times == 4) accept = true;
				}
			}
		}
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
						SrcIP = remote_ad.to_string();
						std::stringstream ss;
						ss << remote_ep.port();
						cmd = (data_[1] == 1) ? "CONNECT" : "BIND";
						SrcPORT = ss.str();
						sprintf(dip,"%u.%u.%u.%u",data_[4],data_[5],data_[6],data_[7]);
						DstIP = dip;
						/*
						printf("<S_IP>: %s\n",client.c_str());
						printf("<S_PORT>: %s\n",ss.str().c_str());
						printf("<D_IP>: %u.%u.%u.%u\n",data_[4],data_[5],data_[6],data_[7]);
						sprintf(dip,"%u.%u.%u.%u",data_[4],data_[5],data_[6],data_[7]);
						printf("<D_PORT>: %u\n",DSTPORT);
						printf("<Command>: %s\n",cmd.c_str());
						// Need to check accept or not
						printf("<Reply>: Accept\n");
						fflush(stdout);
						*/
						/* Extract domain name */
						accept = false;
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
						if(data_[1] == 2){
							checkfirewall();
							if(accept){
								status_str[1] = 90;
							}else{
								status_str[1] = 91;
							}
							bind_write();
							return;
						}
						bind_acceptor.close();
						do_write(length);
					}
					});
		}
		void bind_write(){
			bind_acceptor.set_option(tcp::acceptor::reuse_address(true));
			//bind_acceptor.bind(tcp::endpoint(tcp::v4(), 0));
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
									});
							else{
								status_str[1] = 91;
								print_console_msg();
								socket_.async_send( boost::asio::buffer(status_str, 8),
									[this,self](boost::system::error_code err, std::size_t len){
										if(!err) {
											socket_.close();
											http_socket.close();
											exit(0);
										}
									});
							}
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
			string domain_name = string(domain);
			if(domain_name.empty()){
				boost::asio::ip::tcp::endpoint dst_endpoint(boost::asio::ip::address::from_string(string(dip)),DSTPORT);
				checkfirewall();
				if(accept){
					status_str[1] = 90;
					http_socket.async_connect(dst_endpoint, boost::bind(&session::redirector, self,3,boost::asio::placeholders::error));
				}else{
					status_str[1] = 91;
					print_console_msg();
				}
			}
			else{
				std::stringstream ss;
				ss << DSTPORT;
				tcp::resolver::query query(domain_name, ss.str());
				resolve.async_resolve(query,boost::bind(&session::connection, self,boost::asio::placeholders::error,boost::asio::placeholders::iterator ));
			}
		}
		void connection(const boost::system::error_code& err,const tcp::resolver::iterator it){
			auto self(shared_from_this());
			if (!err)
			{
				boost::asio::ip::tcp::endpoint end = (*it);
				DstIP = end.address().to_string();
				checkfirewall();
				if(accept){
					status_str[1] = 90;
					http_socket.async_connect(*it, boost::bind(&session::redirector, self,3,boost::asio::placeholders::error));
				}else{
					status_str[1] = 91;
					print_console_msg();
				}
			}
		}
		void print_console_msg(){
			print_console = true;
			string result = accept ? "<Reply>: Accept\n" : "<Reply>: Reject\n";
			printf("<S_IP>: %s\n<S_PORT>: %s\n<D_IP>: %s\n<D_PORT>: %u\n<Command>: %s\n%s",
				SrcIP.c_str(),SrcPORT.c_str(),DstIP.c_str(),DSTPORT,cmd.c_str(),result.c_str());
			fflush(stdout);
			if(!accept){
				socket_.close();
				exit(0);
			}
		}
		void redirector(int num,const boost::system::error_code& err){
			auto self(shared_from_this());
			boost::asio::async_write(socket_, boost::asio::buffer(status_str, sizeof(unsigned char)*200),
					[this, self,num](boost::system::error_code ec, std::size_t /*length*/){
					if (!ec){
						clientread(num);
					}
					});
		}
		void clientread(int case_num){
			auto self(shared_from_this());
			if(!print_console) print_console_msg();
			if (case_num & 2) {
				http_socket.async_read_some(boost::asio::buffer(httpdata_, max_length),
				[this, self](boost::system::error_code err, std::size_t length) {
					if (!err){
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
				[this, self](boost::system::error_code err, std::size_t length) {
					if (!err) {
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
		enum { max_length = 200 };
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
