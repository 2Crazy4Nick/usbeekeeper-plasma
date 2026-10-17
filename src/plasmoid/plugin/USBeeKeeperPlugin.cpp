#include "USBeeKeeperPlugin.h"
#include "DeviceModel.h"
#include <qqml.h>

void USBeeKeeperPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("io.github._2crazy4nick.usbeekeeper"));
    qmlRegisterType<USBeeKeeper::DeviceModel>(uri, 1, 0, "DeviceModel");
}
