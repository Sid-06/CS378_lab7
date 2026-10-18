#include <iostream>
#include <cstring>
#include <ifaddrs.h>
#include <unistd.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<map>
using namespace std;

unordered_map<char, int> currInfo;
char me;

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

	sockaddr_in own_addr;
    own_addr.sin_family = AF_INET;
    own_addr.sin_port = htons(udp_port);
    own_addr.sin_addr = my_ip;

    connect(serv_sock,(sockaddr*)&server,sizeof(server));
    char msg[6];
	memcpy(msg, &own_addr.sin_addr, 4);
	memcpy(msg + 4, &own_addr.sin_port, 4);
    send(serv_sock,msg.c_str(),msg.size(),0);
    char rec_buffer[500] = {};
    recv(serv_sock,rec_buffer,sizeof(rec_buffer),0);

    ///  process msg received from server
	for(int i=0;i<26;i++){
		if(rec_buffer[11*i] == '\0') break;
		char neigh; in_addr_t neigh_ip; int neigh_port; int cost;
		memcpy(&neigh, &rec_buffer[11*i], 1);
		memcpy(&neigh_ip, &rec_buffer[11*i+1], 4);
		memcpy(&neigh_port, &rec_buffer[11*i+5], 2);
		memcpy(&cost, &rec_buffer[11*i+7], 4);
		neigh_port = ntohs(neigh_port);
		cost = ntohl(cost);
		currInfo[neigh] = cost;
		cout << neigh << "=" << cost << " ";
	}
	cout << endl;

    int udp_sock = socket(AF_INET,SOCK_DGRAM,0);
    bind(udp_sock, (sockaddr*)&own_addr, sizeof(own_addr));
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