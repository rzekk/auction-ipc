#ifndef SERVER_H
#define SERVER_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class Server;
}
QT_END_NAMESPACE

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QWidget *parent = nullptr);
    ~Server();

private:
    Ui::Server *ui;
};
#endif // SERVER_H
