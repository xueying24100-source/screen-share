#ifndef TOOLBUTTON_H
#define TOOLBUTTON_H

#include <QPushButton>

class ToolButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ToolButton(const QString &iconText, const QString &label,
                        bool checkable = false, QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const;

private:
    void setupUI(const QString &iconText, const QString &label);
    void updateStyle();

    bool m_active = false;
};

#endif // TOOLBUTTON_H
