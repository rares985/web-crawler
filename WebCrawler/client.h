#ifndef _CLIENT_H_
#define _CLIENT_H_


#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <queue>


using namespace std;


class Client {

	private:


		bool recursive;
		bool everything;
		bool has_logfile;

		int server_sock;
		int http_sock;

		ofstream output;
		ofstream error;

		string logfile;
		string server_address;

		int server_port;

		sockaddr_in addr;
		sockaddr_in http_addr;

		string host_name;



	public:

		Client(string logfile, string server_address, int server_port) {
			this->logfile = logfile;
			this->server_address = server_address;
			this->server_port = server_port;
			this->recursive = false;
			this->everything = false;
			this->has_logfile = !logfile.empty();
		}
		~Client() {
			output.close();
			error.close();

			close(server_sock);
		}

		void print(string message);
		void print_err(string err);
		
		void init();
		void run();
		void parse_command(char* command);


		void download_page(string page);
		void connect_to_http_server();
	
};

#endif