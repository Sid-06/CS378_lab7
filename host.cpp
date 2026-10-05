#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;
int main(){
	int sock = socket(AF_INET ,SOCK_STREAM , 0);
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(5000);
	connect(sock , (sockaddr*)&server , sizeof(server));
	string msg = "helloback";
	send(sock , msg.c_str(),msg.size(),0);
	char buffer[500] = {};
	recv(sock , buffer , sizeof(buffer),0);
	cout<<"SERver "<<buffer<<endl;
	close(sock);
}
