/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "getoperation.h"

#include <iostream>

#ifdef WITH_TESTS
#include <QTest>
#endif

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(getlog, "qtc.sdktool.operations.get", QtWarningMsg)

QString GetOperation::name() const
{
    return QLatin1String("get");
}

QString GetOperation::helpText() const
{
    return QLatin1String("get settings from configuration");
}

QString GetOperation::argumentsHelpText() const
{
    return QLatin1String("A file (relative to top-level settings directory and without .xml extension)\n"
                         "followed by one or more keys to list.\n");
}

bool GetOperation::setArguments(const QStringList &args)
{
    if (args.count() < 2)
        return false;

    m_keys = args;
    m_file = m_keys.takeFirst();

    if (m_file.isEmpty())
        qCCritical(getlog) << "No file given.";
    if (m_keys.isEmpty())
        qCCritical(getlog) << "No keys given.";

    return !m_file.isEmpty() && !m_keys.isEmpty();
}

static QString toString(const QVariant &variant, int indentation = 0)
{
    const QString indent(indentation, QLatin1Char(' '));
    switch (variant.type()) {
    case QVariant::Map: {
        QVariantMap map = variant.toMap();
        QString res;
        for (auto item = map.begin(); item != map.end(); ++item) {
            res += indent + item.key() + QLatin1String(": ");
            QVariant value = item.value();
            switch (value.type()) {
            case QVariant::Map:
            case QVariant::List:
                res += QLatin1Char('\n') + toString(value, indentation + 1);
                break;
            default:
                res += value.toString();
                break;
            }
            res += QLatin1Char('\n');
        }
        return res;
    }
    case QVariant::List: {
        const QVariantList list = variant.toList();
        QString res;
        int counter = 0;
        for (const QVariant &item : list)
            res += indent + QString::number(counter++) + QLatin1String(":\n") + toString(item, indentation + 1);
        return res;
    }
    default:
        return indent + variant.toString();
    }
}

int GetOperation::execute() const
{
    Q_ASSERT(!m_keys.isEmpty());
    QVariantMap map = load(m_file);

    for (const QString &key : qAsConst(m_keys)) {
        const QVariant result = get(map, key);
        if (!result.isValid())
            std::cout << "<invalid>" << std::endl;
        else
            std::cout << qPrintable(toString(result)) << std::endl;
    }

    return 0;
}

#ifdef WITH_TESTS
void GetOperation::unittest()
{
    QVariantMap testMap;
    QVariantMap subKeys;
    QVariantMap cur;
    cur.insert(QLatin1String("testint2"), 53);
    subKeys.insert(QLatin1String("subsubkeys"), cur);
    subKeys.insert(QLatin1String("testbool"), true);
    testMap.insert(QLatin1String("subkeys"), subKeys);
    subKeys.clear();
    testMap.insert(QLatin1String("subkeys2"), subKeys);
    testMap.insert(QLatin1String("testint"), 23);

    QVariant result;
    result = get(testMap, QLatin1String("testint"));
    QCOMPARE(result.toString(), QLatin1String("23"));

    result = get(testMap, QLatin1String("subkeys/testbool"));
    QCOMPARE(result.toString(), QLatin1String("true"));

    result = get(testMap, QLatin1String("subkeys/subsubkeys"));
    QCOMPARE(result.type(), QVariant::Map);

    result = get(testMap, QLatin1String("nonexistant"));
    QVERIFY(!result.isValid());
}
#endif

QVariant GetOperation::get(const QVariantMap &map, const QString &key)
{
    if (key.isEmpty())
        return QString();

    const QStringList keys = key.split(QLatin1Char('/'));

    QVariantMap subMap = map;
    for (int i = 0; i < keys.count() - 1; ++i) {
        const QString k = keys.at(i);
        if (!subMap.contains(k))
            return QString();
        subMap = subMap.value(k).toMap();
    }

    return subMap.value(keys.last());
}
