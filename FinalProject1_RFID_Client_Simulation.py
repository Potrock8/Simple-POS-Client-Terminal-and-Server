import socket
import threading
import json
import time
import random

stop_counter = False

class Client(threading.Thread):
    def __init__(self, number, sleep_time, server_sock):
        threading.Thread.__init__(self)
        self.number = number
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sleep_time = sleep_time
        self.server_sock = server_sock
        self.transaction_count = 0

    def run(self):
        counter = threading.Thread(target=self.countTransactions)
        counter.start()
        random_val = random.randint(0, 100)
        prompt = "FAILED"

        message = {
            "device_id": "1000" + str(self.number),
            "command": "load",
            "balance": random_val
        }

        if self.number == 3:
            random_val = 88
            message["balance"] = 1

        time.sleep(self.sleep_time)
        self.socket.connect(self.server_sock)
        print(f"Client {self.number} connected to the server\n")
        
        for i in range(5):
            if random_val < 51:
                message["command"] = "load"
            else:
                message["command"] = "pay"

            json_message = json.dumps(message)
            print(f"Message TO server: {json_message}")
            self.socket.send(json_message.encode())

            received_message = self.socket.recv(1024).decode("utf-8")
            print(f"Message FROM server: {received_message}")

            received_message = json.loads(received_message)
            message["balance"] = int(received_message["updated_balance"])
            status = received_message["command"]

            if status == "S":
                prompt = "SUCCESSFUL"
            else:
                prompt = "FAILED"
            
            print(f"Transaction {prompt}")
            print(f"Updated Balance: {str(message['balance']).zfill(15)}\n")
            time.sleep(self.sleep_time)
            self.transaction_count += 1
            random_val = random.randint(0, 100)
        
        self.socket.close()

    def countTransactions(self):
        global stop_counter

        while stop_counter == False:
            time.sleep(10)
            print(f"Client {self.number} number of transactions: {self.transaction_count}\n")

def main():
    global stop_counter
    server_IP = input("Enter server IP Address (ex. 192.168.100.100): ")
    server_port = 8000
    server_sock = (server_IP, server_port)
    num_clients = 3
    clients = []

    for i in range(num_clients):
        client = Client(i+1, random.randint(1, num_clients), server_sock)
        clients.append(client)
        client.start()

    for client in clients:
        client.join()
 
    stop_counter = True

if __name__ == "__main__":
    main()