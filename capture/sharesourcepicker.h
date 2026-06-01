#ifndef SHARESOURCEPICKER_H
#define SHARESOURCEPICKER_H

#include <QDialog>
#include <QPointer>

class QComboBox;
class QGridLayout;
class QPushButton;
class QTabWidget;

struct ShareSelection {
    enum class Kind { Screen, Window };

    Kind kind = Kind::Screen;
    int screenIndex = 0;
    quintptr hwnd = 0;
    int fps = 15;
};

class ShareSourcePicker : public QDialog
{
    Q_OBJECT

public:
    explicit ShareSourcePicker(QWidget *parent = nullptr);

    ShareSelection selection() const { return m_selection; }

private:
    void populateScreens();
    void populateWindows();
    void setSelectedScreen(int index);
    void setSelectedWindow(quintptr hwnd);
    void updateConfirmEnabled();

    QTabWidget *m_tabs = nullptr;
    QWidget *m_screenTab = nullptr;
    QWidget *m_windowTab = nullptr;
    QGridLayout *m_screenGrid = nullptr;
    QGridLayout *m_windowGrid = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QPushButton *m_startShareButton = nullptr;
    ShareSelection m_selection;
    QPointer<QWidget> m_selectedScreenWidget;
    QPointer<QWidget> m_selectedWindowWidget;
};

#endif // SHARESOURCEPICKER_H

