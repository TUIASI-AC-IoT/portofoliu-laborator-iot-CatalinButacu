import socket
import time

# Completati cu adresa IP a platformei ESP32
PEER_IP = "192.168.89.42"
PEER_PORT = 10001

TO_SEND = b"GPIO4=0"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
while 1:
    try:
        sock.sendto(TO_SEND, (PEER_IP, PEER_PORT))
        print("Am trimis mesajul: ", TO_SEND)
        TO_SEND = b"GPIO4=0" if TO_SEND == b"GPIO4=1" else b"GPIO4=1"
        time.sleep(1)
    except KeyboardInterrupt:
        break