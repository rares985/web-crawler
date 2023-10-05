#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <queue>


#include "server.h"
#include "auxiliary.h"
#include "constants.h"

#define MIN_CLIENTS 5
#define MAX_CLIENTS 10

using namespace std;

#define id pair<string, int>


/* Initialises the server */
void Server::init() {

	/* If a log file was specified, open it */
	if (has_logfile) {
		output.open(logfile+".stdout");
		error.open(logfile+".stderr");
	}

	/* create a new socket for clients to connect */
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0) {
		print_err("Error: " + string(strerror(errno)));
		return;
	}
	
	/* Set the address accordingly */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	/* bind the socket to the specified address */
	if (bind(sockfd,(sockaddr*)&addr, sizeof(addr)) < 0) {
		print_err("Error: " + string(strerror(errno)));
		return;
	}

	/* Set the socket as inactive and wait for clients to connect */
	listen(sockfd,MAX_CLIENTS);

	/* Empty the fd sets */
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	FD_SET(sockfd,&read_fds); /* the listening socket */
	FD_SET(0,&read_fds);	/* keyboard socket */

}

/* starts the server and runs it continuously */
void Server::run() {
	fd_set temp_read, temp_write;


	while(1) {
		temp_read = read_fds;
		temp_write = write_fds;
		int res;
		/* if there are not enough clients or there is nothing to download ,
		    we can only read from keyb or wait for the pages which are being
		    downloaded to finish */
		if (clients.size() > MIN_CLIENTS && !pages.empty()) {
			res = select(FD_SETSIZE,&temp_read,&temp_write,NULL,NULL);
		}
		else {
			res = select(FD_SETSIZE,&temp_read,NULL,NULL,NULL);
		}

		if (res  == -1) {
			print_err("Error: " + string(strerror(errno)));
			return;
		}


		if (FD_ISSET(sockfd,&temp_read)) { /* a new client */
			add_client();
		}

		if(FD_ISSET(0,&temp_read)) { /* keyboard input */
			process_keyboard_command();
		}

		for(int i = 0; i <(int) clients.size(); i ++) { 
			if(FD_ISSET(clients[i].second,&temp_read)) { /* a client is sending sth */
				process_client_answer(clients[i].second);
			}
		}
		/* If there are pages left to download and there are at least min-clients connected,
		start the download */
		if (clients.size() >= MIN_CLIENTS && !pages.empty() && !available_clients.empty())
			send_page_request();
	}
}

void Server::remove_client(int sock) {
	id cl_id;
	vector<pair<id,int> >::iterator it;
	close(sock);
	for(it = clients.begin(); it != clients.end(); ++ it) {
		if (it->second == sock) {
			cl_id = make_pair((it->first).first,(it->first).second);
			break;
		}
	}
	print_err("Error: Client " + cl_id.first + " on port " + to_string(cl_id.second) + " has closed the connection");
	

	FD_CLR(sock,&write_fds);
	FD_CLR(sock,&read_fds);

	clients.erase(it);
}

void Server::process_keyboard_command() {
	string buffer;
	vector<string> tokens;

	getline(cin, buffer);
	tokens = split(buffer,' ');
	if(buffer == "\n" || buffer == "")
		return;
	if (tokens[0] == "status") {
		string status = "\nConnected clients: \n";
		for(int i = 0; i < (int) clients.size(); i ++) {
			status += "Socket: ";
			status += to_string(clients[i].second)	;
			status += ", ";
			status += "Address: ";
			status += clients[i].first.first;
			status += ", ";
			status += "Port: ";
			status += to_string(clients[i].first.second);
			status += "\n";
		}
		print(status);
	}
	if (tokens[0] == "download") {
		if (host_name.empty()) {
			check_url(tokens[1]);
		}
		else {
			print_err("Error: Already downloading from " + host_name);
		}
	}
	if (tokens[0] == "exit") {
		for(auto it = clients.begin(); it != clients.end(); ++it) {
			send_line(it->second,"EXIT");
			close(it->second);
		};
		close(sockfd);
		exit(0);
	}
}


void Server::check_url(string url) {
	string host_name;
	size_t host_endpos;
	if (strstr(url.c_str(),"http://")  == NULL) {
		print_err("URL must begin with http://");
		return;
	}
	host_endpos = url.find_first_of("/",7); /* find the end of hostname */
	host_name = url.substr(7,host_endpos-7); /*extract host name */
	if (host_endpos != string::npos) { /* if the hostname  ends */
		home_page = url.substr(host_endpos); /*extract the first page we have to download */
	}
	else {
		home_page = "/"; /*we need to add index.html */
	}
	if (home_page[home_page.length()-1] == '/') {
		home_page += "/index.html";
	}
	this->host_name = host_name; /* set the host name */
	enqueue_page(home_page);
}
  
void Server::send_page_request() {
	int res = 0;
	int client_sock = available_clients.front().second;
	string page_to_download = pages.front();

	pages.pop();
	available_clients.pop();

	string command = "GET ";
	command += "http://";
	command += host_name;
	command += page_to_download;
	res = send_line(client_sock,command);
	if (res <= 0) {
		remove_client(client_sock);
		return;
	}
	pair<File,int> to_download = make_pair(File(page_to_download,1),client_sock);
	in_transit.push_back(to_download);

	FD_SET(client_sock,&read_fds);
	FD_CLR(client_sock,&write_fds);
}

void Server::process_client_answer(int sock) { /* primeste de la clientul de pe sock */
	char* buf = new char[BUFLEN];
	int res = read_line(sock, buf,BUFLEN);
	if (res <= 0) {
		remove_client(sock);
	}
	if (strstr(buf,"SUCCESS") != NULL) {
		print("Request successful. Starting download");
		open_file(sock);
	}
	else if (strstr(buf, "ERROR") != NULL) {
		if (strstr(buf,"READ") != NULL) {
			print_err("Error: Client could not read from http server");
		}
		else if (strstr(buf,"SEND")!= NULL) {
			print_err("Error: Client could not send http request");
		}
		else if (strstr(buf,"REQUEST")!= NULL) {
			print_err("Error: Request unsuccessful.");
		}
		remove_client(sock);
		host_name.clear();
	}
	else if (strstr(buf, "FINISH") != NULL) {
		vector<string> tokens = split(buf,' ');
		vector<pair<File,int> >::iterator it;
		for(it = in_transit.begin(); it != in_transit.end(); ++ it) {
			if ((it->first).file_path == tokens[1]) {
				break;
			}
		}
		close((it->first).file_descriptor);
		for(auto it = clients.begin(); it != clients.end(); ++ it) {
			if (it->second == sock) {
				available_clients.push(*it);
				break;
			}
		}
		host_name.clear();
	}
	else {
		vector<pair<File,int> >::iterator it;
		for(it = in_transit.begin(); it!= in_transit.end(); ++it) {
			if (it->second == sock) {
				break;
			}
		}
		write_to_file((it->first).file_descriptor, buf);
	}	
}


void Server::open_file(int sock) {
	vector<pair<File,int> >::iterator it;
	struct stat st = {0};

	for(it = in_transit.begin(); it != in_transit.end(); ++it) {
		if (it->second == sock) {
			break;
		}
	}

	size_t res_start = (it->first).file_path.find_last_of("/");
	string resource_name = (it->first).file_path.substr(res_start+1);
	string dir_hierarchy = (it->first).file_path.substr(0,res_start);
	
	if (stat(dir_hierarchy.c_str(),&st) == -1) { /* If folder hierarchy exists already */
		if (system(("mkdir -p " + host_name + dir_hierarchy).c_str()) < 0) {
			print_err("Error: Could not create folder hierarchy");
			return;
		}
	}
	print("Downloading " + resource_name);

	string path = host_name + dir_hierarchy;
	path += "/";
	path += resource_name;

	int fd = open(path.c_str(),O_WRONLY | O_TRUNC | O_CREAT,
	S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		print_err("Error opening file: " + string(strerror(errno)));
	}
	(it->first).file_descriptor = fd;
}

void Server::enqueue_page(string page) {
	pages.push(page);
}

void Server::write_to_file(int fd, char* buf) {
	int res = 0;
	res = write(fd, buf, BUFLEN);
	if (res < 0) {
		print_err("Error writing to file: " + string(strerror(errno)));
	}
}


void Server::add_client() {
	int res = 0;
	sockaddr_in client_addr;
	socklen_t clilen = sizeof(client_addr);
	int client_sock;

	client_sock = accept(sockfd,(sockaddr*)&client_addr, &clilen);

	if (client_sock < 0) {
		print_err("Error:  " + string(strerror(errno)));
	}

	if (clients.size() >= MAX_CLIENTS) {
		close(client_sock);
		return;
	}

	string client_data = string(inet_ntoa(client_addr.sin_addr)) + " : " + to_string(ntohs(client_addr.sin_port));
	
	print("Connection from " + client_data);

	string addr(inet_ntoa(client_addr.sin_addr));
	int port = ntohs(client_addr.sin_port);

	id new_id = make_pair(addr,port);

	pair<id,int> new_client = make_pair(new_id, client_sock);

	clients.push_back(new_client);
	available_clients.push(new_client);

	FD_SET(client_sock, &read_fds);  
	FD_SET(client_sock, &write_fds); 

	/* configure client */

	string config_message = "CONFIG";
	if (recursive) 
		config_message += " RECURSIVE";
	if (everything) 
		config_message += " EVERYTHING";

	res = send_line(client_sock,config_message);
	if (res <= 0) {
		print_err("Error sending config line");
		remove_client(client_sock);
		return;
	}
}

void Server::print(string message) {
	if (has_logfile)
		output << message << "\n";
	else 
		cout << message << "\n";
}

void Server::print_err(string err) {
	if (has_logfile)
		error << err << "\n";
	else
		cerr << err << "\n";
}

void show_usage(char* name) {
	fprintf(stderr, "Usage: %s [-r] [-e] [-o <logfile>] -p <port>\n",name);
	exit(-1);
}

int main(int argc, char** argv) {
	int port = 0;
	int eflag = 0;
	int rflag = 0;
	char* logfile = new char[BUFLEN];
	int c;
	bool valid = false;

	opterr = 0;
	for(int i = 1; i < argc; i++) {
		if (strncmp(argv[i],"-p",2) == 0) {
			valid = true;
			break;
		}
	}

	while((c = getopt(argc,argv,"reo:p:")) != -1) {
		if (!valid)
			show_usage(argv[0]);
		switch(c) {
			case 'r':
				rflag = 1;
				break;
			case'e':
				eflag = 1;
				break;
			case 'o':
				strcpy(logfile,optarg);
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case '?':
				if (optopt == 'o' || optopt == 'p') {
					show_usage(argv[0]);
				}
				else {
					show_usage(argv[0]);
				}
			default:
				abort();
		}
	}

	Server server(rflag,eflag,logfile,port);
	server.init();
	server.run();
	return 0;
}
