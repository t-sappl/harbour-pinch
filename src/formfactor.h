/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FORMFACTOR_H
#define FORMFACTOR_H

#include <QString>

namespace formfactor {

// Values match EarController::FormFactor.
enum Type {
    InEar = 0,
    OpenEar = 1,
    OverEar = 2
};

// Guess the form factor from the Bluetooth name the device reports itself
// (BlueZ "Name", not the user-editable "Alias"). Only used for the generic
// illustration and for the initial guess whether ANC is available; the
// buds' actual answers always win.
inline Type fromName(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.contains(QLatin1String("open")))
        return OpenEar;
    if (lower.contains(QLatin1String("headphone")))
        return OverEar;
    return InEar;
}

} // namespace formfactor

#endif // FORMFACTOR_H
