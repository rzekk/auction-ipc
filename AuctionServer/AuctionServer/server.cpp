#include "server.h"
#include "ui_server.h"
#include <string>
#include <WS2tcpip.h>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>

Server::Server(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Server)
{
    ui->setupUi(this);

    // Основні сигнали
    connect(this, &Server::logToGui, this, &Server::onLogReceived);
    connect(this, &Server::updateHighestBid, this, &Server::onUpdateBidReceived);

    // Сигнали списку клієнтів
    connect(this, &Server::addClientToGui, this, &Server::onAddClient);
    connect(this, &Server::removeClientFromGui, this, &Server::onRemoveClient);

    // Таймер
    connect(this, &Server::updateTimerGui, ui->timer, &QLineEdit::setText);
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Server::onTimerTick);

    // === НАЛАШТУВАННЯ ІНТЕРФЕЙСУ (READ ONLY) ===
    // Забороняємо змінювати інформаційні поля вручну
    ui->currentLot->setReadOnly(true);
    ui->currentBet->setReadOnly(true);
    ui->clientName->setReadOnly(true);
    ui->timer->setReadOnly(true);
    ui->logTextEdit->setReadOnly(true);

    // Вирівнювання тексту для краси
    ui->timer->setAlignment(Qt::AlignCenter);
    ui->currentBet->setAlignment(Qt::AlignCenter);
    ui->clientName->setAlignment(Qt::AlignCenter);

    // Запускаємо сервер
    std::thread(&Server::startServerThread, this).detach();
}

Server::~Server()
{
    isRunning = false;
    closesocket(server_fd);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (SOCKET s : connected_clients) {
            closesocket(s);
        }
    }
    WSACleanup();
    delete ui;
}

// === УПРАВЛІННЯ СПИСКОМ КЛІЄНТІВ (GUI) ===

void Server::onAddClient(int id, QString name) {
    QString itemText = name + " (ID: " + QString::number(id) + ")";
    QListWidgetItem *item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, id);
    ui->clientsListWidget->addItem(item);
}

void Server::onRemoveClient(int id) {
    for (int i = 0; i < ui->clientsListWidget->count(); ++i) {
        QListWidgetItem *item = ui->clientsListWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == id) {
            delete ui->clientsListWidget->takeItem(i);
            break;
        }
    }
}

// === ДОПОМІЖНІ ===

QString Server::getClientName(int id) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    if (client_names.find(id) != client_names.end()) {
        return client_names[id] + " (ID:" + QString::number(id) + ")";
    }
    return "Невідомий (ID:" + QString::number(id) + ")";
}

void Server::setUIControlsEnabled(bool enabled) {
    ui->startButton->setEnabled(enabled);
    ui->createLotButton->setEnabled(enabled);
    ui->deleteLotButton->setEnabled(enabled);
    ui->lotsListWidget->setEnabled(enabled);
}

// === СЛОТИ GUI ===

void Server::on_startButton_clicked() {
    if (currentLotIndex == -1) {
        QMessageBox::warning(this, "Увага", "Спочатку оберіть лот зі списку!");
        return;
    }

    setUIControlsEnabled(false);
    remainingTime = 60;
    ui->timer->setText(QString::number(remainingTime));
    gameTimer->start(1000);

    emit logToGui("Аукціон розпочато по лоту: " + ui->currentLot->text());
}

void Server::onLogReceived(QString message) {
    ui->logTextEdit->append(message);
}

void Server::onUpdateBidReceived(int clientId, int amount) {
    ui->currentBet->setText(QString::number(amount));
    ui->clientName->setText(getClientName(clientId));
}

// === РОБОТА З ЛОТАМИ ===

void Server::on_createLotButton_clicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("Створити новий лот");
    QFormLayout form(&dialog);
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    QSpinBox *priceBox = new QSpinBox(&dialog);
    priceBox->setRange(0, 1000000);
    priceBox->setSingleStep(10);
    form.addRow("Назва лоту:", nameEdit);
    form.addRow("Початкова ставка:", priceBox);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text();
        int price = priceBox->value();
        if (name.isEmpty()) return;

        lots.push_back({name, price});
        ui->lotsListWidget->addItem(name + " (" + QString::number(price) + ")");
        emit logToGui("Створено лот: " + name);
    }
}

void Server::on_lotsListWidget_itemClicked(QListWidgetItem *item) {
    currentLotIndex = ui->lotsListWidget->row(item);
    if (currentLotIndex >= 0 && currentLotIndex < lots.size()) {
        Lot selectedLot = lots[currentLotIndex];
        ui->currentLot->setText(selectedLot.name);
        ui->currentBet->setText(QString::number(selectedLot.startPrice));
        ui->clientName->setText("Немає лідера");
        max_bid = selectedLot.startPrice;
        winner_id = -1;
        std::string msg = "LOT:" + selectedLot.name.toStdString() + ":" + std::to_string(selectedLot.startPrice);
        broadcastMessage(msg);
        emit logToGui("Вибрано лот: " + selectedLot.name);
    }
}

void Server::on_deleteLotButton_clicked() {
    int row = ui->lotsListWidget->currentRow();
    if (row < 0 || row >= lots.size()) return;
    lots.erase(lots.begin() + row);
    delete ui->lotsListWidget->takeItem(row);
    ui->currentLot->clear(); ui->currentBet->clear(); ui->clientName->clear();
    currentLotIndex = -1; max_bid = 0; winner_id = -1;
    emit logToGui("Лот видалено.");
}

// === ТАЙМЕР ===

void Server::onTimerTick() {
    remainingTime--;
    emit updateTimerGui(QString::number(remainingTime));
    broadcastMessage("TIMER:" + std::to_string(remainingTime));

    if (remainingTime <= 0) {
        gameTimer->stop();
        broadcastMessage("TIMER:END");

        if (winner_id != -1) {
            QString winnerName = getClientName(winner_id);
            QString winMsg = "Переможець: " + winnerName + " (Сума: " + QString::number(max_bid) + ")";
            broadcastMessage("STOP:" + winMsg.toStdString());
            emit logToGui("Аукціон завершено! " + winMsg);
        } else {
            QString failMsg = "Час вийшов. Ставок не зроблено.";
            broadcastMessage("STOP:" + failMsg.toStdString());
            emit logToGui(failMsg);
        }

        if (currentLotIndex != -1 && currentLotIndex < lots.size()) {
            lots.erase(lots.begin() + currentLotIndex);
            delete ui->lotsListWidget->takeItem(currentLotIndex);
            ui->currentLot->clear(); ui->currentBet->clear(); ui->clientName->clear();
            currentLotIndex = -1;
        }
        setUIControlsEnabled(true);
    }
}

// === МЕРЕЖА ===

void Server::startServerThread() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        emit logToGui("Помилка WSAStartup");
        return;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        emit logToGui("Помилка Bind (порт зайнятий?)");
        return;
    }
    listen(server_fd, 3);

    isRunning = true;
    std::thread(&Server::auctionLogicLoop, this).detach();

    int client_id_counter = 0;
    emit logToGui("Сервер запущено на порту 8080. Очікування клієнтів...");

    while (isRunning) {
        int addrlen = sizeof(address);
        SOCKET new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);

        if (new_socket == INVALID_SOCKET) continue;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            connected_clients.push_back(new_socket);
        }

        client_id_counter++;
        std::thread(&Server::clientHandler, this, new_socket, client_id_counter).detach();
    }
}

void Server::clientHandler(SOCKET client_socket, int client_id) {
    char nameBuffer[1024];
    ZeroMemory(nameBuffer, 1024);

    int nameBytes = recv(client_socket, nameBuffer, 1024, 0);
    QString username = "Unknown";

    if (nameBytes > 0) {
        username = QString::fromUtf8(nameBuffer, nameBytes);
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_names[client_id] = username;
        }
        emit logToGui("Підключився: " + username);
        emit addClientToGui(client_id, username);
    } else {
        closesocket(client_socket);
        return;
    }

    int bid_amount;
    while (isRunning) {
        int bytes_read = recv(client_socket, (char*)&bid_amount, sizeof(bid_amount), 0);

        if (bytes_read <= 0) {
            emit logToGui(username + " відключився.");
            emit removeClientFromGui(client_id);
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                client_names.erase(client_id);
            }
            closesocket(client_socket);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            bid_queue.push({client_id, bid_amount});
        }
        emit logToGui("Ставка " + QString::number(bid_amount) + " від " + username);
        queue_cv.notify_one();
    }
}

void Server::broadcastMessage(const std::string &msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (SOCKET sock : connected_clients) {
        send(sock, msg.c_str(), msg.length(), 0);
    }
}

void Server::auctionLogicLoop() {
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

            // === ПОВЕРТАЄМО ТАЙМЕР НА 60 СЕК ПРИ НОВІЙ СТАВЦІ ===
            if (remainingTime > 0) {
                remainingTime = 60;
                emit updateTimerGui("60");
                broadcastMessage("TIMER:60");
                emit logToGui("Час продовжено до 60с через нову ставку.");
            }
            // ====================================================

            emit updateHighestBid(winner_id, max_bid);

            std::string updateMsg = "BID:" + std::to_string(max_bid);
            broadcastMessage(updateMsg);
        }
    }
}
