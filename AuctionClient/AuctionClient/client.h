#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <winsock2.h>
#include <thread>
#include <string>
#include <QLineEdit>

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{
    Q_OBJECT

public:
    Client(QWidget *parent = nullptr);
    ~Client();

signals:
    void messageReceived(QString msg);
    void timerUpdated(QString timeStr);
    void updateLotInfo(QString lotName, QString startPrice);
    void updateBidInfo(QString newPrice);

private slots:
    void on_connectButton_clicked();
    void on_betButton_clicked(); // Кнопка ставки

    void updateLog(QString msg);
    void updateTimerDisplay(QString timeStr);

    void onUpdateLot(QString lotName, QString startPrice);
    void onUpdateBid(QString newPrice);

private:
    Ui::Client *ui;

    SOCKET client_socket;
    bool isConnected = false;

    // === НОВА ЗМІННА ===
    bool auctionActive = false; // Чи йде зараз активний торг?

    std::thread *receiverThread = nullptr;

    bool connectToServer(QString ip, int port, QString username);
    void receiveLoop();
};

#endif // CLIENT_H
