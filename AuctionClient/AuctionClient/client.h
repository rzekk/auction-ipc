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
    // Сигнали для передачі даних з потоку в GUI
    void messageReceived(QString msg);
    void timerUpdated(QString timeStr);
    void updateLotInfo(QString lotName, QString startPrice);
    void updateBidInfo(QString newPrice);

private slots:
    void on_connectButton_clicked();

    // ВАЖЛИВО: Назва має бути exactly on_betButton_clicked, бо кнопка в UI називається betButton
    void on_betButton_clicked();

    void updateLog(QString msg);
    void updateTimerDisplay(QString timeStr);

    // Слоти для оновлення полів аукціону
    void onUpdateLot(QString lotName, QString startPrice);
    void onUpdateBid(QString newPrice);

private:
    Ui::Client *ui;

    SOCKET client_socket;
    bool isConnected = false;
    std::thread *receiverThread = nullptr;

    bool connectToServer(QString ip, int port, QString username);
    void receiveLoop();
};

#endif // CLIENT_H
