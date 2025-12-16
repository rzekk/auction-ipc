#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <winsock2.h>
#include <thread>
#include <string>

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
    // Сигнал, щоб передати текст з фонового потоку в GUI
    void messageReceived(QString msg);

private slots:
    // Слоти кнопок
    void on_connectButton_clicked();
    void on_bidButton_clicked();

    // Слот для оновлення тексту на екрані
    void updateLog(QString msg);

private:
    Ui::Client *ui;

    SOCKET client_socket;
    bool isConnected = false;
    std::thread *receiverThread = nullptr;

    // Основна функція підключення
    bool connectToServer(QString ip, int port);

    // Функція, яка буде крутитися в окремому потоці
    void receiveLoop();
};

#endif // CLIENT_H
