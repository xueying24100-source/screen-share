#ifndef SENDERWINDOW_H
#define SENDERWINDOW_H

#include <QWidget>

namespace Ui {
class SenderWindow;
}

class SenderWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SenderWindow(QWidget *parent = nullptr);
    ~SenderWindow();

private:
    Ui::SenderWindow *ui;
};

#endif // SENDERWINDOW_H
