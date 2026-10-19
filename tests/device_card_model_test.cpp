#include <QFile>
#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QTimer>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() < 2) return 1;

    // Exercise production snapshot parsing, grouping, roles and delegate context.
    // All identities below are synthetic, not a live hardware dump.
    const QString snapshot = QStringLiteral(R"json([
        {"category":"hub","headline":"Example USB2 hub","vendorId":"1234","productId":"0001",
         "product":"Example USB2 hub","primaryDriver":"hub","usbVersion":"2.0",
         "deviceClass":"Hub","deviceSubclass":"0x00",
         "usb":{"isHub":true,"speed":480,"maxPowerMA":100,"serial":"example-usb2"},
         "properties":[["port.id","usb1-port1"],["port.peer_id","usb2-port1"],
                       ["usb_bos_container_id","example-container"],["transport.usb2","true"]]},
        {"category":"hub","headline":"Example USB3 hub","vendorId":"1234","productId":"0002",
         "product":"Example USB3 hub","primaryDriver":"hub","usbVersion":"3.0",
         "deviceClass":"Hub","deviceSubclass":"0x00",
         "usb":{"isHub":true,"speed":5000,"maxPowerMA":500,"serial":"example-usb3"},
         "properties":[["port.id","usb2-port1"],["port.peer_id","usb1-port1"],
                       ["usb_bos_container_id","example-container"],["transport.usb3","true"]]},
        {"category":"usb","headline":"Example USB device","vendorId":"1234","productId":"0003",
         "product":"Example USB device","primaryDriver":"example-driver","usbVersion":"2.0",
         "deviceClass":"Audio","deviceSubclass":"0x01",
         "usb":{"speed":12,"maxPowerMA":200,"serial":"example-child"},
         "properties":[["port.id","1-1-port4"],["product_db","Example database label"]]}
    ])json");
    QFile probe(app.arguments().at(1));
    if (!probe.open(QIODevice::ReadOnly)) return 4;
    auto source = probe.readAll();
    // Also permits exercising the installed/staged applet without installing it.
    if (app.arguments().size() > 2) {
        source.replace("\"../../src/plasmoid/contents/ui\"",
                       QByteArray("\"") + QUrl::fromLocalFile(app.arguments().at(2)).toEncoded() + '"');
    }
    bool qmlError = false;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&qmlError](const QList<QQmlError> &errors) {
        for (const auto &error : errors) {
            const auto message = error.description();
            if (message.contains(QStringLiteral("Error:")) || message.contains(QStringLiteral("Binding loop"))
                || message.contains(QStringLiteral("Unable to assign"))) qmlError = true;
        }
    });
    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QCoreApplication::quit);
    QObject::connect(&engine, &QQmlApplicationEngine::exit, &app, &QCoreApplication::exit);
    QTimer::singleShot(15000, &app, [] { QCoreApplication::exit(5); });
    engine.loadData(source, QUrl::fromLocalFile(probe.fileName()));
    if (engine.rootObjects().isEmpty()) return 6;
    // The QML import creates DeviceModel from the real plugin (no duplicate
    // test implementation). Deliver deterministic data through its real client.
    auto *model = engine.rootObjects().first()->findChild<QObject *>(QStringLiteral("testDeviceModel"));
    if (!model) return 2;
    QObject *client = nullptr;
    for (auto *child : model->children())
        if (QByteArray(child->metaObject()->className()) == "USBeeKeeper::UsbeehiveClient") client = child;
    if (!client || !QMetaObject::invokeMethod(client, "snapshotReady", Qt::DirectConnection,
                                             Q_ARG(QString, snapshot))) return 3;
    if (model->property("count").toInt() != 2) return 7;
    auto *rows = qobject_cast<QAbstractItemModel *>(model);
    if (!rows) return 8;
    const auto keyRole = rows->roleNames().key(QByteArrayLiteral("physicalInstanceKey"), -1);
    const auto key = rows->data(rows->index(0, 0), keyRole).toString();
    if (key.isEmpty()) return 9;
    model->setProperty("userAliasesJson", QString::fromUtf8(QJsonDocument(QJsonObject{
        {key, QStringLiteral("Example physical alias")}}).toJson(QJsonDocument::Compact)));
    const auto result = app.exec();
    return qmlError ? 10 : result;
}
