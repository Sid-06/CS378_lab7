import select
import socket
import struct
import sys
import time
from pathlib import Path

ON_PORT = 5000

class VirtualNodes:
    def __init__(self, ip, port, fd):
        self.ip = ip
        self.port = port
        self.fd = fd

def sendState(node, msg):
    try:
        node.fd.sendall(msg)
    except OSError:
        node.fd.close()
        node.fd = None
        return False
    return True 

def readConfig(txt):
    costs = []
    for line in txt.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        row = []
        for value in line.split():
            x = int(value)
            row.append(x)
        costs.append(row)

    if not costs:
        return None
    N = len(costs[0])+1
    full_costs = [[0]*N for _ in range(N)]
    for i in range(N-1):
        for j in range(i+1,N):
            c = costs[i][j-i-1]
            full_costs[i][j]=c
            full_costs[j][i]=c
    return full_costs

def get_ip():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.connect(("8.8.8.8" , 80))
        return s.getsockname()[0]
    
def receiveConnect(fd):
    buffer = bytearray()
    deadline = time.monotonic() + 2
    while len(buffer) < 6:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('incomplete CONNECT')
        fd.settimeout(remaining)
        data = fd.recv(6 - len(buffer))
        if not data:
            raise ConnectionError('incomplete CONNECT')
        buffer.extend(data)
    fd.settimeout(2)
    return buffer


def main():
    #config file read
    config_path = sys.argv[1]
    txt = Path(config_path).read_text()
    fullCosts = readConfig(txt)
    N = len(fullCosts)
    ip = get_ip()

    nodes = []

    fd = socket.socket(family=socket.AF_INET,type = socket.SOCK_STREAM)
    fd.setsockopt(socket.SOL_SOCKET , socket.SO_REUSEADDR,1)
    fd.bind((ip,ON_PORT))
    fd.listen(N)
    print(f'ON listening on {ip}:{ON_PORT}', flush=True)

    changed = False
    try:
        while True:
            ready ,_,_ = select.select([fd] , [] , [] , 1)
            if ready:
                client_fd,_ = fd.accept()
                if len(nodes)>=26:
                    client_fd.close()
                    continue
                try:
                    buffer = receiveConnect(client_fd)
                except OSError as error:
                    print(f'incomplete CONNECT: {error}', file=sys.stderr)
                    client_fd.close()
                    continue
                vn_ip,port = struct.unpack('!4sH' , buffer)
                vn = VirtualNodes(vn_ip, port, client_fd)
                nodes.append(vn)
                changed = True

            try:
                next_txt = Path(config_path).read_text()
            except (OSError, UnicodeError):
                    next_txt = txt
            if next_txt != txt:
                    next_costs = readConfig(next_txt)
                    if next_costs is not None and len(next_costs) == N:
                        fullCosts = next_costs
                        changed = True
                    txt = next_txt
            if not changed or len(nodes) < N:
                continue
            changed = False

            for i, node in enumerate(nodes):
                    if node.fd is None:
                        continue
                    base = bytes([ord('A') + i])
                    msg = bytearray(struct.pack('!c4sHI', base, node.ip, node.port, 0))
                    if i < N:
                        for j in range(N):
                            if j == i or fullCosts[i][j] < 0:
                                continue
                            alphab = bytes([ord('A') + j])
                            vn = nodes[j]
                            msg.extend(struct.pack('!c4sHI', alphab, vn.ip, vn.port, fullCosts[i][j]))
                    msg.append(0)
                    sendState(node, msg)
    except OSError as error:
        print(error, file=sys.stderr)
        return 1
    
    finally:
        for vn in nodes:
            if vn.fd is not None:
                vn.fd.close()
        if fd is not None:
            fd.close()

if __name__ == '__main__':
    sys.exit(main())
