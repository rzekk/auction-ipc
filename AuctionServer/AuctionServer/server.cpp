#include "server.h"
#include "ui_server.h"
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "protocol.h"

Server::Server(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Server)
{
    ui->setupUi(this);
}

Server::~Server()
{
    delete ui;
}


int max_bid = 0;
int winner_id = -1;

std::queue<BidMessage> bid_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;


// this function is called in a different detached thread for every client
// every client has its own thread, which will receive the data through a socket and then process it
// the bids submitted will be queued into a bid_queue
// every time data changes it is notified with a conditional variable
// we use mutex to prevent 2 threads from submitting the bid simultaneously
void handle_client(SOCKET client_socket, int client_id) {
    int bid_amount;
    while (true) {
        int bytes_read = recv(client_socket, (char*)&bid_amount, sizeof(bid_amount), 0);

        if (bytes_read <= 0) {
            std::cout << "Client ID: " << client_id << " disconnected.\n";
            closesocket(client_socket);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            bid_queue.push({client_id, bid_amount});
            std::cout << "[Network] Received a bid $" << bid_amount << " from a client with ID " << client_id << "\n";
        }

        queue_cv.notify_one();
    }
}

// function that processes the bids from client threads
// it takes first element from a bid_queue and then checks if it's the biggest
void auction_logic() {
    std::cout << "The auction has begun! Starting bid: " << max_bid << "\n";

    while (true) {
        BidMessage msg;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [] { return !bid_queue.empty(); });

            msg = bid_queue.front();
            bid_queue.pop();
        }

        if (msg.amount > max_bid) {
            max_bid = msg.amount;
            winner_id = msg.client_id;
            std::cout << "!!! We have a new leader: Client " << winner_id << " with a bid of $" << max_bid << "!\n";
        } else {
            std::cout << "... The bid $" << msg.amount << " is too small. Current max bid is $" << max_bid << ".\n";
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error with WSAStartup\n";
        return 1;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind failed\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed\n";
        return 1;
    }


    std::thread auction_thread(auction_logic);
    auction_thread.detach();

    std::cout << "Server launched on port " << PORT << "...\n";

    int client_count = 0;
    while (true) {
        SOCKET new_socket = accept(server_fd, NULL, NULL);
        if (new_socket == INVALID_SOCKET) {
            std::cerr << "\"accept\" error\n";
            continue;
        }

        client_count++;
        std::cout << "New client connected! ID: " << client_count << ".\n";

        std::thread t(handle_client, new_socket, client_count);
        t.detach();
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}

