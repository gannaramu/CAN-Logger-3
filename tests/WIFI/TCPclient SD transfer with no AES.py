#!/usr/bin/python3
import socket
import sys
import uuid
import struct

LOG_FILE_NAME = 'logfile_{}.bin'.format(uuid.uuid4()) #uuid4 generates a random universally unique identifier
Buffer_size = 16


#setup tcp client for CAN data transfer
SERVER_IP = "192.168.1.1" #insert IP address of server here
SERVER_PORT = 80

print("Connect to {} Port {}.".format(SERVER_IP, SERVER_PORT))
print('---------------------------------------------------------------------')

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    sock.connect((SERVER_IP, SERVER_PORT))
except OSError:
    print("Could not connect TCP Socket. Make sure SERVER_IP is correct.")
    sys.exit()


message_list = []


with open (LOG_FILE_NAME, 'wb') as file:
    while True:
        data = sock.recv(Buffer_size)
        print(len(data))
        print(data)
        if not data: break;
        file.write(data)
    file.close()        
sock.close()
sys.exit()