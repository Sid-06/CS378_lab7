#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <ifaddrs.h>
#include <net/if.h>

using namespace std;

struct VirtualNodes{
	in_addr_t ip;
	uint16_t port;
	int fd;
};

bool readConfig(const string& text, vector<vector<int>>& fullCosts){
	stringstream file(text);
	string line;
	vector<vector<int>> costs;
	while(getline(file, line)){
		stringstream s(line);
		s >> ws;
		if(s.eof() || s.peek() == '#') continue;
		vector<int> row;
		int x;
		while(true){
			s >> ws;
			if(s.eof()) break;
			if(!(s >> x)) return false;
			if(x != -1 && x <= 0) return false;
			row.push_back(x);
		}
		if(row.empty()) return false;
		costs.push_back(row);
	}
	if(costs.empty()) return false;
	int N = costs[0].size() + 1;
	if(N > 26 || (int)costs.size() != N-1) return false;
	vector<vector<int>> nextCosts(N, vector<int>(N, 0));
	for(int i=0;i<N-1;i++){
		if((int)costs[i].size() != N-i-1) return false;
		for(int j=i+1;j<N;j++){
			int c = costs[i][j-i-1];
			nextCosts[i][j] = c;
			nextCosts[j][i] = c;
		}
	}
	fullCosts = nextCosts;
	return true;
}

bool sendState(VirtualNodes& node, const vector<char>& msg){
	int sent = 0;
	while(sent < (int)msg.size()){
		int n = send(node.fd, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL);
		if(n < 0 && errno == EINTR) continue;
		if(n <= 0){
			close(node.fd);
			node.fd = -1;
			return false;
		}
		sent += n;
	}
	return true;
}

int main(int argc, char** argv){
	// take input
	string configPath;
	if(argc == 2) configPath = argv[1];
	else if(argc == 1) cin >> configPath;
	else{
		cerr << "Usage: ./ON config_file" << endl;
		return 1;
	}
	// read config file
	ifstream file(configPath);
	if(!file.is_open()){
		cerr << "file open error"<<endl;
		return 1;
	}
	string text((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	file.close();
	vector<vector<int>> fullCosts;
	if(!readConfig(text, fullCosts)){
		cerr << "invalid config file" << endl;
		return 1;
	}
	int N = fullCosts.size();

	//listen for cnxn form VNs on 5000 port
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5000);
	struct ifaddrs* my_addr;
	if(getifaddrs(&my_addr) < 0){
		perror("getifaddrs");
		return 1;
	}
	bool found = false;
	for(auto p=my_addr;p;p=p->ifa_next){
		if(p->ifa_addr && p->ifa_addr->sa_family == AF_INET &&
		   (p->ifa_flags & IFF_UP) && !(p->ifa_flags & IFF_LOOPBACK)){
			addr.sin_addr = ((sockaddr_in*)p->ifa_addr)->sin_addr;
			found = true;
			break;
		}
	}
	freeifaddrs(my_addr);
	if(!found){
		cerr << "no local IPv4 address" << endl;
		return 1;
	}
	int fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd < 0){
		perror("socket");
		return 1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	int b = bind(fd, (sockaddr*)&addr, sizeof(addr));
	if(b < 0 || listen(fd, N) < 0){
		perror("bind/listen");
		close(fd);
		return 1;
	}
	cout << "ON listening on " << inet_ntoa(addr.sin_addr) << ":5000" << endl;

	vector<VirtualNodes> nodes;
	bool changed = false;
	while(true){
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		timeval timeout{1, 0};
		int ready = select(fd + 1, &readfds, NULL, NULL, &timeout);
		if(ready < 0){
			if(errno == EINTR) continue;
			perror("select");
			break;
		}
		if(ready > 0){
			int client_fd = accept(fd, NULL, NULL);
			if(client_fd < 0){
				cerr << "connection error" << endl;
				continue;
			}
			if(nodes.size() == 26){
				close(client_fd);
				continue;
			}
			timeval wait{2, 0};
			setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
			setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
			char buffer[6];
			int received  = recv(client_fd, buffer, 6, MSG_WAITALL);
			if(received != 6){
				cerr << "incomplete CONNECT" << endl;
				close(client_fd);
				continue;
			}
			VirtualNodes vn;
			memcpy(&vn.ip, buffer, 4);
			uint16_t networkPort;
			memcpy(&networkPort, buffer + 4, 2);
			vn.port = ntohs(networkPort);
			vn.fd = client_fd;
			if(vn.port == 0 || vn.ip ==htonl(INADDR_ANY) ||
			   (ntohl(vn.ip) >> 24) == 127){
				cerr << "invalid CONNECT address" << endl;
				close(client_fd);
				continue;
			}
			nodes.push_back(vn);
			changed = true;
		}

		// check for changes even while waiting for all VNs
		ifstream updatedFile(configPath);
		if(updatedFile.is_open()){
			string nextText((istreambuf_iterator<char>(updatedFile)), istreambuf_iterator<char>());
			if(nextText != text){
				vector<vector<int>> nextCosts;
				if(readConfig(nextText, nextCosts) && (int)nextCosts.size() == N){
					fullCosts = nextCosts;
					changed = true;
				}
				else cerr << "invalid config update; keeping previous costs" << endl;
				text = nextText;
			}
		}
		if(!changed || (int)nodes.size() < N) continue;
		changed = false;

		for(int i=0;i<(int)nodes.size();i++){
			if(nodes[i].fd < 0) continue;
			sockaddr_in vn;
			vn.sin_family = AF_INET;
			vn.sin_port = htons(nodes[i].port);
			vn.sin_addr.s_addr = nodes[i].ip;
			vector<char> msg;
			char base = 'A' + i;
			in_addr_t ip = nodes[i].ip;
			uint16_t port = vn.sin_port;
			int cost = htonl(0);
			msg.push_back(base);
			auto append = [&](const void* data, size_t size){
				const char* p = static_cast<const char*>(data);
				msg.insert(msg.end(), p, p + size);
			};
			append(&ip, 4);
			append(&port, 2);
			append(&cost, 4);
			for(int j=0;i<N && j<N;j++){
				if(j==i || fullCosts[i][j] < 0) continue;
				char alphab = 'A' + j;
				in_addr_t ip = nodes[j].ip;
				uint16_t port = htons(nodes[j].port);
				int cost = htonl(fullCosts[i][j]);
				msg.push_back(alphab);
				append(&ip, 4);
				append(&port, 2);
				append(&cost, 4);
			}
			msg.push_back('\0');
			sendState(nodes[i], msg);
		}
	}
	for(auto vn : nodes) if(vn.fd >= 0) close(vn.fd);
	close(fd);
	return 0;
}
