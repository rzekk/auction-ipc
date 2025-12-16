#include "client.h"
#include "ui_client.h"
#include <WS2tcpip.h>
#include <QMessageBox>
#include <QLineEdit>

Client::Client(QWidget *parent) : QMainWindow(parent), ui(new Ui::Client)
{
    ui->setupUi(this);
    connect(this, &Client::messageReceived, this, &Client::updateLog);
    connect(this, &Client::timerUpdated, this, &Client::updateTimerDisplay);
}

Client::~Client()
{
    isConnected = false;
    if (client_socket != INVALID_SOCKET) {
        shutdown(client_socket, SD_BOTH);
        closesocket(client_socket);
    }
    if (receiverThread && receiverThread->joinable()) {
        receiverThread->join();
        delete receiverThread;
    }
    WSACleanup();
    delete ui;
}

void Client::on_connectButton_clicked()
{
    QString ip = ui->ipLineEdit->text();
    int port = ui->portLineEdit->text().toInt();
    QString username = ui->usernameLineEdit->text();

    if (username.isEmpty()) {
        QMessageBox::warning(this, "Помилка", "Введіть ім'я користувача!");
        return;
    }

    if (connectToServer(ip, port, username)) {
        ui->connectButton->setEnabled(false);
        ui->ipLineEdit->setEnabled(false);
        ui->portLineEdit->setEnabled(false);
        ui->usernameLineEdit->setEnabled(false);
        ui->logTextEdit->append("Підключено до сервера як " + username);
    } else {
        ui->logTextEdit->append("Не вдалося підключитися.");
    }
}

bool Client::connectToServer(QString ip, int port, QString username) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.toStdString().c_str(), &serverAddr.sin_addr);

    if (::connect(client_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(client_socket);
        WSACleanup();
        return false;
    }

    std::string nameStr = username.toStdString();
    send(client_socket, nameStr.c_str(), nameStr.length(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    isConnected = true;
    receiverThread = new std::thread(&Client::receiveLoop, this);

    return true;
}

void Client::receiveLoop() {
    char buffer[4096];

    while (isConnected) {
        ZeroMemory(buffer, 4096);
        int bytesIn = recv(client_socket, buffer, 4096, 0);

        if (bytesIn > 0) {
            QString msg = QString::fromUtf8(buffer, bytesIn);

            if (msg.startsWith("TIMER:")) {
                QString timeStr = msg.mid(6);
                if (timeStr == "END") {
                    emit messageReceived("Аукціон закінчився!");
                    emit timerUpdated("0");
                } else {
                    emit timerUpdated(timeStr);
                }
            }
            else if (msg.startsWith("STOP:")) {
                emit messageReceived(msg.mid(5));
            }
            else if (msg.startsWith("BID:")) {
                emit messageReceived("Нова максимальна ставка: " + msg.mid(4));
            }
            else if (msg.startsWith("LOT:")) {
                QStringList parts = msg.split(":");
                if (parts.size() >= 3) {
                    emit messageReceived("Новий лот: " + parts[1] + " (Старт: " + parts[2] + ")");
                }
            }
            else {
                emit messageReceived(msg);
            }
        }
        else if (bytesIn == 0 || bytesIn == SOCKET_ERROR) {
            emit messageReceived("Зв'язок розірвано.");
            isConnected = false;
            break;
        }
    }
}

void Client::on_bidButton_clicked()
{
    if (!isConnected) return;
    int amount = ui->betSpinBox->value();
    int sendResult = send(client_socket, (char*)&amount, sizeof(int), 0);

    if (sendResult == SOCKET_ERROR) {
        ui->logTextEdit->append("Помилка відправки ставки!");
    } else {
        ui->logTextEdit->append("Ви зробили ставку: " + QString::number(amount));
    }
}

void Client::updateLog(QString msg) {
    ui->logTextEdit->append(msg);
}

void Client::updateTimerDisplay(QString timeStr) {
    QLineEdit *timerWidget = this->findChild<QLineEdit*>("timer");
    if (timerWidget) {
        timerWidget->setText(timeStr);
    }
}
