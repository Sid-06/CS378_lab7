#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

struct VirtualNodes{
	in_addr_t ip;
	int port;
	int fd;
};

int main(){

	// take input
	string configPath;
	cin>>configPath;
	// read config file
	ifstream file(configPath);
	if(!file.is_open()){
		cerr<<"file open error"<<endl;
		return 0;
	}
	string line;
	vector<vector<int>>costs;
	while(getline(file , line)){

		if(line.empty()){continue;}
		stringstream s(line);
		vector<int>row;
		int x;
		while(s >> x){
			row.push_back(x);
		}
		costs.push_back(row);
	}
	int virtualNum = costs[0].size()+1;

	//listen for cnxn form VNs on 5000 port
	sockaddr_in addr{};
	addr.sin_port = htons(5000);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_pton();
	int fd = socket(AF_INET,SOCK_STREAM,0);
	int b = bind(fd,(sockaddr *)&addr , sizeof(addr));
	listen(fd , virtualNum);
	
	
	vector<VirtualNodes> nodes;
	while(nodes.size() < virtualNum)	{
		int client_fd = accept(fd , NULL , NULL);
		if(client_fd < 0){
			cerr<<"connection error"<<endl;
			continue;
		}
		char buffer[6];
		int received  = recv(client_fd, buffer, 6, MSG_WAITALL);
		VirtualNodes vn;
		memcpy(&vn.ip , buffer , 4);
		uint16_t networkPort;
		memcpy(&networkPort , buffer + 4,2);
		vn.port = ntohs(networkPort);
		vn.fd = client_fd;
		nodes.push_back(vn);			
 	}

	//link state protocols

	
}

