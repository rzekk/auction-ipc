#include "client.h"
#include "ui_client.h"
#include <WS2tcpip.h> // Потрібно для inet_pton
#include <QMessageBox>

Client::Client(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Client)
{
    ui->setupUi(this);

    // З'єднуємо сигнал отримання повідомлення зі слотом оновлення екрану
    connect(this, &Client::messageReceived, this, &Client::updateLog);
}

Client::~Client()
{
    isConnected = false;
    // Закриваємо сокет
    if (client_socket != INVALID_SOCKET) {
        shutdown(client_socket, SD_BOTH);
        closesocket(client_socket);
    }
    // Чекаємо завершення потоку (якщо він був)
    if (receiverThread && receiverThread->joinable()) {
        receiverThread->join();
        delete receiverThread;
    }
    WSACleanup();
    delete ui;
}

// === ЛОГІКА ПІДКЛЮЧЕННЯ ===
void Client::on_connectButton_clicked()
{
    QString ip = ui->ipLineEdit->text(); // Наприклад "127.0.0.1"
    int port = ui->portLineEdit->text().toInt(); // Наприклад 8080

    if (connectToServer(ip, port)) {
        ui->connectButton->setEnabled(false);
        ui->bidButton->setEnabled(true);
        ui->logTextEdit->append("Підключено до сервера!");
    } else {
        QMessageBox::critical(this, "Помилка", "Не вдалося підключитися до сервера");
    }
}

bool Client::connectToServer(QString ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // Конвертація IP рядка в структуру адреси
    std::string ipStr = ip.toStdString();
    inet_pton(AF_INET, ipStr.c_str(), &serverAddr.sin_addr);

    if (::connect(client_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(client_socket);
        return false;
    }

    // Успіх! Запускаємо слухача
    isConnected = true;
    receiverThread = new std::thread(&Client::receiveLoop, this);

    return true;
}

// === ФОНОВИЙ ПОТІК (СЛУХАЧ) ===
void Client::receiveLoop() {
    char buffer[4096];

    while (isConnected) {
        ZeroMemory(buffer, 4096);
        // recv блокує потік, поки сервер щось не надішле
        int bytesIn = recv(client_socket, buffer, 4096, 0);

        if (bytesIn > 0) {
            // Перетворюємо байти в рядок
            QString msg = QString::fromUtf8(buffer, bytesIn);
            // Відправляємо в GUI через сигнал
            emit messageReceived(msg);
        }
        else if (bytesIn == 0 || bytesIn == SOCKET_ERROR) {
            // Зв'язок розірвано
            emit messageReceived("Зв'язок з сервером втрачено.");
            isConnected = false;
            break;
        }
    }
}

// === ВІДПРАВКА СТАВКИ ===
void Client::on_bidButton_clicked()
{
    if (!isConnected) return;

    int amount = ui->bidSpinBox->value();

    // Згідно вашого коду сервера, він очікує чистий int (4 байти)
    int sendResult = send(client_socket, (char*)&amount, sizeof(int), 0);

    if (sendResult == SOCKET_ERROR) {
        ui->logTextEdit->append("Помилка відправки ставки!");
    } else {
        ui->logTextEdit->append("Ви зробили ставку: " + QString::number(amount));
    }
}

// === ОНОВЛЕННЯ ІНТЕРФЕЙСУ ===
void Client::updateLog(QString msg) {
    ui->logTextEdit->append(msg);
}
