// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QVariant>

namespace OrcaPortal
{
struct FilterEntry {
    uint type;
    QString value;
};
using FilterEntries = QList<FilterEntry>;
struct Filter {
    QString name;
    FilterEntries entries;
};
using Filters = QList<Filter>;
struct ChoiceOption {
    QString id;
    QString label;
};
using ChoiceOptions = QList<ChoiceOption>;
struct Choice {
    QString id;
    QString label;
    ChoiceOptions options;
    QString selected;
};
using Choices = QList<Choice>;
using ChoiceResults = QList<ChoiceOption>;

inline QDBusArgument &operator<<(QDBusArgument &a, const FilterEntry &v)
{
    a.beginStructure();
    a << v.type << v.value;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, FilterEntry &v)
{
    a.beginStructure();
    a >> v.type >> v.value;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const Filter &v)
{
    a.beginStructure();
    a << v.name << v.entries;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, Filter &v)
{
    a.beginStructure();
    a >> v.name >> v.entries;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const ChoiceOption &v)
{
    a.beginStructure();
    a << v.id << v.label;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, ChoiceOption &v)
{
    a.beginStructure();
    a >> v.id >> v.label;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const Choice &v)
{
    a.beginStructure();
    a << v.id << v.label << v.options << v.selected;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, Choice &v)
{
    a.beginStructure();
    a >> v.id >> v.label >> v.options >> v.selected;
    a.endStructure();
    return a;
}

}
Q_DECLARE_METATYPE(OrcaPortal::FilterEntry)
Q_DECLARE_METATYPE(OrcaPortal::FilterEntries)
Q_DECLARE_METATYPE(OrcaPortal::Filter)
Q_DECLARE_METATYPE(OrcaPortal::Filters)
Q_DECLARE_METATYPE(OrcaPortal::ChoiceOption)
Q_DECLARE_METATYPE(OrcaPortal::ChoiceOptions)
Q_DECLARE_METATYPE(OrcaPortal::Choice)
Q_DECLARE_METATYPE(OrcaPortal::Choices)

namespace OrcaPortal
{
inline void registerTypes()
{
    qDBusRegisterMetaType<FilterEntry>();
    qDBusRegisterMetaType<FilterEntries>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<Filters>();
    qDBusRegisterMetaType<ChoiceOption>();
    qDBusRegisterMetaType<ChoiceOptions>();
    qDBusRegisterMetaType<Choice>();
    qDBusRegisterMetaType<Choices>();
    qDBusRegisterMetaType<QList<QByteArray>>();
}

template<typename T>
T decode(const QVariant &v)
{
    return v.metaType() == QMetaType::fromType<QDBusArgument>() ? qdbus_cast<T>(v.value<QDBusArgument>()) : v.value<T>();
}
}
