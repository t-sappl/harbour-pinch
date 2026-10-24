/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Which features a model offers. Only the models this app is tested with
 * are listed; anything else is "Unknown" and gets the features that are
 * safe to try (the app then shows readable features once the buds answer).
 * Sources: earctl models.rs and ear-web's per-model pages.
 */

#ifndef MODELINFO_H
#define MODELINFO_H

#include <QString>
#include <QVector>

namespace modelinfo {

enum Base {
    Unknown = 0,
    B155, // Nothing Ear (2)
    B173, // Nothing Ear (3)
    B174  // Nothing Ear (open)
};

inline Base fromSku(const QString &sku)
{
    static const char *const ear2[] = { "17", "18", "19", "27", "28", "29" };
    for (const char *s : ear2) {
        if (sku == QLatin1String(s))
            return B155;
    }
    if (sku == QLatin1String("21") || sku == QLatin1String("22")
        || sku == QLatin1String("25"))
        return B173;
    if (sku == QLatin1String("11200005"))
        return B174;
    return Unknown;
}

// Fallback when the serial number gives no answer: the Bluetooth name the
// device reports itself.
inline Base fromName(const QString &name)
{
    const QString lower = name.toLower();
    if (!lower.contains(QLatin1String("nothing")))
        return Unknown;
    if (lower.contains(QLatin1String("ear (2)")))
        return B155;
    if (lower.contains(QLatin1String("ear (3)")))
        return B173;
    if (lower.contains(QLatin1String("ear (open)")))
        return B174;
    return Unknown;
}

inline QString displayName(Base base)
{
    switch (base) {
    case B155: return QStringLiteral("Nothing Ear (2)");
    case B173: return QStringLiteral("Nothing Ear (3)");
    case B174: return QStringLiteral("Nothing Ear (open)");
    default:   return QString();
    }
}

// Features that depend on the model. Unknown models get "maybe" (true) for
// readable features, which the controller only shows after the buds answer.
inline bool inEarDetection(Base b)  { return b != B174; }
inline bool fitTest(Base b)         { return b == B155 || b == B173; }
inline bool personalizedAnc(Base b) { return b == B155; }
inline bool enhancedBass(Base b)    { return b == B173; }
inline bool superMic(Base b)        { return b == B173; }
inline bool spatialAudio(Base b)    { return b == B173; } // write-only, no read
inline bool dualConnection(Base b)  { return b == B155 || b == B173 || b == B174; }
// Ear (2) restarts when dual connection is switched on or off.
inline bool dualConnectionReboot(Base b) { return b == B155; }
// Spatial audio and enhanced bass cannot be active at the same time.
inline bool bassSpatialExclusive(Base b) { return b == B173; }

// Gesture types offered and the actions for each type. "Noise control" is
// listed as action 10; the controller maps the variants 20/21/22 onto it.
inline QVector<int> gestureTypes(Base b)
{
    Q_UNUSED(b)
    return QVector<int>{ 2, 3, 7, 9 };
}

inline QVector<int> gestureActions(Base b, int type)
{
    // 1 none, 8 skip back, 9 skip forward, 10 noise control,
    // 11 voice assistant, 18 volume up, 19 volume down, 29 mic on/off
    switch (b) {
    case B173:
        switch (type) {
        case 2:  return { 8, 9, 11 };
        case 3:  return { 8, 9, 11, 1 };
        case 7:  return { 10, 18, 19, 11, 29, 1 };
        case 9:  return { 10, 18, 19, 11, 1 };
        }
        break;
    case B174:
        switch (type) {
        case 2:  return { 8, 9, 11 };
        case 3:  return { 8, 9, 11 };
        case 7:  return { 18, 19, 11 };
        case 9:  return { 18, 19, 11, 1 };
        }
        break;
    default: // B155 and unknown models
        switch (type) {
        case 2:  return { 8, 9, 11 };
        case 3:  return { 8, 9, 11 };
        case 7:  return { 10, 18, 19, 11 };
        case 9:  return { 10, 18, 19, 11, 1 };
        }
        break;
    }
    return {};
}

} // namespace modelinfo

#endif // MODELINFO_H
