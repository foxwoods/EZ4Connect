#ifndef DOCKICONVISIBILITYCONTROLLER_H
#define DOCKICONVISIBILITYCONTROLLER_H

#include <QObject>

// 监听顶层窗口的显示/隐藏：存在可见的普通窗口或对话框时显示 Dock 图标，
// 全部隐藏（仅剩系统托盘）时隐藏 Dock 图标。仅在 macOS 上有实际效果。
class DockIconVisibilityController : public QObject
{
public:
    explicit DockIconVisibilityController(QObject *parent = nullptr);

    void update();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool updateScheduled = false;
};

#endif // DOCKICONVISIBILITYCONTROLLER_H
