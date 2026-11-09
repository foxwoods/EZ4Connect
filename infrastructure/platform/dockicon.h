#ifndef DOCKICON_H
#define DOCKICON_H

#include <QtGlobal>

namespace DockIcon
{
#ifdef Q_OS_MACOS
// 切换 NSApplicationActivationPolicy：Regular（显示 Dock 图标）/ Accessory（隐藏）
void setVisible(bool visible);
#else
inline void setVisible(bool) {}
#endif
}

#endif // DOCKICON_H
