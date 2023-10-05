#ifndef _SERVER_H_
#define _SERVER_H_

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

struct File {
	int file_descriptor;
	string file_path;
	int recursive_depth;

	File() {
		this->file_descriptor = -1;
		this->recursive_depth = 1;

	}

	File(string path, int depth) {
		this->file_path = path;
		this->recursive_depth = depth;
		this->file_descriptor = -1;
	}
};




#define id pair<string, int>


class Server {

	private:

		string logfile;

		bool recursive;
		bool everything;
		bool has_logfile;

		string address;
		
		sockaddr_in addr;

		int port;
		int sockfd;

		ofstream output;
		ofstream error;

		fd_set read_fds;
		fd_set write_fds;

		vector<pair<id,int> > clients;
		vector<pair<File,int> > in_transit;
		queue<string> pages;
		queue<pair<id,int> > available_clients;

		string host_name;
		string home_page;


	public:
		Server(bool recursive, bool everything, string logfile, int port) {
			this->recursive = recursive;
			this->everything = everything;
			this->logfile = logfile;
			this->port = port;
			this->has_logfile = !logfile.empty();
		}
		~Server() {
			output.close();
			error.close();


			close(sockfd);
		}

		void init();
		void run();
		void print(string message);
		void print_err(string err);

		void add_client();
		void remove_client(int sock);
		
		void process_keyboard_command();
		void process_client_answer(int sock);
		
		void check_url(string url);

		void open_file(int sock);
		void write_to_file(int fd, char* buf);



		void send_page_request();
		void enqueue_page(string page);


};


#endif