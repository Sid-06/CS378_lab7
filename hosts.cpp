#include<bits/stdc++.h>
#include<ifaddrs.h>
#include<unistd.h>
#include<netinet/in.h>
#include<net/if.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<chrono>
#include<sys/select.h>
using namespace std;

unordered_map<char, int> currInfo;
char me = 0;
map<char, sockaddr_in> neighbours;
map<char, map<char, int>> graph;
map<char, uint32_t> sequences;
bool graphChanged = false;

vector<char> makeLSP(char origin){
    vector<char> msg;
    msg.push_back(origin);
    uint32_t sequence = htonl(sequences[origin]);
    char* p = (char*)&sequence;
    msg.insert(msg.end(), p, p+4);
    msg.push_back((char)(graph[origin].size() - graph[origin].count(origin))); // if origin is present, it will be 1, else 0
    for(auto entry : graph[origin]){
        if(entry.first == origin) continue;
        msg.push_back(entry.first);
        uint32_t cost = htonl(entry.second);
        p = (char*)&cost;
        msg.insert(msg.end(), p, p+4);
    }
    return msg;
}

void floodLSP(int udp_sock, char* buffer, int size, sockaddr_in* sender = NULL){
    for(auto entry : neighbours){
        sockaddr_in vn = entry.second;
        if(sender && vn.sin_addr.s_addr == sender->sin_addr.s_addr && vn.sin_port == sender->sin_port) continue;
        int sent;
        do{
            sent = sendto(udp_sock, buffer, size, 0, (sockaddr*)&vn, sizeof(vn));
        }while(sent < 0 && errno == EINTR);
        if(sent != size) perror("send LSP");
    }
}

void sendOwnLSP(int udp_sock){
    map<char, int> oldLinks = graph[me];
    graph[me].clear();
    for(auto entry : currInfo) graph[me][entry.first] = entry.second;
    if(graph[me] != oldLinks) graphChanged = true;
    sequences[me]++;
    vector<char> msg = makeLSP(me);
    floodLSP(udp_sock, msg.data(), msg.size());
}

void receiveLSP(int udp_sock){
    char buffer[1000] = {};
    sockaddr_in sender{};
    socklen_t sender_len = sizeof(sender);
    int received = recvfrom(udp_sock, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &sender_len);
    if(received < 0){
        if(errno != EINTR) perror("recv LSP");
        return;
    }
    if(received < 6) return;
    char origin = buffer[0];
    uint32_t sequence;
    memcpy(&sequence, buffer + 1, 4);
    sequence = ntohl(sequence);
    int count = (unsigned char)buffer[5];
    if(received != 6 + 5*count) return;
    if(sequences.count(origin) && sequence <= sequences[origin]) return;
    map<char, int> links;
    links[origin] = 0;
    for(int i=0;i<count;i++){
        char neigh =buffer[6 + 5*i];
        uint32_t cost;
        memcpy(&cost, buffer + 7 + 5*i, 4);
        cost = ntohl(cost);
        links[neigh] = cost;
    }
    sequences[origin] = sequence;
    if(!graph.count(origin) || graph[origin] != links) graphChanged = true;
    graph[origin] = links;
    floodLSP(udp_sock, buffer, received, &sender);
}

void printGraph(){  
    set<char> nodes;
    for(auto row : graph){
        nodes.insert(row.first);
        for(auto entry : row.second) nodes.insert(entry.first);
    }
    cout << "Graph at " << me << endl;
    cout << "   ";
    for(char node : nodes) cout << node << " ";
    cout << endl;
    for(char node : nodes){
        cout << node << ":";
        for(char dest : nodes){
            int cost = -1; // -1 = currently unknown/no link
            if(node == dest) cost = 0;
            else if(graph.count(node) && graph[node].count(dest)) cost = graph[node][dest];
            cout << " " << cost;
        }
        cout << endl;
    }
}

bool receiveState(char* rec_buffer, int size){
    unordered_map<char, int> nextInfo;
    map<char, sockaddr_in> nextNeighbours;
    for(int i=0;11*i<size;i++){
        if(rec_buffer[11*i] == '\0') break;
        char neigh; in_addr_t neigh_ip; uint16_t neigh_port = 0; uint32_t cost;
        memcpy(&neigh, &rec_buffer[11*i], 1);
        memcpy(&neigh_ip, &rec_buffer[11*i+1], 4);
        memcpy(&neigh_port, &rec_buffer[11*i+5], 2);
        memcpy(&cost, &rec_buffer[11*i+7], 4);
        neigh_port = ntohs(neigh_port);
        cost = ntohl(cost);
        nextInfo[neigh] = cost;
        if(cost == 0) me = neigh;
        else{
            sockaddr_in vn{};
            vn.sin_family = AF_INET;
            vn.sin_addr.s_addr = neigh_ip;
            vn.sin_port = htons(neigh_port);
            nextNeighbours[neigh] = vn;
        }
    }
    currInfo = nextInfo;
    neighbours = nextNeighbours;
    cout << me << "=0";
    for(auto entry : neighbours) cout << "," << entry.first << "=" << currInfo[entry.first];
    cout << endl;
    return true;
}

int main(int argc, char** argv){
    if(argc != 4) {
        cout << "Usage: ./executable ON_ip ON_port udp_port\n";
        return 1;
    }
    string ON_ip = argv[1];
    int ON_port = stoi(argv[2]);
    int udp_port = stoi(argv[3]);
    if(ON_port < 1 || ON_port > 65535 || udp_port < 1 || udp_port > 65535){
        cerr << "invalid port" << endl;
        return 1;
    }
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(serv_sock < 0){
        perror("socket");
        return 1;
    }
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(ON_port);
    inet_pton(AF_INET, ON_ip.c_str(), &server.sin_addr);
    if(connect(serv_sock,(sockaddr*)&server,sizeof(server)) < 0){
        perror("connect");
        close(serv_sock);
        return 1;
    }

    sockaddr_in own_addr{};
    socklen_t own_len = sizeof(own_addr);
    if(getsockname(serv_sock, (sockaddr*)&own_addr, &own_len) < 0){
        perror("getsockname");
        close(serv_sock);
        return 1;
    }
    own_addr.sin_port = htons(udp_port);
    int udp_sock = socket(AF_INET,SOCK_DGRAM,0);
    if(udp_sock < 0 || bind(udp_sock, (sockaddr*)&own_addr, sizeof(own_addr)) < 0){
        perror("bind UDP");
        close(serv_sock);
        if(udp_sock >= 0) close(udp_sock);
        return 1;
    }
    cout << "VN running at " << inet_ntoa(own_addr.sin_addr) << ":"
         << udp_port << " (UDP), ON at " << ON_ip << ":" << ON_port << endl;
    char msg[6];
    memcpy(msg, &own_addr.sin_addr, 4);
    memcpy(msg + 4, &own_addr.sin_port, 2);
    int sent = 0;
    while(sent < 6){
        int n = send(serv_sock,msg + sent,sizeof(msg) - sent, MSG_NOSIGNAL);
        if(n < 0 && errno == EINTR) continue;
        if(n <= 0){
            perror("send CONNECT");
            close(serv_sock);
            close(udp_sock);
            return 1;
        }
        sent += n;
    }
    // pending: TCP bytes received but not yet parsed as a complete
    // LINK-STATE message (TCP is a byte stream - one recv() may hold
    // several messages, half a message, or both, so leftovers wait here)
    vector<char> pending;
    auto nextLSP = chrono::steady_clock::now();        // when to next re-flood my own LSP periodically
    auto nextPrint = nextLSP + chrono::seconds(10);    // when to next print the graph periodically
    auto printAfter = nextLSP;                         // when the print (1s after last change) fires
    bool printPending = false;
    int result = 0;
    while(true){
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serv_sock, &readfds);
        FD_SET(udp_sock, &readfds);
        timeval timeout{1, 0};
        int ready = select(max(serv_sock, udp_sock) + 1, &readfds, NULL, NULL, &timeout);
        if(ready < 0){
            if(errno == EINTR) continue;
            perror("select");
            result = 1;
            break;
        }
        if(FD_ISSET(serv_sock, &readfds)){
            char rec_buffer[500] = {};
            int received = recv(serv_sock,rec_buffer,sizeof(rec_buffer),0);
            if(received < 0 && errno == EINTR) continue;
            if(received <= 0){
                if(received < 0) perror("recv LINK-STATE");
                else cerr << "ON connection closed" << endl;
                if(received < 0 || !pending.empty()) result = 1;
                break;
            }
            pending.insert(pending.end(), rec_buffer, rec_buffer + received);
            while(!pending.empty()){
                int length = 0;
                while(length < (int)pending.size() && length < 286 &&
                      pending[length] >= 'A' && pending[length] <= 'Z') length += 11; // 25*11 + 1(/0)=286
                if(length >= (int)pending.size()) break;
                if(length > 286 || pending[length] != '\0' ||
                   !receiveState(pending.data(), length + 1)){
                    cerr << "invalid LINK-STATE" << endl;
                    result = 1;
                    break;
                }
                pending.erase(pending.begin(), pending.begin() + length + 1);
                sendOwnLSP(udp_sock);
                // send the stored LSPs too, so a new neighbour learns the graph
                for(auto row : graph){
                    if(row.first == me) continue;
                    vector<char> lsp = makeLSP(row.first);
                    floodLSP(udp_sock, lsp.data(), lsp.size());
                }
                nextLSP = chrono::steady_clock::now() + chrono::seconds(10);
            }
            if(result != 0) break;
        }
        /// process msg received from other nodes(LSPs)
        if(FD_ISSET(udp_sock, &readfds)) receiveLSP(udp_sock);
        auto now = chrono::steady_clock::now();
        if(me != 0 && now >= nextLSP){
            sendOwnLSP(udp_sock);
            nextLSP = now + chrono::seconds(10);
        }
        // let updates from the same config change arrive before printing
        if(graphChanged){
            printPending = true;
            printAfter = now + chrono::seconds(1);
            graphChanged = false;
        }
        // Triggered Print
        if(me != 0 && printPending && now >= printAfter){
            printGraph();
            printPending = false;
            nextPrint = now + chrono::seconds(10);
        }
        // Periodic Print
        else if(me != 0 && now >= nextPrint){
            printGraph();
            nextPrint = now + chrono::seconds(10);
        }
    }
    close(serv_sock);
    close(udp_sock);
    return result;
}
