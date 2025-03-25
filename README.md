# P2P-Chat-Box 🖥️

A **Single-File Peer-to-Peer Chatbox** using a **Local Area Network (LAN)** for communication, written in **C language**. This terminal-based application allows users on the same network to join a chatroom, exchange messages, and see who is online in real-time. It's a lightweight solution for local chatting without the need for centralized servers.

## Features 📝
- **Peer-to-Peer Communication**: No central server required. All peers in the same chatroom can communicate directly.
- **LAN Discovery**: Automatically detects and lists available peers in the network.
- **Inactivity Detection**: Peers that are inactive are automatically removed from the available peer list.
- **Simple User Interface**: Terminal-based interface, making it easy and fast to use in any environment.

## Terminal-Based Interface ⌨️
This chat application runs entirely in the terminal, with simple prompts and command-line interactions. Messages are displayed with colored formatting to distinguish between peers, making the chat experience clearer. Users can start sending messages, see the current participants, and the system will handle broadcasting the messages over the network.

---

## How It Works ❓

1. **Peer Discovery** 🔍:
   - Once connected, the application sends periodic LAN broadcasts to discover peers in the same chatroom. It then displays the list of online users.

2. **Chat Messaging** 💬: 
   - After setting up, users can type and send messages which are broadcasted to all other peers in the same room.
   - Received messages are displayed in the terminal with usernames clearly labeled.

3. **Inactivity Handling** ⏳:
   - If a peer becomes inactive for too long, the system automatically removes them from the list of active users.

---

## Usage 🚀

1. **Compile** the program using any C compiler:
   ```bash
   gcc -o main main.c -lws2_32
