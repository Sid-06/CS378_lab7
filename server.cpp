#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main(){
	char buffer[500] = {};
	sockaddr_in addr{};
	addr.sin_port = htons(5000);
	addr.sin_addr.s_addr = INADDR_ANY;
	int fd = socket(PF_INET,SOCK_STREAM,0);
	int b = bind(fd , (sockaddr *)&addr , sizeof(addr));
	listen(fd , 10 );
	int client_fd = accept(fd , NULL ,NULL);
	recv(client_fd , buffer , sizeof(buffer) , 0);
	send(client_fd ,   "hello" , 5,0);
	close(client_fd);
	close(fd);
}
