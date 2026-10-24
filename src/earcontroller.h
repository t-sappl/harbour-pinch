/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EARCONTROLLER_H
#define EARCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QSettings>
#include <QTimer>

#include <vector>

namespace nothing { struct Packet; }

class BluezProfile;
class RfcommChannel;
class QDBusPendingCallWatcher;

/*
 * Owns the RFCOMM control connection to one pair of Nothing earbuds and
 * exposes the ANC state to QML. The audio connection itself (A2DP/HFP) is
 * handled by the system; this class only opens the vendor control channel
 * once BlueZ reports the selected earbuds as connected.
 *
 * Connecting normally goes through BlueZ (Profile1 + Device1.ConnectProfile),
 * which finds the RFCOMM channel by service UUID. A fixed channel from the
 * settings bypasses that and connects the socket directly.
 */
class EarController : public QObject
{
    Q_OBJECT
    Q_ENUMS(ConnectionState AncMode AncStrength FormFactor FitState Side)

    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool hasDevice READ hasDevice NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceAddress READ deviceAddress NOTIFY deviceChanged)
    // Name the device reports itself (e.g. "Nothing Ear (3)"), independent of
    // a renamed alias, and the generic form factor derived from it.
    Q_PROPERTY(QString modelName READ modelName NOTIFY deviceChanged)
    Q_PROPERTY(int formFactor READ formFactor NOTIFY deviceChanged)
    // False for models without noise control (e.g. Ear (open)).
    Q_PROPERTY(bool ancSupported READ ancSupported NOTIFY ancSupportedChanged)
    // Audio connection as seen by the system (A2DP/HFP), not the control channel.
    Q_PROPERTY(bool systemConnected READ systemConnected NOTIFY systemConnectedChanged)
    Q_PROPERTY(bool audioConnecting READ audioConnecting NOTIFY audioConnectingChanged)
    Q_PROPERTY(bool autoConnectAudio READ autoConnectAudio WRITE setAutoConnectAudio NOTIFY autoConnectAudioChanged)

    // Model and device info (from the serial number, else the Bluetooth name).
    Q_PROPERTY(int modelBase READ modelBase NOTIFY infoChanged)
    Q_PROPERTY(QString detectedModel READ detectedModel NOTIFY infoChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber NOTIFY infoChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY infoChanged)

    // Which extra features to show. Readable features appear once the buds
    // answered the request on the current connection.
    Q_PROPERTY(bool hasInEarDetection READ hasInEarDetection NOTIFY featuresChanged)
    Q_PROPERTY(bool hasLowLatency READ hasLowLatency NOTIFY featuresChanged)
    Q_PROPERTY(bool hasSuperMic READ hasSuperMic NOTIFY featuresChanged)
    Q_PROPERTY(bool hasPersonalizedAnc READ hasPersonalizedAnc NOTIFY featuresChanged)
    Q_PROPERTY(bool hasEnhancedBass READ hasEnhancedBass NOTIFY featuresChanged)
    Q_PROPERTY(bool hasSpatialAudio READ hasSpatialAudio NOTIFY featuresChanged)
    Q_PROPERTY(bool hasGestures READ hasGestures NOTIFY featuresChanged)
    Q_PROPERTY(bool hasFitTest READ hasFitTest NOTIFY featuresChanged)
    Q_PROPERTY(bool bassSpatialExclusive READ bassSpatialExclusive NOTIFY featuresChanged)
    Q_PROPERTY(bool hasDualConnection READ hasDualConnection NOTIFY featuresChanged)
    Q_PROPERTY(bool dualConnectionNeedsRestart READ dualConnectionNeedsRestart NOTIFY featuresChanged)

    // Feature states.
    Q_PROPERTY(bool inEarDetection READ inEarDetection NOTIFY featuresChanged)
    Q_PROPERTY(bool lowLatency READ lowLatency NOTIFY featuresChanged)
    Q_PROPERTY(bool superMic READ superMic NOTIFY featuresChanged)
    Q_PROPERTY(bool personalizedAnc READ personalizedAnc NOTIFY featuresChanged)
    Q_PROPERTY(bool enhancedBass READ enhancedBass NOTIFY featuresChanged)
    Q_PROPERTY(int bassLevel READ bassLevel NOTIFY featuresChanged)
    Q_PROPERTY(bool spatialAudio READ spatialAudio NOTIFY featuresChanged)
    Q_PROPERTY(bool dualConnection READ dualConnection NOTIFY featuresChanged)
    // Remembered devices: [{ mac, name, self, connected }]
    Q_PROPERTY(QVariantList dualDevices READ dualDevices NOTIFY dualDevicesChanged)

    // Find my buds and the ear tip fit test.
    Q_PROPERTY(bool ringingLeft READ ringingLeft NOTIFY ringingChanged)
    Q_PROPERTY(bool ringingRight READ ringingRight NOTIFY ringingChanged)
    Q_PROPERTY(int fitState READ fitState NOTIFY fitChanged)
    Q_PROPERTY(int fitLeft READ fitLeft NOTIFY fitChanged)
    Q_PROPERTY(int fitRight READ fitRight NOTIFY fitChanged)
    Q_PROPERTY(int ancMode READ ancMode NOTIFY ancChanged)
    Q_PROPERTY(int ancStrength READ ancStrength NOTIFY ancChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(int manualChannel READ manualChannel WRITE setManualChannel NOTIFY manualChannelChanged)
    // Battery in percent, -1 when the component is not reported.
    Q_PROPERTY(int batteryLeft READ batteryLeft NOTIFY batteryChanged)
    Q_PROPERTY(int batteryRight READ batteryRight NOTIFY batteryChanged)
    Q_PROPERTY(int batteryCase READ batteryCase NOTIFY batteryChanged)
    Q_PROPERTY(bool chargingLeft READ chargingLeft NOTIFY batteryChanged)
    Q_PROPERTY(bool chargingRight READ chargingRight NOTIFY batteryChanged)
    Q_PROPERTY(bool chargingCase READ chargingCase NOTIFY batteryChanged)
    // Over-ear headphones report a single battery instead of left/right/case.
    Q_PROPERTY(int batterySingle READ batterySingle NOTIFY batteryChanged)
    Q_PROPERTY(bool chargingSingle READ chargingSingle NOTIFY batteryChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        Connecting,
        Connected
    };

    enum AncMode {
        ModeUnknown = 0,
        ModeOff,
        ModeTransparency,
        ModeNoiseCancellation
    };

    enum AncStrength {
        StrengthLow = 0,
        StrengthMid,
        StrengthHigh,
        StrengthAdaptive
    };

    // Same values as nothing::DeviceId.
    enum Side {
        SideLeft = 2,
        SideRight = 3
    };

    enum FitState {
        FitIdle = 0,
        FitRunning,
        FitDone,
        FitNoResult
    };

    // Same values as formfactor::Type.
    enum FormFactor {
        FormInEar = 0,
        FormOpenEar,
        FormOverEar
    };

    explicit EarController(QObject *parent = nullptr);
    ~EarController();

    int connectionState() const { return m_state; }
    bool hasDevice() const { return !m_address.isEmpty(); }
    QString deviceName() const { return m_name; }
    QString deviceAddress() const { return m_address; }
    QString modelName() const { return m_modelName; }
    int formFactor() const;
    bool ancSupported() const { return m_ancSupported; }
    bool systemConnected() const { return m_systemConnected; }
    bool audioConnecting() const { return m_audioConnecting; }
    bool autoConnectAudio() const { return m_autoConnectAudio; }
    void setAutoConnectAudio(bool enabled);

    int modelBase() const { return m_modelBase; }
    QString detectedModel() const;
    QString serialNumber() const { return m_serial; }
    QString firmwareVersion() const { return m_firmware; }

    bool hasInEarDetection() const;
    bool hasLowLatency() const { return m_seenLatency; }
    bool hasSuperMic() const;
    bool hasPersonalizedAnc() const;
    bool hasEnhancedBass() const;
    bool hasSpatialAudio() const;
    bool hasGestures() const { return m_seenGestures; }
    bool hasFitTest() const;
    bool bassSpatialExclusive() const;
    bool hasDualConnection() const;
    bool dualConnectionNeedsRestart() const;
    bool dualConnection() const { return m_dualEnabled; }
    QVariantList dualDevices() const;

    bool inEarDetection() const { return m_inEar; }
    bool lowLatency() const { return m_lowLatency; }
    bool superMic() const { return m_superMic; }
    bool personalizedAnc() const { return m_personalizedAnc; }
    bool enhancedBass() const { return m_enhancedBass; }
    int bassLevel() const { return m_bassLevel; }
    bool spatialAudio() const { return m_spatialAudio; }

    bool ringingLeft() const { return m_ringingLeft; }
    bool ringingRight() const { return m_ringingRight; }
    int fitState() const { return m_fitState; }
    int fitLeft() const { return m_fitLeft; }
    int fitRight() const { return m_fitRight; }
    int ancMode() const { return m_mode; }
    int ancStrength() const { return m_strength; }
    QString errorString() const { return m_errorString; }
    int manualChannel() const { return m_manualChannel; }
    void setManualChannel(int channel);
    int batteryLeft() const { return m_batteryLeft; }
    int batteryRight() const { return m_batteryRight; }
    int batteryCase() const { return m_batteryCase; }
    bool chargingLeft() const { return m_chargingLeft; }
    bool chargingRight() const { return m_chargingRight; }
    bool chargingCase() const { return m_chargingCase; }
    int batterySingle() const { return m_batterySingle; }
    bool chargingSingle() const { return m_chargingSingle; }

    Q_INVOKABLE void selectDevice(const QString &address, const QString &name,
                                  const QString &modelName, const QString &path,
                                  bool systemConnected);
    // Connects whatever is missing: the earbuds' audio connection first, the
    // control channel follows automatically once the system reports it.
    Q_INVOKABLE void connectNow();
    Q_INVOKABLE void reconnect();
    // Opens the control channel if the system has the earbuds connected
    // but the channel is not open (called when the app becomes active).
    Q_INVOKABLE void ensureConnected();
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void setAncMode(int mode);
    Q_INVOKABLE void setAncStrength(int strength);
    Q_INVOKABLE void cycleMode();
    Q_INVOKABLE void requestAnc();
    Q_INVOKABLE void requestBattery();

    Q_INVOKABLE void setInEarDetection(bool enabled);
    Q_INVOKABLE void setLowLatency(bool enabled);
    Q_INVOKABLE void setSuperMic(bool enabled);
    Q_INVOKABLE void setPersonalizedAnc(bool enabled);
    Q_INVOKABLE void setEnhancedBass(bool enabled, int level);
    Q_INVOKABLE void setSpatialAudio(bool enabled);
    Q_INVOKABLE void setDualConnection(bool enabled);
    Q_INVOKABLE void refreshDualDevices();
    Q_INVOKABLE void setDualDeviceConnected(const QString &mac, bool connect);

    // Gestures: side is Side, type a gesture type (2, 3, 7, 9).
    Q_INVOKABLE QVariantList gestureTypes() const;
    // Actions offered for a type; includes the current action of 'side'
    // if the buds report one the list does not know.
    Q_INVOKABLE QVariantList gestureActions(int side, int type) const;
    Q_INVOKABLE int gestureAction(int side, int type) const; // -1 if unknown
    Q_INVOKABLE void setGestureAction(int side, int type, int action);
    Q_INVOKABLE bool isNoiseControlAction(int action) const;

    Q_INVOKABLE void ring(int side, bool on);
    Q_INVOKABLE void stopRinging();
    Q_INVOKABLE void startFitTest();
    Q_INVOKABLE void resetFitTest();

public slots:
    // Fed by DeviceModel whenever BlueZ reports a change of a device's
    // Connected property (and once per device on the first refresh).
    void onSystemConnectionChanged(const QString &address, const QString &path,
                                   const QString &modelName, bool connected);

signals:
    void connectionStateChanged();
    void deviceChanged();
    void ancChanged();
    void errorStringChanged();
    void manualChannelChanged();
    void batteryChanged();
    void ancSupportedChanged();
    void systemConnectedChanged();
    void audioConnectingChanged();
    void autoConnectAudioChanged();
    void infoChanged();
    void featuresChanged();
    void gesturesChanged();
    void ringingChanged();
    void fitChanged();
    void dualDevicesChanged();

private slots:
    void onProfileConnection(const QString &devicePath, int fd);
    void onProfileDisconnectionRequested(const QString &devicePath);
    void onConnectProfileFinished(QDBusPendingCallWatcher *watcher);
    void onChannelConnected();
    void onChannelFailed(const QString &error);
    void onChannelClosed();
    void onDataReceived(const QByteArray &data);
    void onConnectTimeout();
    void onHandoverTimeout();
    void onAncProbeTimeout();
    void onAudioConnectFinished(QDBusPendingCallWatcher *watcher);
    void onQueueTimer();
    void onFitTimeout();

private:
    void openConnection();
    bool retryLater(const char *reason);
    void onWatchdog();
    void checkStartupState();
    void connectAudio();
    void setSystemConnected(bool connected);
    void setAncSupported(bool supported);
    void setModelName(const QString &modelName);
    void queueRequest(quint16 command);
    void requestDeviceState();
    void resetFeatures();
    bool handleFeaturePacket(const nothing::Packet &packet);
    void updateModelBase();
    void closeConnection();
    RfcommChannel *createChannel();
    QString devicePath() const;
    void setState(ConnectionState state);
    void setErrorString(const QString &error);
    void applyAncCode(quint8 code);
    void applyBattery(int left, bool chargingLeft, int right, bool chargingRight,
                      int caseLevel, bool chargingCase,
                      int single = -1, bool chargingSingle = false);
    void sendAncCode(quint8 code);
    void send(quint16 command, const std::vector<quint8> &payload);
    quint8 nextOperationId();
    quint8 strengthCode(int strength) const;

    QSettings m_settings;
    BluezProfile *m_profile;
    RfcommChannel *m_channel;
    QTimer m_connectDelay;
    QTimer m_connectTimeout;
    QTimer m_confirmTimer;
    QTimer m_batteryPoll;
    QTimer m_ancProbe;
    QTimer m_queueTimer;
    QTimer m_gestureRefresh;
    QTimer m_fitTimeout;
    QTimer m_watchdog;
    QList<quint16> m_requestQueue;
    QTimer m_dualRefresh;
    QTimer m_handoverTimer;

    std::vector<quint8> m_buffer;
    quint8 m_operationId;

    ConnectionState m_state;
    QString m_address;
    QString m_name;
    QString m_path;
    QString m_modelName;
    AncMode m_mode;
    int m_strength;
    QString m_errorString;
    int m_manualChannel;
    bool m_systemConnected;
    int m_batteryLeft;
    int m_batteryRight;
    int m_batteryCase;
    bool m_chargingLeft;
    bool m_chargingRight;
    bool m_chargingCase;
    int m_batterySingle;
    bool m_chargingSingle;
    bool m_ancSupported;
    bool m_ancSeen;          // an ANC report arrived on the current connection
    bool m_audioConnecting;
    bool m_autoConnectAudio;
    bool m_autoConnectTried; // auto-connect runs at most once per app start

    int m_modelBase;
    QString m_serial;
    QString m_serialSku;
    QString m_firmware;

    bool m_seenInEar;
    bool m_seenLatency;
    bool m_seenSuperMic;
    bool m_seenPersonalizedAnc;
    bool m_seenEnhancedBass;
    bool m_seenGestures;
    bool m_inEar;
    bool m_lowLatency;
    bool m_superMic;
    bool m_personalizedAnc;
    bool m_enhancedBass;
    int m_bassLevel;
    bool m_spatialAudio;
    bool m_seenDual;
    bool m_dualEnabled;

    struct DualEntry {
        QByteArray mac; // 6 bytes, display order
        QString name;
        bool self;
        bool connected;
    };
    QList<DualEntry> m_dualDevices;
    int m_dualPages;
    int m_retriesLeft; // automatic retries for the control channel
    int m_watchdogTries; // watchdog attempts since the last connection change
    bool m_userDisconnected; // "Disconnect" was chosen: no automatic reconnects

    struct Gesture {
        int common;
        int action;
    };
    QHash<int, Gesture> m_gestures; // key: side << 8 | type

    bool m_ringingLeft;
    bool m_ringingRight;
    int m_fitState;
    int m_fitLeft;
    int m_fitRight;
};

#endif // EARCONTROLLER_H
