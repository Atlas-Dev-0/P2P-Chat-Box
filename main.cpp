#include <complex.h>
#include <conio.h> 
#include <minwindef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <synchapi.h>
#include <time.h> 
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>          
#pragma comment(lib, "Ws2_32.lib") 

#define UDP_BROADCAST_PORT 5555
#define TCP_CHAT_PORT 6666
#define BROADCAST_IP "255.255.255.255"
#define MAX_PEERS 20
#define BUFFER_SIZE 2024
#define MAX_MESSAGES 100

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define TIMEOUT_SECONDS 5 // Set timeout threshold for inactivity

typedef struct {
  char ip[INET_ADDRSTRLEN];
  char username[50];
  time_t last_seen; // Track last time a message was received
} Peer;

Peer peer_list[MAX_PEERS];
int peer_count = 0;
CRITICAL_SECTION peer_mutex;
char my_username[50];
char my_room[50];

HANDLE hConsole; // Console Handle for Colors

// Function to set console text color
void set_console_color(int color) { SetConsoleTextAttribute(hConsole, color); }

void add_peer(const char *ip, const char *username) {
  EnterCriticalSection(&peer_mutex);

  // ====== Check if ip address already exists ======
  for (int i = 0; i < peer_count; i++) {
    if (strcmp(peer_list[i].ip, ip) == 0) {
      peer_list[i].last_seen = time(NULL);
      LeaveCriticalSection(&peer_mutex);
      return;
    }
  }

  // ====== Add new user to peer list array ======
  if (peer_count < MAX_PEERS) {
    strcpy(peer_list[peer_count].ip, ip);
    strcpy(peer_list[peer_count].username, username);
    peer_list[peer_count].last_seen = time(NULL);
    set_console_color(10); // Green Color
    set_console_color(7);  // Reset to default color
    peer_count++;
  }

  LeaveCriticalSection(&peer_mutex);
}

// Function to detect and remove inactive peers
void check_disconnections() {
  EnterCriticalSection(&peer_mutex);
  time_t now = time(NULL);

  for (int i = 0; i < peer_count; i++) {
    if (difftime(now, peer_list[i].last_seen) > TIMEOUT_SECONDS) {
      // Peer is inactive, remove from list
      set_console_color(12);
      printf("Peer [%s] (%s) disconnected due to inactivity.\n",
             peer_list[i].username, peer_list[i].ip);
      set_console_color(7);

      // Shift remaining peers down to fill the gap
      for (int j = i; j < peer_count - 1; j++) {
        peer_list[j] = peer_list[j + 1];
      }
      peer_count--;
      i--; // Adjust index since we removed an entry
    }
  }
  LeaveCriticalSection(&peer_mutex);
}

void list_available_peers() {
  EnterCriticalSection(&peer_mutex);
  set_console_color(14); // Yellow color
  printf("\nAvailable Peers in Room [%s]:\n", my_room);
  set_console_color(7); // Reset
  if (peer_count == 0) {
    set_console_color(12); // Red color for no peers
    printf("No Available Peers Found.\n");
    set_console_color(7); // Reset
  } else {
    for (int i = 0; i < peer_count; i++) {
      set_console_color(11); // Cyan color
      printf("[%d] %s - %s\n", i + 1, peer_list[i].username, peer_list[i].ip);
      set_console_color(7); // Reset
    }
  }

  LeaveCriticalSection(&peer_mutex);
}

void send_broadcast(SOCKET udp_socket, const char* message) {
    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(UDP_BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // Always send the LAN_CHAT_DISCOVERY message
    char discovery_message[BUFFER_SIZE];
    sprintf(discovery_message, "LAN_CHAT_DISCOVERY %s %s", my_room, my_username);

    sendto(udp_socket, discovery_message, strlen(discovery_message), 0,
           (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));

    // If a chat message is available, send it
    if (message && strlen(message) > 0) {
        char encoded_message[BUFFER_SIZE];
        sprintf(encoded_message, "LAN_MESSAGE: %s %s", my_username, message);
        int result = sendto(udp_socket, encoded_message, strlen(encoded_message), 0,
                            (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
        if (result == SOCKET_ERROR) {
            printf("Error: Failed to send broadcast message. Error code: %d\n", WSAGetLastError());
            return;  // Error occurred, exit the function
        }
        printf("Message broadcasted successfully!\n");
    }
}


char message[BUFFER_SIZE];
char message_log[MAX_MESSAGES][BUFFER_SIZE];
char error_log[MAX_MESSAGES][BUFFER_SIZE];
int message_count = 0;

void display_chat() {
    set_console_color(14);  // Yellow for room name
    printf("Room [%s] Chat Log:\n", my_room);
    set_console_color(7);  // Reset color

    for (int i = 0; i < message_count; i++) {
        printf("%s\n", message_log[i]);  // Display each message
    }
}

void add_message_to_log(const char* message) {
    if (message_count < MAX_MESSAGES) {
        strcpy(message_log[message_count], message);
        message_count++;
    } else {
        // Shift messages up when log is full
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            strcpy(message_log[i], message_log[i + 1]);
        }
        strcpy(message_log[MAX_MESSAGES - 1], message);
    }
}

void catch(const char* error) {
    strcpy(error_log[0], error);
}

void printError() {
    printf("%s\n", error_log[0]);  // Display each message
}

int isUserChatting() {
    if (_kbhit()) { 
        char key = _getch();
        printf("\nKey pressed: %c\n", key);

        if (key == 'y' || key == 'Y') {  // Check if 'y' or 'Y' is pressed
            return 1; // Start chatting if 'y' is pressed
        } else {
            printf("Press 'y' to start chatting.\n");
            return 0;
        }
    } else {
        printf("..");
        fflush(stdout);
        Sleep(100);
        return 0;
    }
}


DWORD WINAPI listen_for_peers(LPVOID param) {
    SOCKET udp_socket = *(SOCKET *)param;
    struct sockaddr_in sender_addr;
    int sender_addr_len = sizeof(sender_addr);
    char buffer[BUFFER_SIZE];

    while (1) {
        // Listen for incoming broadcast messages
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';  // Null-terminate the received string

            char received_room[50], received_username[50], received_message[BUFFER_SIZE];

            // First, check for LAN_CHAT_DISCOVERY messages
            if (sscanf_s(buffer, "LAN_CHAT_DISCOVERY %49s %49s", 
                         received_room, (unsigned)_countof(received_room), 
                         received_username, (unsigned)_countof(received_username)) == 2) {

                // Only add peers if they're in the same room
                if (strcmp(received_room, my_room) == 0) {
                    add_peer(inet_ntoa(sender_addr.sin_addr), received_username);
                }

            // Otherwise, check for LAN_MESSAGE format (chat messages)
            } else if (sscanf_s(buffer, "LAN_MESSAGE: %49s %[^\n]", 
                                received_username, (unsigned)_countof(received_username), 
                                received_message, (unsigned)_countof(received_message)) == 2) {

                // Display or log the received message
                char log_entry[BUFFER_SIZE];
                sprintf(log_entry, "[%s]: %s", received_username, received_message);
                add_message_to_log(log_entry); // Assuming you have a function to log or display messages
            }
        }
    }
    return 0;
}

int main() {
  system("cls");
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
  InitializeCriticalSection(&peer_mutex);
  hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  // ======= GET USERNAME =========
  set_console_color(14); // Yellow color
  printf("Enter Your Username (no spaces): ");
  set_console_color(7); // Reset to default
  fgets(my_username, sizeof(my_username), stdin);
  my_username[strcspn(my_username, "\n")] = 0;

  // ======= GET ROOM NAME =========
  set_console_color(14); // Yellow color
  printf("Enter Room Name (Create/Join an Existing Room): ");
  set_console_color(7); // Reset to default

  fgets(my_room, sizeof(my_room), stdin);
  my_room[strcspn(my_room, "\n")] = 0;

  printf("Connecting to [%s]: \n", my_room);

  SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (udp_socket == INVALID_SOCKET) {
    set_console_color(12); // Red color for errors
    printf("Failed to create UDP socket.\n");
    set_console_color(7); // Reset
    return 1;
  }

  // Allow Multiple Instances to bind to the same UDP port
  BOOL reuse = TRUE;
  setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
             sizeof(reuse));

  // Allow Broadcasting
  BOOL broadcastEnable = TRUE;
  setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcastEnable,
             sizeof(broadcastEnable));


  // =================================================
  // Define a sockaddr_in structure to store address information for the UDP
  // socket
  struct sockaddr_in udp_addr;

  // Specify that the socket will use the IPv4 address family
  udp_addr.sin_family = AF_INET;

  // Set the port number for the socket, converting it to network byte order
  // (Big Endian) UDP_BROADCAST_PORT is predefined as 5555
  udp_addr.sin_port = htons(UDP_BROADCAST_PORT);

  // Bind the socket to all available network interfaces on the machine
  // This allows the program to receive UDP packets from any IP address on the
  // local network
  udp_addr.sin_addr.s_addr = INADDR_ANY;
  // =================================================
  
  if (bind(udp_socket, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) ==
      SOCKET_ERROR) {
    set_console_color(12); // Red for error
    printf("Binding failed. Error: %d\n", WSAGetLastError());
    set_console_color(7); // Reset
    return 1;
  }

  // ===== Handle Thread for Listening =====
  HANDLE listenToNewConnections = CreateThread(NULL, 0, listen_for_peers, &udp_socket, 0, NULL);


  // ==== Run Program ====
  while (1) {
    send_broadcast(udp_socket, NULL); 
    // printError();
    if (isUserChatting()) {
        printf("Enter to send message: ");
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0;
        send_broadcast(udp_socket, message); 
        printf("Sending Message..\n");
        Sleep(250);
    } else {
        send_broadcast(udp_socket, NULL); 
        check_disconnections();
        system("cls");
        list_available_peers();
    }

    display_chat();
  }

  closesocket(udp_socket);
  DeleteCriticalSection(&peer_mutex); // Clean up before exiting
  WSACleanup();

  return 0;
}
