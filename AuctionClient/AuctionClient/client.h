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

private slots:
    void on_connectButton_clicked();
    void on_bidButton_clicked();
    void updateLog(QString msg);
    void updateTimerDisplay(QString timeStr);

private:
    Ui::Client *ui;

    SOCKET client_socket;
    bool isConnected = false;
    std::thread *receiverThread = nullptr;

    bool connectToServer(QString ip, int port, QString username);
    void receiveLoop();
};

#endif // CLIENT_H
