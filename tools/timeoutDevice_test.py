import socket
s = socket.socket()
s.bind(("127.0.0.1", 83))
s.listen()
input("Listening on 83, never answering. Press Enter to stop.")