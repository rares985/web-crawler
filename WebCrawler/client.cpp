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

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <queue>


#include "client.h"
#include "auxiliary.h"
#include "constants.h"


using namespace std;




void Client::parse_command(char* command) {
	string com(command);
	vector<string> tokens = split(com,' ');

    if (strstr(command,"CONFIG") != NULL) { /* If the line is the configuration line */
        if (strstr(command,"RECURSIVE") != NULL) {
            recursive = true;
            print("Recursive mode on ");
        }
        if (strstr(command, "EVERYTHING") != NULL) {
            everything = true;
            print("Everything mode on");
        }
        print("Configured");
    }

    if (strstr(command, "EXIT") != NULL) { /* If the line was exit */
        close(server_sock); /* close the socket */
        exit(-1); /* and terminate the program */
    }
    if (strstr(command, "GET") != NULL) { /* If a request is received */
    	string host, page,link;
    	link = tokens[1];
    	this->host_name = link.substr(7,link.find_first_of("/",7)-7); /* set the host name */
    	if (link.find_first_of("/",7) == string::npos) /*if the link is a host name, we add the / */
    		page = "/"; 
    	else 
    		page = link.substr(link.find_first_of("/",7));
    	download_page(page); /* and download the page */
    }
}



void Client::run() { /* Run the client continuously */
    while(1) {
        char* buffer = new char[BUFLEN];
        memset(buffer,0,BUFLEN);
        int res = read_line(server_sock,buffer,BUFLEN); 
        if (res <= 0) {
            continue;
        }
        parse_command(buffer);
    }
}


void Client::connect_to_http_server() {
    hostent *http_host;

    http_host = gethostbyname(host_name.c_str());
    if (http_host == NULL) {
        print_err("Error: Could not get host details");
        return;
    }

    http_addr.sin_family = AF_INET;
    http_addr.sin_port = htons(80);
    http_addr.sin_addr = *((in_addr*)http_host->h_addr);

    http_sock = socket(AF_INET,SOCK_STREAM,0);
    if (http_sock < 0) {
        print_err("Error: " + string(strerror(errno)));
        close(http_sock);
        return;
    }

    int res;
    res = connect(http_sock, (sockaddr*)&http_addr, sizeof(http_addr));
    if (res < 0) {
        print_err("Error: " + string(strerror(errno)));
        close(http_sock);
        return;
    }

}

void Client::download_page(string page) {
    int res = 0;
    char* buf = new char[BUFLEN];

    
    connect_to_http_server(); /* connect to the http server */
    
    string http_command = "GET " + page + " HTTP/1.1";
    
    if (!host_name.empty()) {
        http_command += "\nHost:";
        http_command += host_name;
        http_command += "\r\n\r\n";
    }
    
    print("Downloading " + page + "...");
    
    //res = send_line(http_sock, http_command);
    res = send(http_sock, http_command.c_str(), http_command.length(),0);
    if (res <= 0 ) {
        print_err("Error: Could not send http request");
        res = send_line(server_sock,"ERR SND");
        if (res <= 0) {
            print_err("Error: Could not report to server");
        }
        close(http_sock);
        return;
    }
    
    
    res = read_line(http_sock, buf,BUFLEN); /* receive the http server's answer */

    if (res <= 0) {
        print_err("Error reading from http server");
        res = send_line(server_sock,"ERROR READ");
        if (res <= 0) {
            print_err("Error reporting to server");
        }
        close(http_sock);
        return;
    }

    if (strstr(buf,"200") == NULL) { /* If success is not received */
        print_err("Error: HTTP Request unsuccessful");
        close(http_sock);
        res = send_line(server_sock, "ERROR REQUEST"); /* Send the answer to the server */
        if (res <= 0 )  {
            print_err("Error: Could not report to server");
        }
        return;
    }
    else { /* If success is received */
        res = send_line(server_sock,"SUCCESS"); /* Send it to the server */
        if (res <= 0) {
            print_err("Error: Could not report to server");
            return;
        }
        /*read the rest of the header */
        int bytes_read = 0;
        do {
            bytes_read = read_line(http_sock,buf,BUFLEN);
        } while (bytes_read > 2);
    }
    /*discard it */
    /* and start to read the file */
    char* sendbuf = new char[BUFLEN];
    while ((res = read_line(http_sock,buf, BUFLEN)) > 0) {
        memset(sendbuf,0,BUFLEN);
        strcpy(sendbuf,buf);
        res = send_line(server_sock,sendbuf);
        if (res < 0) {
            print("ERR: " + string(strerror(errno)));
        }
    }
    string end = "FINISH " + page;
    print("Sending " + end);
    if (send_line(server_sock,end) < 0) {
        print_err("Error: Could not send FINISH to server");
    }
    print("Download successful"); 
}

/* Initialise client */
void Client::init() {
    int res = 0;
    char* config_buffer = new char[BUFLEN];

    /* open log file, if there was one specified */
    if (has_logfile) {
        output.open(logfile+to_string(getpid()) + ".stdout");
        error.open(logfile+to_string(getpid()) + ".stderr");
    }

    /* create a new socket for the connection */
    server_sock = socket(AF_INET,SOCK_STREAM,0);
    if (server_sock < 0) {
        print_err("SOCKET: " + string(strerror(errno)));
        return;
    }

    /* Create the address on which the server will connect */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);

    /* convert the address from string to binary */
    res = inet_pton(AF_INET,server_address.c_str(), &addr.sin_addr);
    if (res <= 0) {
        print_err("INET_PTON: Invalid address");
        return;
    }

    /* Connect to the server */
    res = connect(server_sock,(sockaddr*)&addr,sizeof(addr));
    if (res < 0) {
        print_err("CONNECT: " + string(strerror(errno)));
    }

    /* Wait for the configuration message */
    res = read_line(server_sock,config_buffer,BUFLEN);
    if (res <= 0) {
        print_err("SERVER: Closed connection before CONFIG");
    }

    /*and configure accordingly */
    parse_command(config_buffer);
}



/* Prints to the stdout of the server */
void Client::print(string message) {
    if(has_logfile)
        output << message << "\n";
    else
        cout << message << "\n";
}
/* Prints to the stderr of the server */
void Client::print_err(string err) {
    if(has_logfile) 
        error << err << "\n";
    else
        cerr << err << "\n";
}

/*Shows the correct usage of the program */
void show_usage(char* name) {
    fprintf(stderr,"Usage: %s [-o <log_file>] -a <server_address> -p <port>\n",name);
    exit(-1);
}

int main(int argc, char** argv) {

    char* log_file = new char[BUFLEN];
    char* server_address = new char[BUFLEN];
    int port_no = 0;
    int c;
    bool has_address = false;
    bool has_port = false;
    bool valid = false;

    /*Check for mandatory arguments */
    for(int i = 1; i < argc; i ++) {
        if (strncmp(argv[i],"-a",2) == 0) {
            has_address = true;
        }
        if (strncmp(argv[i],"-p",2) == 0) {
            has_port = true;
        }
        if (has_address && has_port) {
            valid = true;
            break;
        }
    }

    opterr = 0; /* supress the error message */

    /* Parse the command-line arguments */
    while ((c = getopt(argc,argv,"o:a:p:")) != -1) {
        if (!valid) 
            show_usage(argv[0]);
        switch(c) {
            case 'o':
                strcpy(log_file,optarg);
                break;
            case 'a':
                strcpy(server_address,optarg);
                break;
            case 'p':
                port_no = atoi(optarg);
                break;
            case '?':
                show_usage(argv[0]);
                break;
            default:
                abort();
        }
    }

    /* Instantiate, initialise and run the client */
    Client client(log_file,server_address,port_no);
    client.init();
    client.run();

    return 0;
}

