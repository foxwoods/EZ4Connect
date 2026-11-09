#include "dockiconvisibilitycontroller.h"

#include <QApplication>
#include <QEvent>
#include <QTimer>
#include <QWidget>

#include "infrastructure/platform/dockicon.h"

DockIconVisibilityController::DockIconVisibilityController(QObject *parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
}

bool DockIconVisibilityController::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) &&
        watched->isWidgetType() &&
        static_cast<QWidget *>(watched)->isWindow() &&
        !updateScheduled)
    {
        // 合并同一轮事件循环内的多次显示/隐藏
        updateScheduled = true;
        QTimer::singleShot(0, this, [this]() {
            updateScheduled = false;
            update();
        });
    }

    return QObject::eventFilter(watched, event);
}

void DockIconVisibilityController::update()
{
    bool hasVisibleWindow = false;
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets)
    {
        const Qt::WindowType type = widget->windowType();
        // 排除托盘菜单（Popup）、ToolTip 等
        if ((type == Qt::Window || type == Qt::Dialog) && widget->isVisible())
        {
            hasVisibleWindow = true;
            break;
        }
    }

    DockIcon::setVisible(hasVisibleWindow);
}
