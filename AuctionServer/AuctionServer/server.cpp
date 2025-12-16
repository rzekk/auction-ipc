#include "server.h"
#include "ui_server.h"
#include <string>


// TODO: WRITE UI

Server::Server(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Server)
{
    ui->setupUi(this);

    connect(this, &Server::logToGui, this, &Server::onLogReceived);
    connect(this, &Server::updateHighestBid, this, &Server::onUpdateBidReceived);
}

Server::~Server()
{
    isRunning = false;
    closesocket(server_fd);
    WSACleanup();
    delete ui;
}


void Server::on_startButton_clicked() {
    ui->startButton->setEnabled(false);
    startServer();
}

void Server::onLogReceived(QString message) {
    ui->logTextEdit->append(message);
}

void Server::onUpdateBidReceived(int clientId, int amount) {
    ui->bidLabel->setText(QString::number(amount));
    ui->winnerLabel->setText("Клієнт ID: " + QString::number(clientId));
}



void Server::startServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    isRunning = true;
    emit logToGui("Сервер запущено на порті 8080...");

    std::thread logicThread(&Server::auctionLogicLoop, this);
    logicThread.detach();

    std::thread acceptThread(&Server::serverAcceptLoop, this);
    acceptThread.detach();
}

void Server::serverAcceptLoop() {
    int client_count = 0;
    while (isRunning) {
        SOCKET new_socket = accept(server_fd, NULL, NULL);
        if (new_socket == INVALID_SOCKET) continue;

        client_count++;
        emit logToGui("Новий клієнт підключився! ID: " + QString::number(client_count));

        std::thread t(&Server::clientHandler, this, new_socket, client_count);
        t.detach();
    }
}

void Server::clientHandler(SOCKET client_socket, int client_id) {
    int bid_amount;
    while (isRunning) {
        int bytes_read = recv(client_socket, (char*)&bid_amount, sizeof(bid_amount), 0);

        if (bytes_read <= 0) {
            emit logToGui("Клієнт ID: " + QString::number(client_id) + " відключився.");
            closesocket(client_socket);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            bid_queue.push({client_id, bid_amount});
        }

        emit logToGui("Отримано ставку " + QString::number(bid_amount) + " від ID " + QString::number(client_id));
        queue_cv.notify_one();
    }
}

void Server::auctionLogicLoop() {
    emit logToGui("Аукціон почався!");

    while (isRunning) {
        BidMessage msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] { return !bid_queue.empty(); });

            msg = bid_queue.front();
            bid_queue.pop();
        }

        if (msg.amount > max_bid) {
            max_bid = msg.amount;
            winner_id = msg.client_id;

            emit logToGui("!!! Новий лідер: ID " + QString::number(winner_id) + " (" + QString::number(max_bid) + ")");
            emit updateHighestBid(winner_id, max_bid);

            // TODO: send signal to client once it's done
        } else {
            emit logToGui("Ставка " + QString::number(msg.amount) + " замала.");
        }
    }
}
