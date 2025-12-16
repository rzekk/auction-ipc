#ifndef SERVER_H
#define SERVER_H

#include <QMainWindow>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <QTimer>
#include <QListWidgetItem>
#include <map>

QT_BEGIN_NAMESPACE
namespace Ui { class Server; }
QT_END_NAMESPACE

struct BidMessage {
    int client_id;
    int amount;
};

struct Lot {
    QString name;
    int startPrice;
};

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QWidget *parent = nullptr);
    ~Server();

signals:
    void logToGui(QString message);
    void updateHighestBid(int clientId, int amount);
    void updateTimerGui(QString timeStr);

    // Нові сигнали для списку клієнтів
    void addClientToGui(int id, QString name);
    void removeClientFromGui(int id);

private slots:
    void onLogReceived(QString message);
    void onUpdateBidReceived(int clientId, int amount);
    void on_startButton_clicked();
    void onTimerTick();

    // Слоти лотів
    void on_createLotButton_clicked();
    void on_deleteLotButton_clicked();
    void on_lotsListWidget_itemClicked(QListWidgetItem *item);

    // Нові слоти для списку клієнтів
    void onAddClient(int id, QString name);
    void onRemoveClient(int id);

private:
    Ui::Server *ui;

    SOCKET server_fd;
    bool isRunning = false;
    std::vector<SOCKET> connected_clients;
    std::mutex clients_mutex;

    // Карта імен: ID -> Name
    std::map<int, QString> client_names;

    // Аукціон
    std::queue<BidMessage> bid_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    int max_bid = 0;
    int winner_id = -1;

    QTimer *gameTimer;
    int remainingTime;

    std::vector<Lot> lots;
    int currentLotIndex = -1;

    void startServerThread(); // Метод для запуску потоку
    void clientHandler(SOCKET client_socket, int client_id);
    void auctionLogicLoop();
    void broadcastMessage(const std::string &msg);
    void setUIControlsEnabled(bool enabled);
    QString getClientName(int id);
};

#endif // SERVER_H
