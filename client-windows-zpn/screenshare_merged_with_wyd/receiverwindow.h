#ifndef RECEIVERWINDOW_H
#define RECEIVERWINDOW_H

#include <QWidget>

namespace Ui {
class Receiverwindow;
}

class Receiverwindow : public QWidget
{
    Q_OBJECT

public:
    explicit Receiverwindow(QWidget *parent = nullptr);
    ~Receiverwindow();

private:
    Ui::Receiverwindow *ui;
};

#endif // RECEIVERWINDOW_H
