/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "earcontroller.h"
#include "bluezprofile.h"
#include "devicemodel.h"
#include "formfactor.h"
#include "modelinfo.h"
#include "nothingprotocol.h"
#include "rfcommchannel.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDebug>
#include <QStringList>
#include <QVariantMap>

#include <unistd.h>

namespace {
const char *const KeyAddress = "device/address";
const char *const KeyName = "device/name";
const char *const KeyPath = "device/path";
const char *const KeyModelName = "device/modelName";
const char *const KeyAncSupported = "device/ancSupported";
const char *const KeyAutoConnect = "connection/autoConnectAudio";
const char *const KeySpatialAudio = "device/spatialAudio";
const char *const KeyStrength = "anc/strength";
const char *const KeyChannel = "connection/manualChannel";

// The earbuds need a moment after the audio profiles came up before the
// control channel accepts connections.
const int ConnectDelayMs = 1500;
// Give up if BlueZ does not hand over a connection in time.
const int ConnectTimeoutMs = 20000;
// After a write, ask the buds for the actual mode to confirm it.
const int ConfirmDelayMs = 400;
// The buds push battery changes themselves; polling is only a safety net.
const int BatteryPollMs = 60000;
// Models without noise control never answer the ANC request.
const int AncProbeMs = 5000;
// Paging earbuds that are in the closed case takes a while to fail.
const int AudioConnectTimeoutMs = 30000;
// Spacing between the state requests sent after connecting.
const int QueueIntervalMs = 80;
// Re-read gestures after a change to show what the buds accepted.
const int GestureRefreshMs = 400;
// The fit test plays a sound and measures; give it some time.
const int FitTimeoutMs = 20000;
// Re-read the device list after a change; also used after enabling.
const int DualRefreshMs = 400;
// After ConnectProfile succeeded, BlueZ hands the socket over right away.
// If it does not, another app's profile got it.
const int HandoverTimeoutMs = 4000;
// Right after the app starts or the buds connect, BlueZ is sometimes not ready
// for the control channel yet; try a few times before showing an error.
const int RetryDelayMs = 3000;
const int AutoRetries = 6;
const int WatchdogMs = 15000;
const int WatchdogTries = 4;
// Safety limit for paging through the remembered devices.
const int DualMaxPages = 20;

QString macToString(const QByteArray &mac)
{
    QStringList parts;
    for (int i = 0; i < mac.size(); ++i)
        parts.append(QString::number(quint8(mac.at(i)), 16).rightJustified(2, QLatin1Char('0')).toUpper());
    return parts.join(QLatin1Char(':'));
}

QByteArray macFromString(const QString &text)
{
    QByteArray mac;
    for (const QString &part : text.split(QLatin1Char(':')))
        mac.append(char(part.toUInt(nullptr, 16)));
    return mac;
}

int gestureKey(int side, int type)
{
    return (side << 8) | type;
}
}

EarController::EarController(QObject *parent)
    : QObject(parent)
    , m_profile(new BluezProfile(this))
    , m_channel(nullptr)
    , m_operationId(0)
    , m_state(Disconnected)
    , m_mode(ModeUnknown)
    , m_strength(StrengthHigh)
    , m_manualChannel(0)
    , m_systemConnected(false)
    , m_batteryLeft(-1)
    , m_batteryRight(-1)
    , m_batteryCase(-1)
    , m_chargingLeft(false)
    , m_chargingRight(false)
    , m_chargingCase(false)
    , m_batterySingle(-1)
    , m_chargingSingle(false)
    , m_ancSupported(true)
    , m_ancSeen(false)
    , m_audioConnecting(false)
    , m_autoConnectAudio(true)
    , m_autoConnectTried(false)
    , m_modelBase(modelinfo::Unknown)
    , m_seenInEar(false)
    , m_seenLatency(false)
    , m_seenSuperMic(false)
    , m_seenPersonalizedAnc(false)
    , m_seenEnhancedBass(false)
    , m_seenGestures(false)
    , m_inEar(false)
    , m_lowLatency(false)
    , m_superMic(false)
    , m_personalizedAnc(false)
    , m_enhancedBass(false)
    , m_bassLevel(3)
    , m_spatialAudio(false)
    , m_seenDual(false)
    , m_dualEnabled(false)
    , m_dualPages(0)
    , m_retriesLeft(0)
    , m_watchdogTries(0)
    , m_userDisconnected(false)
    , m_ringingLeft(false)
    , m_ringingRight(false)
    , m_fitState(FitIdle)
    , m_fitLeft(-1)
    , m_fitRight(-1)
{
    m_address = m_settings.value(KeyAddress).toString();
    m_name = m_settings.value(KeyName).toString();
    m_path = m_settings.value(KeyPath).toString();
    m_strength = qBound(int(StrengthLow), m_settings.value(KeyStrength, int(StrengthHigh)).toInt(),
                        int(StrengthAdaptive));
    m_manualChannel = m_settings.value(KeyChannel, 0).toInt();
    m_modelName = m_settings.value(KeyModelName).toString();
    m_ancSupported = m_settings.value(KeyAncSupported, formFactor() != FormOpenEar).toBool();
    m_autoConnectAudio = m_settings.value(KeyAutoConnect, true).toBool();
    m_spatialAudio = m_settings.value(KeySpatialAudio, false).toBool();
    updateModelBase();

    connect(m_profile, &BluezProfile::connectionReady, this, &EarController::onProfileConnection);
    connect(m_profile, &BluezProfile::disconnectionRequested,
            this, &EarController::onProfileDisconnectionRequested);

    m_connectDelay.setSingleShot(true);
    m_connectDelay.setInterval(ConnectDelayMs);
    connect(&m_connectDelay, &QTimer::timeout, this, &EarController::openConnection);

    m_connectTimeout.setSingleShot(true);
    m_connectTimeout.setInterval(ConnectTimeoutMs);
    connect(&m_connectTimeout, &QTimer::timeout, this, &EarController::onConnectTimeout);

    m_confirmTimer.setSingleShot(true);
    m_confirmTimer.setInterval(ConfirmDelayMs);
    connect(&m_confirmTimer, &QTimer::timeout, this, &EarController::requestAnc);

    m_batteryPoll.setInterval(BatteryPollMs);
    connect(&m_batteryPoll, &QTimer::timeout, this, &EarController::requestBattery);

    m_ancProbe.setSingleShot(true);
    m_ancProbe.setInterval(AncProbeMs);
    connect(&m_ancProbe, &QTimer::timeout, this, &EarController::onAncProbeTimeout);

    m_queueTimer.setInterval(QueueIntervalMs);
    connect(&m_queueTimer, &QTimer::timeout, this, &EarController::onQueueTimer);

    m_gestureRefresh.setSingleShot(true);
    m_gestureRefresh.setInterval(GestureRefreshMs);
    connect(&m_gestureRefresh, &QTimer::timeout, this, [this]() {
        queueRequest(nothing::cmd::RequestGestures);
    });

    m_handoverTimer.setSingleShot(true);
    m_handoverTimer.setInterval(HandoverTimeoutMs);
    connect(&m_handoverTimer, &QTimer::timeout, this, &EarController::onHandoverTimeout);

    m_dualRefresh.setSingleShot(true);
    m_dualRefresh.setInterval(DualRefreshMs);
    connect(&m_dualRefresh, &QTimer::timeout, this, &EarController::refreshDualDevices);

    m_fitTimeout.setSingleShot(true);
    m_fitTimeout.setInterval(FitTimeoutMs);
    connect(&m_fitTimeout, &QTimer::timeout, this, &EarController::onFitTimeout);

    // Safety net: the system has the earbuds connected but the control
    // channel is not open and nothing is pending - try again a few times.
    m_watchdog.setInterval(WatchdogMs);
    connect(&m_watchdog, &QTimer::timeout, this, &EarController::onWatchdog);
    m_watchdog.start();

    // Register the profile right away, so BlueZ knows it before the first
    // ConnectProfile (helps when the app starts with the earbuds connected).
    QTimer::singleShot(0, this, [this]() {
        if (m_manualChannel == 0)
            m_profile->ensureRegistered();
        checkStartupState();
    });
}

// Asks BlueZ directly whether the remembered earbuds are connected, instead
// of relying only on the device list's first report.
void EarController::checkStartupState()
{
    if (m_address.isEmpty()) {
        qDebug() << "Startup: no earbuds chosen yet";
        return;
    }
    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                                       QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                                       QStringLiteral("GetManagedObjects"));
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<ManagedObjectList> reply = *w;
        if (reply.isError()) {
            qWarning() << "Startup: BlueZ not reachable:" << reply.error().message();
            return;
        }
        const ManagedObjectList objects = reply.value();
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
            const QVariantMap props = it.value().value(QStringLiteral("org.bluez.Device1"));
            if (props.value(QStringLiteral("Address")).toString().compare(m_address, Qt::CaseInsensitive) != 0)
                continue;
            const bool connected = props.value(QStringLiteral("Connected")).toBool();
            qDebug() << "Startup:" << m_address << "found at" << it.key().path()
                     << (connected ? "(connected)" : "(not connected)");
            onSystemConnectionChanged(m_address, it.key().path(),
                                      props.value(QStringLiteral("Name")).toString(), connected);
            return;
        }
        qWarning() << "Startup: remembered earbuds" << m_address << "not found in BlueZ";
    });
}

void EarController::onWatchdog()
{
    if (m_address.isEmpty() || !m_systemConnected || m_userDisconnected
        || m_state != Disconnected || m_connectDelay.isActive() || m_audioConnecting
        || m_watchdogTries >= WatchdogTries)
        return;
    ++m_watchdogTries;
    qDebug() << "Earbuds connected but no control channel, trying again (" << m_watchdogTries
             << "of" << WatchdogTries << ")";
    m_retriesLeft = 1;
    openConnection();
}

void EarController::ensureConnected()
{
    if (m_address.isEmpty() || !m_systemConnected || m_userDisconnected
        || m_state != Disconnected || m_connectDelay.isActive() || m_audioConnecting)
        return;
    qDebug() << "App active, earbuds connected: opening the control channel";
    m_watchdogTries = 0;
    m_retriesLeft = AutoRetries;
    openConnection();
}

EarController::~EarController()
{
    closeConnection();
}

void EarController::setManualChannel(int channel)
{
    channel = qBound(0, channel, 30);
    if (channel == m_manualChannel)
        return;
    m_manualChannel = channel;
    m_settings.setValue(KeyChannel, channel);
    emit manualChannelChanged();
}

void EarController::selectDevice(const QString &address, const QString &name,
                                 const QString &modelName, const QString &path,
                                 bool systemConnected)
{
    const bool newDevice = address != m_address;
    m_userDisconnected = false;
    m_watchdogTries = 0;
    if (newDevice) {
        closeConnection();
        m_address = address;
        m_mode = ModeUnknown;
        emit ancChanged();
    }
    m_name = name;
    m_modelName = modelName.isEmpty() ? name : modelName;
    if (!path.isEmpty())
        m_path = path;
    m_settings.setValue(KeyAddress, m_address);
    m_settings.setValue(KeyName, m_name);
    m_settings.setValue(KeyModelName, m_modelName);
    m_settings.setValue(KeyPath, m_path);
    emit deviceChanged();

    if (newDevice) {
        // First guess from the model; the buds' answer corrects it.
        m_settings.remove(KeyAncSupported);
        setAncSupported(formFactor() != FormOpenEar);
        m_settings.remove(KeySpatialAudio);
        m_spatialAudio = false;
        m_serial.clear();
        m_serialSku.clear();
        m_firmware.clear();
    }
    updateModelBase();

    setSystemConnected(systemConnected);
    setErrorString(QString());
    if (systemConnected)
        openConnection();
    else
        connectAudio();
}

int EarController::formFactor() const
{
    return formfactor::fromName(m_modelName.isEmpty() ? m_name : m_modelName);
}

void EarController::setModelName(const QString &modelName)
{
    if (modelName.isEmpty() || modelName == m_modelName)
        return;
    const bool hadModel = !m_modelName.isEmpty();
    m_modelName = modelName;
    m_settings.setValue(KeyModelName, m_modelName);
    emit deviceChanged();
    updateModelBase();
    // Settings from before 0.3 have no model name yet: make the first guess now.
    if (!hadModel && !m_settings.contains(KeyAncSupported))
        setAncSupported(formFactor() != FormOpenEar);
}

void EarController::setAutoConnectAudio(bool enabled)
{
    if (enabled == m_autoConnectAudio)
        return;
    m_autoConnectAudio = enabled;
    m_settings.setValue(KeyAutoConnect, enabled);
    emit autoConnectAudioChanged();
}

void EarController::setSystemConnected(bool connected)
{
    if (connected == m_systemConnected)
        return;
    m_systemConnected = connected;
    emit systemConnectedChanged();
}

void EarController::setAncSupported(bool supported)
{
    if (!m_settings.contains(KeyAncSupported)
        || m_settings.value(KeyAncSupported).toBool() != supported)
        m_settings.setValue(KeyAncSupported, supported);
    if (supported == m_ancSupported)
        return;
    m_ancSupported = supported;
    emit ancSupportedChanged();
    emit featuresChanged(); // personalized ANC depends on it
}

void EarController::connectNow()
{
    if (m_address.isEmpty())
        return;
    m_userDisconnected = false;
    m_watchdogTries = 0;
    setErrorString(QString());
    if (m_systemConnected)
        reconnect();
    else
        connectAudio();
}

void EarController::connectAudio()
{
    if (m_address.isEmpty() || m_audioConnecting || m_systemConnected)
        return;

    qDebug() << "Connecting audio of" << devicePath();
    m_audioConnecting = true;
    emit audioConnectingChanged();

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), devicePath(),
                                                       QStringLiteral("org.bluez.Device1"),
                                                       QStringLiteral("Connect"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(call, AudioConnectTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &EarController::onAudioConnectFinished);
}

void EarController::onAudioConnectFinished(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    m_audioConnecting = false;
    emit audioConnectingChanged();

    QDBusPendingReply<> reply = *watcher;
    if (!reply.isError()) {
        // The Connected property change starts the control channel.
        qDebug() << "Audio connected";
        return;
    }

    const QString name = reply.error().name();
    const QString message = reply.error().message();
    qWarning() << "Device1.Connect failed:" << name << message;
    if (name == QLatin1String("org.bluez.Error.AlreadyConnected")
        || name == QLatin1String("org.bluez.Error.InProgress"))
        return;

    if (message.contains(QLatin1String("page-timeout"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("host is down"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("timeout"), Qt::CaseInsensitive)
        || name == QLatin1String("org.freedesktop.DBus.Error.NoReply")) {
        setErrorString(tr("Your earbuds are not reachable. Take them out of the case and try again."));
    } else {
        setErrorString(tr("Could not connect your earbuds: %1").arg(message));
    }
}

bool EarController::retryLater(const char *reason)
{
    if (!m_systemConnected || m_retriesLeft <= 0)
        return false;
    --m_retriesLeft;
    qDebug() << "Control channel failed (" << reason << "), retrying in" << RetryDelayMs
             << "ms," << m_retriesLeft << "retries left";
    closeConnection();
    setErrorString(QString());
    m_connectDelay.start(RetryDelayMs);
    return true;
}

void EarController::reconnect()
{
    if (m_address.isEmpty())
        return;
    m_retriesLeft = 1;
    closeConnection();
    openConnection();
}

void EarController::disconnectDevice()
{
    m_userDisconnected = true;
    m_connectDelay.stop();
    closeConnection();
}

void EarController::onSystemConnectionChanged(const QString &address, const QString &path,
                                              const QString &modelName, bool connected)
{
    if (address.compare(m_address, Qt::CaseInsensitive) != 0)
        return;

    qDebug() << "System reports" << address << (connected ? "connected" : "disconnected")
             << "- control channel state" << m_state;
    if (!path.isEmpty() && path != m_path) {
        m_path = path;
        m_settings.setValue(KeyPath, m_path);
    }
    setModelName(modelName);
    m_userDisconnected = false;
    m_watchdogTries = 0;

    setSystemConnected(connected);
    if (connected) {
        m_autoConnectTried = true;
        setErrorString(QString());
        m_retriesLeft = AutoRetries;
        if (m_state == Disconnected) {
            qDebug() << "Earbuds connected, opening the control channel";
            m_connectDelay.start(ConnectDelayMs);
        }
    } else if (!m_autoConnectTried) {
        // First report after the app started: the earbuds are not connected.
        m_autoConnectTried = true;
        if (m_autoConnectAudio)
            connectAudio();
    } else {
        m_connectDelay.stop();
        closeConnection();
        setErrorString(QString());
    }
}

QString EarController::devicePath() const
{
    if (!m_path.isEmpty())
        return m_path;
    // Fallback for the default adapter.
    QString suffix = m_address.toUpper();
    suffix.replace(QLatin1Char(':'), QLatin1Char('_'));
    return QStringLiteral("/org/bluez/hci0/dev_") + suffix;
}

RfcommChannel *EarController::createChannel()
{
    RfcommChannel *channel = new RfcommChannel(this);
    connect(channel, &RfcommChannel::connected, this, &EarController::onChannelConnected);
    connect(channel, &RfcommChannel::failed, this, &EarController::onChannelFailed);
    connect(channel, &RfcommChannel::closed, this, &EarController::onChannelClosed);
    connect(channel, &RfcommChannel::dataReceived, this, &EarController::onDataReceived);
    return channel;
}

void EarController::openConnection()
{
    if (m_address.isEmpty() || m_state != Disconnected)
        return;

    setErrorString(QString());
    m_buffer.clear();
    setState(Connecting);
    m_connectTimeout.start();

    if (m_manualChannel > 0) {
        qDebug() << "Connecting to" << m_address << "on RFCOMM channel" << m_manualChannel;
        m_channel = createChannel();
        m_channel->connectToChannel(m_address, m_manualChannel);
        return;
    }

    if (!m_profile->ensureRegistered()) {
        closeConnection();
        setErrorString(tr("Connection failed: %1").arg(m_profile->lastError()));
        return;
    }

    qDebug() << "ConnectProfile on" << devicePath();
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), devicePath(),
                                                       QStringLiteral("org.bluez.Device1"),
                                                       QStringLiteral("ConnectProfile"));
    call << QString::fromLatin1(nothing::SppServiceUuid);
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, ConnectTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &EarController::onConnectProfileFinished);
}

void EarController::onConnectProfileFinished(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    QDBusPendingReply<> reply = *watcher;
    if (m_state != Connecting)
        return;
    if (!reply.isError()) {
        // The socket arrives through NewConnection; watch that it does.
        qDebug() << "ConnectProfile succeeded, waiting for NewConnection";
        m_handoverTimer.start();
        return;
    }

    const QString name = reply.error().name();
    const QString message = reply.error().message();
    qWarning() << "ConnectProfile failed:" << name << message;

    if (name == QLatin1String("org.bluez.Error.InProgress")) {
        // Usually the audio connection is still being set up; BlueZ does
        // not queue our request, so ask again shortly.
        if (retryLater("busy"))
            return;
    }
    if (name == QLatin1String("org.bluez.Error.AlreadyConnected")) {
        // BlueZ thinks the channel is open (e.g. left over from an earlier
        // run). Drop it and connect again.
        QDBusMessage drop = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), devicePath(),
                                                           QStringLiteral("org.bluez.Device1"),
                                                           QStringLiteral("DisconnectProfile"));
        drop << QString::fromLatin1(nothing::SppServiceUuid);
        QDBusConnection::systemBus().asyncCall(drop);
        if (retryLater("already connected"))
            return;
    }

    QString text;
    if (message.contains(QLatin1String("profile-unavailable"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("not available"), Qt::CaseInsensitive)
        || name == QLatin1String("org.bluez.Error.DoesNotExist")) {
        text = tr("The control service was not found. Try again, "
                  "or set an RFCOMM channel in the settings.");
    } else if (message.contains(QLatin1String("host is down"), Qt::CaseInsensitive)
               || message.contains(QLatin1String("page-timeout"), Qt::CaseInsensitive)
               || name == QLatin1String("org.bluez.Error.NotConnected")) {
        text = tr("The earbuds are not reachable.");
    } else {
        text = tr("Connection failed: %1").arg(message);
    }
    if (retryLater("ConnectProfile error"))
        return;
    closeConnection();
    setErrorString(text);
}

void EarController::onProfileConnection(const QString &devicePath, int fd)
{
    if (devicePath != this->devicePath() || m_state == Connected) {
        qDebug() << "Ignoring connection for" << devicePath;
        ::close(fd);
        return;
    }

    m_connectTimeout.stop();
    m_handoverTimer.stop();
    if (m_channel) {
        m_channel->disconnect(this);
        m_channel->deleteLater();
    }
    m_channel = createChannel();
    m_channel->adopt(fd);
    onChannelConnected();
}

void EarController::onProfileDisconnectionRequested(const QString &devicePath)
{
    if (devicePath == this->devicePath())
        closeConnection();
}

void EarController::onChannelConnected()
{
    qDebug() << "Control channel connected";
    m_connectTimeout.stop();
    setState(Connected);
    m_ancSeen = false;
    requestAnc();
    requestBattery();
    m_batteryPoll.start();
    m_ancProbe.start();
    requestDeviceState();
}

void EarController::onAncProbeTimeout()
{
    if (m_state != Connected || m_ancSeen)
        return;
    qDebug() << "No ANC report, treating the earbuds as without noise control";
    setAncSupported(false);
}

void EarController::onChannelFailed(const QString &error)
{
    qWarning() << "Channel connect failed:" << error;
    if (retryLater("socket connect failed"))
        return;
    closeConnection();
    setErrorString(tr("Connection failed: %1").arg(error));
}

void EarController::onChannelClosed()
{
    qDebug() << "Control channel closed";
    closeConnection();
}

void EarController::onConnectTimeout()
{
    if (m_state != Connecting)
        return;
    if (retryLater("timeout"))
        return;
    closeConnection();
    setErrorString(tr("Connection failed: %1").arg(tr("timed out")));
}

void EarController::onHandoverTimeout()
{
    if (m_state != Connecting)
        return;
    qWarning() << "No NewConnection after ConnectProfile - the socket went elsewhere";
    if (retryLater("no NewConnection"))
        return;
    closeConnection();
    setErrorString(tr("The control channel went to another app. Close other earbud apps (for example Sailbuds), then connect again."));
}

void EarController::closeConnection()
{
    m_confirmTimer.stop();
    m_connectTimeout.stop();
    m_handoverTimer.stop();
    m_batteryPoll.stop();
    m_ancProbe.stop();
    m_queueTimer.stop();
    m_requestQueue.clear();
    m_gestureRefresh.stop();
    m_dualRefresh.stop();
    applyBattery(-1, false, -1, false, -1, false);
    resetFeatures();
    if (m_channel) {
        RfcommChannel *channel = m_channel;
        m_channel = nullptr;
        channel->disconnect(this);
        channel->close();
        channel->deleteLater();
    }
    m_buffer.clear();
    setState(Disconnected);
}

void EarController::onDataReceived(const QByteArray &data)
{
    m_buffer.insert(m_buffer.end(), data.constBegin(), data.constEnd());

    nothing::Packet packet;
    for (;;) {
        const nothing::ParseResult result = nothing::tryParse(m_buffer, packet);
        if (result == nothing::ParseResult::NeedMoreData)
            break;
        if (result == nothing::ParseResult::CrcError) {
            qWarning() << "Dropped packet with bad CRC";
            continue;
        }
        quint8 code = 0;
        nothing::BatteryStatus battery;
        if (handleFeaturePacket(packet)) {
            // handled
        } else if (nothing::parseAnc(packet, code)) {
            m_ancSeen = true;
            setAncSupported(true);
            applyAncCode(code);
        } else if (nothing::parseBattery(packet, battery)) {
            applyBattery(battery.left.present ? battery.left.percent : -1, battery.left.charging,
                         battery.right.present ? battery.right.percent : -1, battery.right.charging,
                         battery.caseBattery.present ? battery.caseBattery.percent : -1,
                         battery.caseBattery.charging,
                         battery.single.present ? battery.single.percent : -1,
                         battery.single.charging);
        }
    }
}

void EarController::applyAncCode(quint8 code)
{
    AncMode mode = ModeUnknown;
    int strength = m_strength;

    switch (code) {
    case nothing::AncOff:          mode = ModeOff; break;
    case nothing::AncTransparency: mode = ModeTransparency; break;
    case nothing::AncLow:          mode = ModeNoiseCancellation; strength = StrengthLow; break;
    case nothing::AncMid:          mode = ModeNoiseCancellation; strength = StrengthMid; break;
    case nothing::AncHigh:         mode = ModeNoiseCancellation; strength = StrengthHigh; break;
    case nothing::AncAdaptive:     mode = ModeNoiseCancellation; strength = StrengthAdaptive; break;
    default:
        return;
    }

    if (strength != m_strength)
        m_settings.setValue(KeyStrength, strength);

    if (mode != m_mode || strength != m_strength) {
        m_mode = mode;
        m_strength = strength;
        emit ancChanged();
    }
}

void EarController::applyBattery(int left, bool chargingLeft, int right, bool chargingRight,
                                 int caseLevel, bool chargingCase,
                                 int single, bool chargingSingle)
{
    if (left == m_batteryLeft && right == m_batteryRight && caseLevel == m_batteryCase
        && chargingLeft == m_chargingLeft && chargingRight == m_chargingRight
        && chargingCase == m_chargingCase && single == m_batterySingle
        && chargingSingle == m_chargingSingle)
        return;
    m_batterySingle = single;
    m_chargingSingle = single >= 0 && chargingSingle;
    m_batteryLeft = left;
    m_batteryRight = right;
    m_batteryCase = caseLevel;
    m_chargingLeft = left >= 0 && chargingLeft;
    m_chargingRight = right >= 0 && chargingRight;
    m_chargingCase = caseLevel >= 0 && chargingCase;
    emit batteryChanged();
}

void EarController::setAncMode(int mode)
{
    switch (mode) {
    case ModeOff:
        sendAncCode(nothing::AncOff);
        break;
    case ModeTransparency:
        sendAncCode(nothing::AncTransparency);
        break;
    case ModeNoiseCancellation:
        sendAncCode(strengthCode(m_strength));
        break;
    default:
        break;
    }
}

void EarController::setAncStrength(int strength)
{
    strength = qBound(int(StrengthLow), strength, int(StrengthAdaptive));
    if (strength != m_strength) {
        m_strength = strength;
        m_settings.setValue(KeyStrength, strength);
        emit ancChanged();
    }
    // Choosing a strength implies noise cancellation.
    sendAncCode(strengthCode(strength));
}

void EarController::cycleMode()
{
    // Same order as the pinch gesture on the buds.
    switch (m_mode) {
    case ModeNoiseCancellation: setAncMode(ModeTransparency); break;
    case ModeTransparency:      setAncMode(ModeOff); break;
    default:                    setAncMode(ModeNoiseCancellation); break;
    }
}

void EarController::requestAnc()
{
    send(nothing::cmd::RequestAnc, std::vector<quint8>());
}

void EarController::requestBattery()
{
    send(nothing::cmd::RequestBattery, std::vector<quint8>());
}

void EarController::sendAncCode(quint8 code)
{
    if (m_state != Connected || !m_ancSupported)
        return;
    send(nothing::cmd::SetAnc, nothing::setAncPayload(code));
    // Update optimistically, the confirmation read corrects it if needed.
    applyAncCode(code);
    m_confirmTimer.start();
}

void EarController::send(quint16 command, const std::vector<quint8> &payload)
{
    if (!m_channel || m_state != Connected)
        return;
    const std::vector<quint8> packet = nothing::encode(command, nextOperationId(), payload);
    m_channel->write(QByteArray(reinterpret_cast<const char *>(packet.data()), int(packet.size())));
}

quint8 EarController::nextOperationId()
{
    m_operationId = (m_operationId >= 250) ? 1 : quint8(m_operationId + 1);
    return m_operationId;
}

quint8 EarController::strengthCode(int strength) const
{
    switch (strength) {
    case StrengthLow:      return nothing::AncLow;
    case StrengthMid:      return nothing::AncMid;
    case StrengthAdaptive: return nothing::AncAdaptive;
    case StrengthHigh:
    default:               return nothing::AncHigh;
    }
}

void EarController::setState(ConnectionState state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit connectionStateChanged();
}

void EarController::setErrorString(const QString &error)
{
    if (error == m_errorString)
        return;
    m_errorString = error;
    emit errorStringChanged();
}

// ---------------------------------------------------------------------------
// Model detection and extra features

void EarController::updateModelBase()
{
    int base = modelinfo::fromSku(m_serialSku);
    if (base == modelinfo::Unknown)
        base = modelinfo::fromName(m_modelName.isEmpty() ? m_name : m_modelName);
    if (base == m_modelBase)
        return;
    m_modelBase = base;
    emit infoChanged();
    emit featuresChanged();
    emit gesturesChanged();
}

QString EarController::detectedModel() const
{
    const QString name = modelinfo::displayName(modelinfo::Base(m_modelBase));
    return name.isEmpty() ? m_modelName : name;
}

namespace {
// Readable features: known models use the table, unknown ones may try.
bool maybe(int base, bool (*capable)(modelinfo::Base))
{
    return base == modelinfo::Unknown || capable(modelinfo::Base(base));
}
}

bool EarController::hasInEarDetection() const
{
    return m_seenInEar && maybe(m_modelBase, modelinfo::inEarDetection);
}

bool EarController::hasSuperMic() const
{
    return m_seenSuperMic && maybe(m_modelBase, modelinfo::superMic);
}

bool EarController::hasPersonalizedAnc() const
{
    return m_seenPersonalizedAnc && m_ancSupported
        && maybe(m_modelBase, modelinfo::personalizedAnc);
}

bool EarController::hasEnhancedBass() const
{
    return m_seenEnhancedBass && maybe(m_modelBase, modelinfo::enhancedBass);
}

bool EarController::hasSpatialAudio() const
{
    // Write-only: only offered for models where it is verified.
    return m_state == Connected && modelinfo::spatialAudio(modelinfo::Base(m_modelBase));
}

bool EarController::hasFitTest() const
{
    return m_state == Connected && modelinfo::fitTest(modelinfo::Base(m_modelBase));
}

bool EarController::bassSpatialExclusive() const
{
    return modelinfo::bassSpatialExclusive(modelinfo::Base(m_modelBase));
}

void EarController::queueRequest(quint16 command)
{
    if (m_state != Connected || m_requestQueue.contains(command))
        return;
    m_requestQueue.append(command);
    if (!m_queueTimer.isActive())
        m_queueTimer.start();
}

void EarController::onQueueTimer()
{
    if (m_requestQueue.isEmpty() || m_state != Connected) {
        m_queueTimer.stop();
        return;
    }
    send(m_requestQueue.takeFirst(), std::vector<quint8>());
}

void EarController::requestDeviceState()
{
    const int base = m_modelBase;
    queueRequest(nothing::cmd::RequestSerial);
    queueRequest(nothing::cmd::RequestFirmware);
    queueRequest(nothing::cmd::RequestLatency);
    queueRequest(nothing::cmd::RequestGestures);
    if (maybe(base, modelinfo::inEarDetection))
        queueRequest(nothing::cmd::RequestInEar);
    if (maybe(base, modelinfo::personalizedAnc))
        queueRequest(nothing::cmd::RequestPersonalizedAnc);
    if (maybe(base, modelinfo::enhancedBass))
        queueRequest(nothing::cmd::RequestEnhancedBass);
    if (maybe(base, modelinfo::superMic))
        queueRequest(nothing::cmd::RequestSuperMic);
    if (maybe(base, modelinfo::dualConnection))
        queueRequest(nothing::cmd::RequestDualEnable);
    // Spatial audio cannot be read; the switch shows the last value set here.
    emit featuresChanged(); // spatial audio / fit test depend on Connected
}

void EarController::resetFeatures()
{
    m_seenInEar = m_seenLatency = m_seenSuperMic = false;
    m_seenPersonalizedAnc = m_seenEnhancedBass = m_seenGestures = false;
    m_seenDual = false;
    m_gestures.clear();
    if (!m_dualDevices.isEmpty()) {
        m_dualDevices.clear();
        emit dualDevicesChanged();
    }
    const bool wasRinging = m_ringingLeft || m_ringingRight;
    m_ringingLeft = m_ringingRight = false;
    m_fitTimeout.stop();
    if (m_fitState == FitRunning)
        m_fitState = FitIdle;
    emit featuresChanged();
    emit gesturesChanged();
    if (wasRinging)
        emit ringingChanged();
    emit fitChanged();
}

bool EarController::handleFeaturePacket(const nothing::Packet &packet)
{
    bool on = false;
    std::vector<nothing::DualDevice> dualList;
    quint8 mac[6];
    if (nothing::parseDualEnable(packet, on)) {
        m_seenDual = true;
        m_dualEnabled = on;
    } else if (nothing::parseDualList(packet, dualList)) {
        bool added = false;
        for (const nothing::DualDevice &d : dualList) {
            DualEntry e;
            e.mac = QByteArray(reinterpret_cast<const char *>(d.mac), 6);
            e.name = QString::fromUtf8(d.name.c_str());
            e.self = d.self;
            e.connected = d.connected;
            bool found = false;
            for (DualEntry &existing : m_dualDevices) {
                if (existing.mac == e.mac) {
                    existing = e;
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_dualDevices.append(e);
                added = true;
            }
        }
        // The list comes in pages; ask for the next one while new devices arrive.
        if (added && ++m_dualPages < DualMaxPages)
            send(nothing::cmd::RequestDualList,
                 std::vector<quint8>{ quint8(m_dualDevices.size() & 0xFF) });
        emit dualDevicesChanged();
        return true;
    } else if (nothing::parseDualDeviceEvent(packet, on, mac)) {
        const QByteArray key(reinterpret_cast<const char *>(mac), 6);
        bool found = false;
        for (DualEntry &e : m_dualDevices) {
            if (e.mac == key) {
                e.connected = on;
                found = true;
            }
        }
        if (found)
            emit dualDevicesChanged();
        else
            m_dualRefresh.start();
        return true;
    } else if (nothing::parseInEar(packet, on)) {
        m_seenInEar = true;
        m_inEar = on;
    } else if (nothing::parseLatency(packet, on)) {
        m_seenLatency = true;
        m_lowLatency = on;
    } else if (nothing::parseSuperMic(packet, on)) {
        m_seenSuperMic = true;
        m_superMic = on;
    } else if (nothing::parsePersonalizedAnc(packet, on)) {
        m_seenPersonalizedAnc = true;
        m_personalizedAnc = on;
    } else {
        quint8 level = 0;
        if (nothing::parseEnhancedBass(packet, on, level)) {
            m_seenEnhancedBass = true;
            m_enhancedBass = on;
            if (level >= 1 && level <= 5)
                m_bassLevel = level;
        } else {
            std::string text;
            std::vector<nothing::GestureSlot> gestureList;
            quint8 left = 0, right = 0;
            if (nothing::parseFirmware(packet, text)) {
                m_firmware = QString::fromStdString(text);
                emit infoChanged();
                return true;
            } else if (nothing::parseSerial(packet, text)) {
                m_serial = QString::fromStdString(text);
                m_serialSku = QString::fromStdString(nothing::skuFromSerial(text));
                qDebug() << "Serial" << m_serial << "SKU" << m_serialSku;
                emit infoChanged();
                updateModelBase();
                return true;
            } else if (nothing::parseGestures(packet, gestureList)) {
                m_seenGestures = true;
                m_gestures.clear();
                for (const nothing::GestureSlot &slot : gestureList) {
                    qDebug() << "Gesture reported: side" << slot.device << "common" << slot.common
                             << "type" << slot.type << "action" << slot.action;
                    Gesture g;
                    g.common = slot.common;
                    g.action = slot.action;
                    m_gestures.insert(gestureKey(slot.device, slot.type), g);
                }
                emit gesturesChanged();
            } else if (nothing::parseFitResult(packet, left, right)) {
                m_fitTimeout.stop();
                m_fitLeft = left;
                m_fitRight = right;
                m_fitState = FitDone;
                emit fitChanged();
                return true;
            } else {
                return false;
            }
        }
    }
    emit featuresChanged();
    return true;
}

void EarController::setInEarDetection(bool enabled)
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::SetInEar, nothing::setInEarPayload(enabled));
    m_inEar = enabled;
    emit featuresChanged();
    queueRequest(nothing::cmd::RequestInEar);
}

void EarController::setLowLatency(bool enabled)
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::SetLatency, nothing::setLatencyPayload(enabled));
    m_lowLatency = enabled;
    emit featuresChanged();
    queueRequest(nothing::cmd::RequestLatency);
}

void EarController::setSuperMic(bool enabled)
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::SetSuperMic, std::vector<quint8>{ quint8(enabled ? 1 : 0) });
    m_superMic = enabled;
    emit featuresChanged();
    queueRequest(nothing::cmd::RequestSuperMic);
}

void EarController::setPersonalizedAnc(bool enabled)
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::SetPersonalizedAnc, std::vector<quint8>{ quint8(enabled ? 1 : 0) });
    m_personalizedAnc = enabled;
    emit featuresChanged();
    queueRequest(nothing::cmd::RequestPersonalizedAnc);
}

void EarController::setEnhancedBass(bool enabled, int level)
{
    if (m_state != Connected)
        return;
    level = qBound(1, level, 5);
    if (enabled && m_spatialAudio && bassSpatialExclusive())
        setSpatialAudio(false);
    send(nothing::cmd::SetEnhancedBass, nothing::setEnhancedBassPayload(enabled, quint8(level)));
    m_enhancedBass = enabled;
    m_bassLevel = level;
    emit featuresChanged();
    queueRequest(nothing::cmd::RequestEnhancedBass);
}

void EarController::setSpatialAudio(bool enabled)
{
    if (m_state != Connected)
        return;
    if (enabled && m_enhancedBass && bassSpatialExclusive())
        setEnhancedBass(false, m_bassLevel);
    send(nothing::cmd::SetSpatialAudio, nothing::setSpatialAudioPayload(enabled));
    m_spatialAudio = enabled;
    m_settings.setValue(KeySpatialAudio, enabled);
    emit featuresChanged();
}

QVariantList EarController::gestureTypes() const
{
    QVariantList list;
    for (int type : modelinfo::gestureTypes(modelinfo::Base(m_modelBase)))
        list.append(type);
    return list;
}

bool EarController::isNoiseControlAction(int action) const
{
    return action == nothing::ActionNoiseControlAll
        || action == nothing::ActionNoiseControlAncOff
        || action == nothing::ActionNoiseControlTranspOff
        || action == nothing::ActionNoiseControlAncTransp;
}

QVariantList EarController::gestureActions(int side, int type) const
{
    QVariantList list;
    for (int action : modelinfo::gestureActions(modelinfo::Base(m_modelBase), type))
        list.append(action);
    int current = gestureAction(side, type);
    if (isNoiseControlAction(current))
        current = nothing::ActionNoiseControlAll;
    if (current > 0 && !list.contains(current))
        list.append(current);
    return list;
}

int EarController::gestureAction(int side, int type) const
{
    const auto it = m_gestures.constFind(gestureKey(side, type));
    return it == m_gestures.constEnd() ? -1 : it->action;
}

void EarController::setGestureAction(int side, int type, int action)
{
    if (m_state != Connected || action <= 0)
        return;
    nothing::GestureSlot slot;
    slot.device = quint8(side);
    slot.type = quint8(type);
    slot.action = quint8(action);
    // Always 1 for earbuds with pinch controls, as ear-web sends it; the value
    // the buds report is not always accepted back.
    slot.common = 1;
    qDebug() << "Set gesture side" << side << "type" << type << "action" << action;

    send(nothing::cmd::SetGesture, nothing::setGesturePayload(slot));
    Gesture g;
    g.common = slot.common;
    g.action = action;
    m_gestures.insert(gestureKey(side, type), g);
    emit gesturesChanged();
    m_gestureRefresh.start();
}

void EarController::ring(int side, bool on)
{
    if (m_state != Connected || (side != SideLeft && side != SideRight))
        return;
    send(nothing::cmd::Ring, nothing::ringPayload(quint8(side), on));
    if (side == SideLeft)
        m_ringingLeft = on;
    else
        m_ringingRight = on;
    emit ringingChanged();
}

void EarController::stopRinging()
{
    if (m_ringingLeft)
        ring(SideLeft, false);
    if (m_ringingRight)
        ring(SideRight, false);
}

void EarController::startFitTest()
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::StartFitTest, std::vector<quint8>{ 0x01 });
    m_fitLeft = m_fitRight = -1;
    m_fitState = FitRunning;
    m_fitTimeout.start();
    emit fitChanged();
}

void EarController::resetFitTest()
{
    m_fitTimeout.stop();
    m_fitLeft = m_fitRight = -1;
    m_fitState = FitIdle;
    emit fitChanged();
}

void EarController::onFitTimeout()
{
    if (m_fitState != FitRunning)
        return;
    m_fitState = FitNoResult;
    emit fitChanged();
}

// ---------------------------------------------------------------------------
// Dual connection

bool EarController::hasDualConnection() const
{
    return m_seenDual && maybe(m_modelBase, modelinfo::dualConnection);
}

bool EarController::dualConnectionNeedsRestart() const
{
    return modelinfo::dualConnectionReboot(modelinfo::Base(m_modelBase));
}

QVariantList EarController::dualDevices() const
{
    QVariantList list;
    for (const DualEntry &e : m_dualDevices) {
        QVariantMap map;
        map.insert(QStringLiteral("mac"), macToString(e.mac));
        map.insert(QStringLiteral("name"), e.name);
        map.insert(QStringLiteral("self"), e.self);
        map.insert(QStringLiteral("connected"), e.connected);
        list.append(map);
    }
    return list;
}

void EarController::setDualConnection(bool enabled)
{
    if (m_state != Connected)
        return;
    send(nothing::cmd::SetDualEnable, std::vector<quint8>{ quint8(enabled ? 1 : 0) });
    m_dualEnabled = enabled;
    emit featuresChanged();
    if (enabled) {
        m_dualRefresh.start();
    } else if (!m_dualDevices.isEmpty()) {
        m_dualDevices.clear();
        emit dualDevicesChanged();
    }
    // Models that restart drop the connection; the normal reconnect follows.
    queueRequest(nothing::cmd::RequestDualEnable);
}

void EarController::refreshDualDevices()
{
    if (m_state != Connected)
        return;
    m_dualDevices.clear();
    m_dualPages = 0;
    emit dualDevicesChanged();
    send(nothing::cmd::RequestDualList, std::vector<quint8>{ 0x00 });
}

void EarController::setDualDeviceConnected(const QString &mac, bool connect)
{
    if (m_state != Connected)
        return;
    const QByteArray bytes = macFromString(mac);
    if (bytes.size() != 6)
        return;
    send(nothing::cmd::SetDualConnect,
         nothing::dualConnectPayload(reinterpret_cast<const quint8 *>(bytes.constData()), connect));
    m_dualRefresh.start();
}
