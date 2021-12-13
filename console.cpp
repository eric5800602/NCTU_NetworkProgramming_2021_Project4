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
client_info client[5];


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
        valid = false;
        return;
      }
      memset(data_,0,max_length);
      id = index;
      filename = "./test_case/"+f;
      file_input.open(filename,ios::in);
      all_msg = "";
      //if ( file_input.fail() ) cerr << "open fail" << endl;
      add_row(index,host,port);
		}
		void start()
		{
			do_read();
		}
    bool valid;

	private:
		void do_read()
		{
			auto self(shared_from_this());
      client_socket.async_read_some(boost::asio::buffer(data_, max_length),
          [this,self](boost::system::error_code ec, std::size_t length){
          if (!ec){
            all_msg += data_;
            memset(data_,0,length);
            size_t pos;
            if((pos = all_msg.find("%")) != string::npos){
              output_shell(id,all_msg);
              all_msg = "";
              do_write();
            }
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
        [this,self](boost::system::error_code ec, std::size_t /*length*/){
          if (!ec){
             /*
            io_context[id].stop();
            io_context[id].reset();
            cerr << "test3" << endl;
            return;*/
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
      for(int i = 0;i < 5;i++){
        if(client[i].host.length() !=0){
          tcp::resolver::query query(client[i].host, client[i].port);
          resolve.async_resolve(query,
          boost::bind(&server::connection, this,i,boost::asio::placeholders::error,boost::asio::placeholders::iterator ));
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
  private:
    tcp::socket *socket_[5];
    tcp::resolver resolve;
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
    // example: h0=nplinux3.cs.nctu.edu.tw&p0=9898&f0=t2.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=
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
        if(token.length() == 0)break;
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
    for(int j = 0;j < i;j++){
      cerr << "host: "<<client[j].host << endl;
      cerr << "port: "<<client[j].port << endl;
      cerr << "file: "<<client[j].file << endl;
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