	#include <iostream>
	#include <cstring>
	#include <ifaddrs.h>
	#include <unistd.h>
	#include <netinet/in.h>
	#include <net/if.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>

	using namespace std;

	int main(int argc, char** argv){
		if(argc != 4) {
			cout << "Usage: ./executable ON_ip ON_port udp_port\n";
			return 1;
		}
		string ON_ip = argv[1];
		int ON_port = stoi(argv[2]);
		int udp_port = stoi(argv[3]);
		struct ifaddrs* my_addr;
		getifaddrs(&my_addr);
		in_addr my_ip = ((sockaddr_in*)my_addr->ifa_addr)->sin_addr;
		int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in server;
		server.sin_family = AF_INET;
		server.sin_port = htons(ON_port);

		inet_pton(AF_INET, ON_ip.c_str(), &server.sin_addr);
		connect(serv_sock,(sockaddr*)&server,sizeof(server));
		string msg = "CONNECT: " + ON_ip + " " + to_string(udp_port);
		send(serv_sock,msg.c_str(),msg.size(),0);
		char sbuffer[500] = {};
		recv(serv_sock, sbuffer, sizeof(sbuffer),0);

		///  process msg received from server

		int udp_sock = socket(AF_INET,SOCK_DGRAM,0);
		sockaddr_in own_addr;
		own_addr.sin_family = AF_INET;
		own_addr.sin_port = htons(udp_port);
		own_addr.sin_addr = my_ip;
		bind(udp_sock, (sockaddr_in*)&own_addr, sizeof(own_addr));
		char buffer[1000] = {};
		while(true){
			sockaddr_in sender;
			socklen_t sender_len = sizeof(sender);
			recvfrom(udp_sock, buffer,sizeof(buffer),0, (sockaddr*)&sender, &sender_len);

			/// process msg received from other nodes(LSPs)
			
		}
		close(serv_sock);
		close(udp_sock);

		return 0;
	}
