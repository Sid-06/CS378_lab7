import fcntl
import re
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


def readConfig(text):
    costs = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        row = []
        for value in line.split():
            if not re.fullmatch(r'[+-]?[0-9]+', value):
                return None
            try:
                x = int(value)
            except ValueError:
                return None
            if x != -1 and not 0 < x <= 2147483647:
                return None
            row.append(x)
        costs.append(row)

    if not costs:
        return None
    N = len(costs[0]) + 1
    if N > 26 or len(costs) != N - 1:
        return None
    fullCosts = [[0] * N for _ in range(N)]
    for i in range(N - 1):
        if len(costs[i]) != N - i - 1:
            return None
        for j in range(i + 1, N):
            c = costs[i][j - i - 1]
            fullCosts[i][j] = c
            fullCosts[j][i] = c
    return fullCosts


def sendState(node, msg):
    try:
        node.fd.sendall(msg)
    except OSError:
        node.fd.close()
        node.fd = None
        return False
    return True


def getLocalIP():
    # Linux interface flags and IPv4 address, like getifaddrs in the C++ code
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as fd:
        for index, name in socket.if_nameindex():
            request = struct.pack('256s', name.encode()[:15])
            try:
                info = fcntl.ioctl(fd.fileno(), 0x8913, request)  # SIOCGIFFLAGS
                flags = struct.unpack_from('H', info, 16)[0]
                if not flags & 0x1 or flags & 0x8:  # IFF_UP, IFF_LOOPBACK
                    continue
                info = fcntl.ioctl(fd.fileno(), 0x8915, request)  # SIOCGIFADDR
                return socket.inet_ntoa(info[20:24])
            except OSError:
                continue
    raise OSError('no local IPv4 address')


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
    # take input
    if len(sys.argv) == 2:
        configPath = sys.argv[1]
    elif len(sys.argv) == 1:
        try:
            configPath = input().strip()
        except EOFError:
            print('file open error', file=sys.stderr)
            return 1
    else:
        print('Usage: python3 ON.py config_file', file=sys.stderr)
        return 1

    # read config file
    try:
        text = Path(configPath).read_text()
    except (OSError, UnicodeError):
        print('file open error', file=sys.stderr)
        return 1
    fullCosts = readConfig(text)
    if fullCosts is None:
        print('invalid config file', file=sys.stderr)
        return 1
    N = len(fullCosts)

    nodes = []
    fd = None
    try:
        # listen for cnxn from VNs on port 5000
        ip = getLocalIP()
        fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        fd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        fd.bind((ip, ON_PORT))
        fd.listen(N)
        print(f'ON listening on {ip}:{ON_PORT}', flush=True)

        changed = False
        while True:
            ready, _, _ = select.select([fd], [], [], 1)
            if ready:
                try:
                    client_fd, _ = fd.accept()
                except OSError as error:
                    print(f'connection error: {error}', file=sys.stderr)
                    continue
                if len(nodes) == 26:
                    client_fd.close()
                    continue
                try:
                    buffer = receiveConnect(client_fd)
                except OSError:
                    print('incomplete CONNECT', file=sys.stderr)
                    client_fd.close()
                    continue
                vn_ip, port = struct.unpack('!4sH', buffer)
                vn = VirtualNodes(vn_ip, port, client_fd)
                if vn.port == 0 or vn.ip == b'\0\0\0\0' or vn.ip[0] == 127:
                    print('invalid CONNECT address', file=sys.stderr)
                    client_fd.close()
                    continue
                nodes.append(vn)
                changed = True

            # check for changes even while waiting for all VNs
            try:
                nextText = Path(configPath).read_text()
            except (OSError, UnicodeError):
                nextText = text
            if nextText != text:
                nextCosts = readConfig(nextText)
                if nextCosts is not None and len(nextCosts) == N:
                    fullCosts = nextCosts
                    changed = True
                else:
                    print('invalid config update; keeping previous costs', file=sys.stderr)
                text = nextText
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
    except KeyboardInterrupt:
        return 0
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
