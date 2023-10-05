#ifndef _AUXILIARY_H_
#define _AUXILIARY_H_

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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <sstream>

#define BUFLEN 512

using namespace std;



using namespace std;

vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
    	if (!item.empty()) 
    		elems.push_back(item);
    }
    return elems;
}


vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}


int send_n_bytes(int sock, char* buf, int n) {
    int count = 0;
    int written_bytes = 0;
    while (count < n) {
        written_bytes = write(sock,buf+count,n-count);
        if (written_bytes <= 0)
            return -1;
        count += written_bytes;
    }
    return count;
}

int read_n_bytes(int sock, char* buf, int n) {
    int count = 0;
    int read_bytes = 0;
    while (count < n) {
        read_bytes = read(sock,buf+count,n-count);
        if (read_bytes <= 0)
            return -1;
        n+= read_bytes;
    }
    return count;
}

ssize_t read_line(int sockd, void *vptr, size_t maxlen) {
    ssize_t n, rc;
    char    c, *buffer;

    buffer = (char*) vptr;

    for ( n = 1; n < (ssize_t)maxlen; n++ ) {    
    if ( (rc = read(sockd, &c, 1)) == 1 ) {
        if ( c == '\n' )
        break;
        *buffer++ = c;
    }
    else if ( rc == 0 ) {
        if ( n == 1 )
        return 0;
        else
        break;
    }
    else {
        if ( errno == EINTR )
        continue;
        return -1;
    }
    }

    *buffer = 0;
    return n;
}

int send_line(int sock, const char* line) {
    if (line[0] == '\0' || line[0] == '\n')
        return 0;
    char* buf = new char[BUFLEN-1];
    memset(buf,0,BUFLEN-1);
    strcpy(buf,line);
    strcat(buf,"\n");

    return send_n_bytes(sock,buf,strlen(buf));
}
int send_line(int sock, string line) {
    return send_line(sock,line.c_str());
}

#endif