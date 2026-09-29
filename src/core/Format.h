#pragma once

#include <QString>
#include <QtGlobal>

// 界面上的数值格式。使用 C 语言小数点，不跟系统区域设置走，避免排序和显示不一致。
namespace format {

// 字节数。1024 进位，小于 1024 时不带小数，更大时保留 0 或 1 位。
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

// 字节每秒，例如「1.2 MB/s」。
inline QString rate(quint64 bytesPerSec)
{
    return bytes(bytesPerSec) + QStringLiteral("/s");
}

// 百分比，固定一位小数。
inline QString percent(double value)
{
    return QString::number(value, 'f', 1) + QLatin1Char('%');
}

} // namespace format
