#pragma once

#include <QString>
#include <QtGlobal>

namespace format {

inline QString bytes(quint64 value)
{
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double scaled = static_cast<double>(value);
    int unit = 0;
    while (scaled >= 1024.0 && unit < 4) {
        scaled /= 1024.0;
        ++unit;
    }
    if (unit == 0) {
        return QString::number(value) + QLatin1Char(' ') + QLatin1String(units[unit]);
    }
    const int digits = scaled >= 100.0 ? 0 : 1;
    return QString::number(scaled, 'f', digits) + QLatin1Char(' ') + QLatin1String(units[unit]);
}

inline QString rate(quint64 bytesPerSec)
{
    return bytes(bytesPerSec) + QStringLiteral("/s");
}

inline QString percent(double value)
{
    return QString::number(value, 'f', 1) + QLatin1Char('%');
}

} // namespace format
