#!/usr/bin/python3
import socket
import sys
import uuid
import struct

LOG_FILE_NAME = 'canlog_{}.txt'.format(uuid.uuid4()) #uuid4 generates a random universally unique identifier
Buffer_size = 100
CAN_size = 25

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


with open (LOG_FILE_NAME, 'w') as file:
    while True:
        data = sock.recv(Buffer_size)
        if not data: break;
        for i in range (int(len(data)/CAN_size)):
            record = data[CAN_size*i:CAN_size*(i+1)]
            channel = record[0]
            timeSeconds = struct.unpack("<L",record[1:5])[0]
            timeMicrosecondsAndDLC = struct.unpack("<L",record[13:17])[0]
            DLC =  (timeMicrosecondsAndDLC & 0xFF000000) >> 24
            timeMicroseconds = (timeMicrosecondsAndDLC & 0x00FFFFFF)
            abs_time = timeSeconds + timeMicroseconds * 0.000001
            ID = struct.unpack("<L",record[9:13])[0]
            message_bytes = record[17:(17+DLC)]
            hex_data_print = ' '.join(["{:02X}".format(b) for b in message_bytes])
            print("{:0.6f} can{:0.0f} {:08X} [{}] {}\n".format(abs_time, channel, ID, DLC, hex_data_print))
            file.write("{:0.6f} can{:0.0f} {:08X} [{}] {}\n".format(abs_time, channel, ID, DLC, hex_data_print))
    file.close()        
sock.close()
sys.exit()