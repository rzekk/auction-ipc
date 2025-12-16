#ifndef SERVER_H
#define SERVER_H

#include <QMainWindow>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

QT_BEGIN_NAMESPACE
namespace Ui { class Server; }
QT_END_NAMESPACE

struct BidMessage {
    int client_id;
    int amount;
};

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QWidget *parent = nullptr);
    ~Server();

    // Функція запуску сервера (викличемо її по кнопці Старт)
    void startServer();

signals:
    // Сигнали, щоб оновлювати GUI з інших потоків
    void logToGui(QString message);
    void updateHighestBid(int clientId, int amount);

private slots:
    // Слоти, які приймають сигнали і малюють на екрані
    void onLogReceived(QString message);
    void onUpdateBidReceived(int clientId, int amount);
    void on_startButton_clicked(); // Слот для кнопки у .ui файлі

private:
    Ui::Server *ui;

    // Мережеві змінні
    SOCKET server_fd;
    bool isRunning = false;

    // Змінні аукціону
    int max_bid = 0;
    int winner_id = -1;

    // Синхронізація
    std::queue<BidMessage> bid_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    // Потокові функції (тепер це методи класу)
    void serverAcceptLoop();      // Цикл прийому нових клієнтів
    void clientHandler(SOCKET clientSocket, int clientId); // Обробка клієнта
    void auctionLogicLoop();      // Логіка визначення переможця
};

#endif // SERVER_H
