#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <vector>

using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;


struct client_info{
  string host;
  string port;
  string file;
};

boost::asio::io_service io_context;
client_info client[6];


void output_shell(int index,string content){
  //cerr << content << endl;
  boost::replace_all(content, "\n\r", " ");
  boost::replace_all(content, "\n", "&NewLine;");
  boost::replace_all(content, "\r", " ");
  boost::replace_all(content, "'", "\\'");
  boost::replace_all(content, "<", "&lt;");
  boost::replace_all(content, ">", "&gt;");
  printf("<script>document.getElementById('s%d').innerHTML += '%s';</script>",index,content.c_str());
  fflush(stdout);
}

void output_command(int index,string content){
  //cerr << content << endl;
  boost::replace_all(content, "\n\r", " ");
  boost::replace_all(content, "\n", "&NewLine;");
  boost::replace_all(content, "\r", " ");
  boost::replace_all(content, "'", "\\'");
  boost::replace_all(content, "<", "&lt;");
  boost::replace_all(content, ">", "&gt;");
  printf("<script>document.getElementById('s%d').innerHTML += '<b>%s</b>';</script>",index,content.c_str());
  fflush(stdout);
}

void add_row(int index,string host,string port){
  printf("<script>var table = document.getElementById('table_tr'); table.innerHTML += '<th>%s:%s</th>';</script>",host.c_str(),port.c_str());
  printf("<script>var table = document.getElementById('session'); table.innerHTML += '<td><pre id=\\'s%d\\' class=\\'mb-0\\'></pre></td>&NewLine;' </script>",index);
  fflush(stdout);
}

class session
: public std::enable_shared_from_this<session>
{
	public:
		session(tcp::socket socket_,int index,string host, string port,string f)
			: client_socket(std::move(socket_))
		{
      if(host.length() == 0){
        return;
      }
      id = index;
      filename = "./test_case/"+f;
      file_input.open(filename,ios::in);
      all_msg = "";
      flag = false;
      //if ( file_input.fail() ) cerr << "open fail" << endl;
		}
		void start()
		{
			do_read();
		}

	private:
		void do_read()
		{
			auto self(shared_from_this());
      if(flag) return;
      client_socket.async_read_some(boost::asio::buffer(data_, max_length),
          [this,self](boost::system::error_code ec, std::size_t length){
          if (!ec){
            for(size_t i = 0;i < length;++i){
              if(data_[i]!='\0'){
                all_msg += data_[i];
              }
            }
            memset(data_,0,length);
            output_shell(id,all_msg);
            size_t pos;
            if((pos = all_msg.find("%")) != string::npos){
              do_write();
            }
            all_msg = "";
            do_read();
          }
          });
		}

		void do_write()
		{
			auto self(shared_from_this());
      string input;
      //if(file_input.eof()) return;
      if(!getline(file_input,input)){
        cerr << "getline fail" << endl;
      }
      input = input + "\n";
      cerr << "input = "<< input << endl;
      output_command(id,input);
      boost::asio::async_write(client_socket, boost::asio::buffer(input.c_str(), input.length()),
        [this,self,input](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec){
            if(input == "exit"){
              flag = true;
              client_socket.close();
            }
        }else{
          cerr << ec << endl;
        }
        });
    }

		tcp::socket client_socket;
		enum { max_length = 1024 };
		char data_[max_length];
    string all_msg;
    ifstream file_input;
    string filename;
    bool flag;
    int id;
};


void print_header(){
  cout << "Content-type: text/html\r\n\r\n";
  cout << "<!DOCTYPE html>\
<html lang=\"en\">\
  <head>\
    <meta charset=\"UTF-8\" />\
    <title>NP Project 3 Console</title>\
    <link\
      rel=\"stylesheet\"\
      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
      crossorigin=\"anonymous\"\
    />\
    <link\
      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
      rel=\"stylesheet\"\
    />\
    <link\
      rel=\"icon\"\
      type=\"image/png\"\
      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
    />\
    <style>\
      * {\
        font-family: 'Source Code Pro', monospace;\
        font-size: 1rem !important;\
      }\
      body {\
        background-color: #212529;\
      }\
      pre {\
        color: #cccccc;\
      }\
      b {\
        color: #01b468;\
      }\
    </style>\
  </head>\
  <body>\
    <table class=\"table table-dark table-bordered\">\
      <thead>\
        <tr id=\"table_tr\">\
        </tr>\
      </thead>\
      <tbody>\
        <tr id=\"session\">\
        </tr>\
      </tbody>\
    </table>\
  </body>\
</html>";
}

class server
{
  public:
    server()
    :resolve(io_context)
    {
      if(client[5].host.length() !=0){
        tcp::resolver::query query(client[5].host, client[5].file);
        resolve.async_resolve(query,
        boost::bind(&server::proxy_connection, this,boost::asio::placeholders::error,boost::asio::placeholders::iterator ));
        return;
      }
      for(int i = 0;i < 5;i++){
        if(client[i].host.length() !=0){
          tcp::resolver::query query(client[i].host, client[i].port);
          resolve.async_resolve(query,
          boost::bind(&server::connection, this,i,boost::asio::placeholders::error,boost::asio::placeholders::iterator ));
        }
      }
    }
    void proxy_connection(const boost::system::error_code& err,const tcp::resolver::iterator it){
      if(!err){
        for(int i = 0;i < 5;i++){
          if(client[i].host.length() !=0){
            socket_[i] = new tcp::socket(io_context);
            (*socket_[i]).async_connect(*it,
            boost::bind(&server::create_proxy_session, this,i,boost::asio::placeholders::error,it));
          }
        }
      }
    }
    void connection(const int i,const boost::system::error_code& err,const tcp::resolver::iterator it){
      if (!err)
      {
        socket_[i] = new tcp::socket(io_context);
        (*socket_[i]).async_connect(*it,
        boost::bind(&server::create_session, this,i,boost::asio::placeholders::error,it ));
      }
    }
    void create_session(const int i,const boost::system::error_code& err,const tcp::resolver::iterator it){
      if (!err)
      {
          std::make_shared<session>(std::move(*socket_[i]),i,client[i].host, client[i].port,client[i].file)->start();
      }
    }
    void create_proxy_session(const int i,const boost::system::error_code& err,const tcp::resolver::iterator it){
      if (!err)
      {
        size_t j = 0;
        unsigned char socks_request[200];
        memset(socks_request,'\0',200);
        socks_request[0] = 4;
        socks_request[1] = 1;
        socks_request[2] = stoi(client[i].port)/256;
        socks_request[3] = stoi(client[i].port)%256;
        socks_request[4] = 0;
        socks_request[5] = 0;
        socks_request[6] = 0;
        socks_request[7] = 1;
        socks_request[8] = 0;
        for(j = 0;j < client[i].host.length();j++){
          socks_request[9+j] = client[i].host[j];
        }
        socks_request[9+j] = 0;
        (*socket_[i]).async_send(boost::asio::buffer(socks_request, sizeof(unsigned char)*200),
          [this, i](boost::system::error_code err, size_t len) {
              if(!err) {
                  (*socket_[i]).async_read_some(boost::asio::buffer(socks_reply, 8), [this, i](boost::system::error_code err, size_t len) {
                      if(!err) {
                        if(socks_reply[1] == 90){
                          std::make_shared<session>(std::move(*socket_[i]),i,client[i].host, client[i].port,client[i].file)->start();
                        }
                        else{
                          return;
                        }
                      }else
                          cerr << err << "\n";
                  });
              }
          });
      }
    }
  private:
    tcp::socket *socket_[5];
    tcp::resolver resolve;
    unsigned char socks_reply[8];
};

int main(int argc, char* argv[]){
  try
	{
		if (argc != 4)
		{
			std::cerr << "Usage: console.cgi <host> <port> <file>\n";
			//return 1;
		}
    // string host = string(argv[1]);
    // string port = string(argv[2]);
    // string filename = string(argv[3]);
    // example: h0=&p0=&f0=&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&sh=&sp=
    print_header();
    string QUERY_STRING = getenv("QUERY_STRING");
    size_t pos = 0;
    std::string token;
    int index = 0;
    int i = 0;
    while ((pos = QUERY_STRING.find("&")) != std::string::npos) {
      size_t pos2 = 0;
      if((pos2 = QUERY_STRING.find("=")+1) != std::string::npos){
        token = QUERY_STRING.substr(pos2, pos-3);
        switch(index){
          case 0:
            client[i].host = token;
            index++;
            break;
          case 1:
            client[i].port = token;
            index++;
            break;
          case 2:
            client[i].file = token;
            index = 0;
            i++;
            break;
          default:
            std::cerr << "Exception: switch error " << "\n";
            break;
        }
        QUERY_STRING.erase(0, pos + string("&").length());
      }
    }
    size_t pos2 = 0;
    if((pos2 = QUERY_STRING.find("=")+1) != std::string::npos){
      token = QUERY_STRING.substr(pos2, QUERY_STRING.length());
      client[i].file = token;
      i++;
    }
    ///cerr << getenv("QUERY_STRING") << endl;
    //cerr << i << endl;
    for(int j = 0;j < 5;j++){
      if(client[j].host.length()!=0)
        add_row(j,client[j].host,client[j].port);
    }
    server server_obj;
    io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}