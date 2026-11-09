#include "infrastructure/platform/dockicon.h"

#import <AppKit/AppKit.h>

namespace DockIcon
{
void setVisible(bool visible)
{
    const NSApplicationActivationPolicy target = visible
        ? NSApplicationActivationPolicyRegular
        : NSApplicationActivationPolicyAccessory;

    if ([NSApp activationPolicy] == target)
    {
        return;
    }

    [NSApp setActivationPolicy:target];

    if (visible)
    {
        // Accessory -> Regular 后需重新激活，否则窗口不在前台且菜单栏不刷新
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    }
}
}
