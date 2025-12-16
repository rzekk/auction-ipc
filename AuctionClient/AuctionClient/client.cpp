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
    connect(this, &Client::updateLotInfo, this, &Client::onUpdateLot);
    connect(this, &Client::updateBidInfo, this, &Client::onUpdateBid);

    ui->betSpinBox->setRange(0, 1000000);
    ui->betSpinBox->setSingleStep(10);

    ui->currentLot->setReadOnly(true);
    ui->maxBet->setReadOnly(true);
    ui->yourBet->setReadOnly(true);
    ui->timer->setReadOnly(true);
    ui->logTextEdit->setReadOnly(true);

    ui->timer->setAlignment(Qt::AlignCenter);
    ui->maxBet->setAlignment(Qt::AlignCenter);
    ui->yourBet->setAlignment(Qt::AlignCenter);

    auctionActive = false;
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

void Client::onUpdateLot(QString lotName, QString startPrice) {
    ui->currentLot->setText(lotName);
    ui->maxBet->setText(startPrice);
    ui->yourBet->clear();

    auctionActive = true;

    updateLog("Увага! Новий лот: " + lotName);
}

void Client::onUpdateBid(QString newPrice) {
    ui->maxBet->setText(newPrice);
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
                    auctionActive = false;
                    emit messageReceived("Аукціон закінчився!");
                    emit timerUpdated("0");
                } else {
                    emit timerUpdated(timeStr);
                }
            }
            else if (msg.startsWith("STOP:")) {
                auctionActive = false;
                emit messageReceived(msg.mid(5));
            }
            else if (msg.startsWith("BID:")) {
                QString price = msg.mid(4);
                emit updateBidInfo(price);
                emit messageReceived("Нова ставка: " + price);
            }
            else if (msg.startsWith("LOT:")) {
                QStringList parts = msg.split(":");
                if (parts.size() >= 3) {
                    emit updateLotInfo(parts[1], parts[2]);
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


void Client::on_betButton_clicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "Помилка", "Спочатку підключіться до сервера!");
        return;
    }

    if (!auctionActive) {
        QMessageBox::warning(this, "Увага", "На даний момент немає активного лоту для ставок!");
        return;
    }

    int amount = ui->betSpinBox->value();

    int currentMax = ui->maxBet->text().toInt();
    if (amount <= currentMax) {
        QMessageBox::warning(this, "Увага", "Ваша ставка має бути вищою за поточну (" + QString::number(currentMax) + ")!");
        return;
    }

    ui->yourBet->setText(QString::number(amount));

    int sendResult = send(client_socket, (char*)&amount, sizeof(int), 0);

    if (sendResult == SOCKET_ERROR) {
        ui->logTextEdit->append("Помилка відправки ставки!");
    } else {
        ui->logTextEdit->append("Ви зробили ставку: " + QString::number(amount));
    }
}
