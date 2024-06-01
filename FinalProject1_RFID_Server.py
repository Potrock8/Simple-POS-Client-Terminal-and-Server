import socket
import threading
import datetime
import csv
import json
import os

class Server:
    #Server object initialization
    def __init__(self):
        self.host = socket.gethostbyname(socket.gethostname()) #Get the current IP address of the host
        self.port = 8000
        self.address = (self.host, self.port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM) #Create and initializeTCP/IP socket
        self.semaphore = threading.BoundedSemaphore(1) #Create and initialize a bounded semaphore set to one for locking

    #Main function of Server class
    def run(self):
        try:
            #Fields header for transactions csv file
            header = ["datetime", "device_id", "command", "old_balance", "updated_balance", "status"]

            #Check if "transactions.csv" exists in current directory. If it doesn't exist, create it with the variable header for its header fields.
            if not os.path.exists("transactions.csv"):
                with open("transactions.csv", "w+", encoding = "UTF8", newline = "") as file:
                    writer = csv.DictWriter(file, fieldnames = header)
                    writer.writeheader()

            #Bind the server's socket then listen for incoming connection requests. A maximum of five connections is set.
            self.socket.bind(self.address)
            self.socket.listen(5)
            print(f"Server is listening on {self.host}:{self.port}\n")

            #The server object continously loops and accepts connection requests. Each connection has its own thread that handles the client requests
            while True:
                conn, c_addr = self.socket.accept()
                thread = threading.Thread(target = self.handle_client, args = (conn, c_addr))
                thread.start()
        #Output socket error if an error is encountered
        except socket.error:
            print(socket.error)
    
    def handle_client(self, conn, c_addr):
        print(f"Client {c_addr} connected to server")
        print(f"Number of connected clients: {threading.active_count() - 1}\n")
        connected = True
        #The thread continously loops, waiting for data from the client. The connection is dropped when the client closes the connection.
        while connected:
            try:
                data = conn.recv(1024).decode("utf-8") #Receive data from the buffer whenever the client sends data to the server

                #If the client sends empty bytes, then it disconnected from the server
                if not data:
                    print(f"Client {c_addr} disconnected from the server")
                    connected = False
                    continue

                current_time = { "datetime" : str(datetime.datetime.now()) } #Log the time when the data arrived
                json_data = json.loads(data) #Convert the received json string data into a Python dictionary
                print(f"[{current_time['datetime']}] Data received from {c_addr}: {json_data}")
                client_data = current_time
                client_data.update(json_data)

                #The key-value pairs in data_entry will be stored in the csv file as a new transaction entry
                data_entry = {
                    "datetime": client_data["datetime"],
                    "device_id": client_data["device_id"],
                    "command": client_data["command"],
                    "old_balance": int(client_data["balance"]),
                    "updated_balance": int(client_data["balance"]),
                    "status": "Failed"
                }
                
                #Invoke a function, depending on the given command
                if client_data["command"] == "pay":
                    self.process_payment(conn, data_entry)
                elif client_data["command"] == "load":
                    self.process_load(conn, data_entry)
                else:
                    print("Unrecognized command, please try again.\n")
            
            #Outputs that the connection was forcibly closed on the client side then ends the loop on the server side
            except ConnectionResetError:
                connected = False
                print(f"{c_addr} forcibly closed their connection to the server\n")

        #Close the connection then outputs the remaining active connections/subthreads
        conn.close()
        print(f"Number of connected clients: {threading.active_count() - 2}\n")

    #Method that processes payment transactions. The default charge for payments is 25.
    def process_payment(self, conn, data):
        #The prompt to be printed out once the transaction has been completed
        prompt = "Payment Failed! Insufficient balance. Message Length:"
        card_balance = data["old_balance"]
        
        #The message to be sent from the server to the client
        server_message = {
            "device_id": data["device_id"],
            "command": "F",
            "updated_balance": str(card_balance).zfill(15)
        }

        #Checks if the balance given by the client is enough for payment and updates values accordingly.
        if card_balance >= 25:
            prompt = "Payment successful! Message Length:"
            card_balance = card_balance - 25
            data["updated_balance"] = card_balance
            data["status"] = "Success"
            server_message["command"] = "S"
            server_message["updated_balance"] = str(card_balance).zfill(15)

        self.semaphore.acquire() #Semaphore is acquired to prevent other threads from accessing and writing on "transactions.csv"
        prompt = self.write_to_file(data, prompt) #Calls the function write_to_file to write the transaction entry data onto "transactions.csv". The prompt is updated if a permission error is encountered due to the csv file being open when the program tries to update it.
        self.semaphore.release() #Semaphore is released to allow the next thread to access and update "transactions.csv"

        #If a permission error was encountered when writing to the csv file, update values accordingly
        if prompt == "Permission Error":
            data["updated_balance"] = data["old_balance"]
            server_message["command"] = "F"
            server_message["updated_balance"] = str(data["old_balance"]).zfill(15)
            prompt = 'Transaction failed. "transactions.csv" is currently open. Please close the file and try again. Message Length:'

        server_message = json.dumps(server_message) #Converts the Python dictionary into a json string
        conn.send(server_message.encode()) #Sends the json string to the connected client
        print(f"{prompt} {len(server_message)}, {data['device_id']} new balance: {data['updated_balance']}\n\n")

    #Method that processes payment transactions. The default load value is 100.
    def process_load(self, conn, data):
        #The prompt to be printed out once the transaction has been completed
        prompt = "Loading successful! Message Length:"
        card_balance = data["old_balance"]

        #Update values accordingly
        card_balance += 100
        data["updated_balance"] = card_balance 
        data["status"] = "Success"

        server_message = {
            "device_id": data["device_id"],
            "command": "S",
            "updated_balance": str(card_balance).zfill(15)
        }

        self.semaphore.acquire() #Semaphore is acquired to prevent other threads from accessing and writing on "transactions.csv"
        prompt = self.write_to_file(data, prompt) #Calls the function write_to_file to write the transaction entry data onto "transactions.csv". The prompt is updated if a permission error is encountered due to the csv file being open when the program tries to update it.
        self.semaphore.release() #Semaphore is released to allow the next thread to access and update "transactions.csv"

        if prompt == "Permission Error":
            data["updated_balance"] = data["old_balance"]
            server_message["command"] = "F"
            server_message["updated_balance"] = str(data["old_balance"]).zfill(15)
            prompt = 'Transaction failed. "transactions.csv" is currently open. Please close the file and try again. Message Length:'
        
        #If a permission error was encountered when writing to the csv file, update values accordingly
        server_message = json.dumps(server_message) #Converts the Python dictionary into a json string
        conn.send(server_message.encode()) #Sends the json string to the connected client
        print(f"{prompt} {len(server_message)}, {data['device_id']} new balance: {data['updated_balance']}\n\n")

    #Function used to update "transactions.csv"
    def write_to_file(self, data, prompt):
        header = ["datetime", "device_id", "command", "old_balance", "updated_balance", "status"]
        
        try:
            #Open and update the file with the given data
            with open("transactions.csv", "a+", encoding = "UTF8", newline = "") as file:
                writer = csv.DictWriter(file, fieldnames = header)
                writer.writerow(data)
        #If a permission error is encountered due to the file being open, return "Permission Error",
        except PermissionError:
            prompt = "Permission Error"

        return prompt

#Main function of the program which initializes a Server object then runs it
def main():
    server = Server()
    server.run()

if __name__ == "__main__":
    main()