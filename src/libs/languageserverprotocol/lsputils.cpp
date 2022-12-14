/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "lsputils.h"

#include <QHash>
#include <QLoggingCategory>
#include <QVector>

namespace LanguageServerProtocol {

Q_LOGGING_CATEGORY(conversionLog, "qtc.languageserverprotocol.conversion", QtWarningMsg)

template<>
QString fromJsonValue<QString>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isString())
        qCDebug(conversionLog) << "Expected String in json value but got: " << value;
    return value.toString();
}

template<>
int fromJsonValue<int>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isDouble())
        qCDebug(conversionLog) << "Expected double in json value but got: " << value;
    return value.toInt();
}

template<>
double fromJsonValue<double>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isDouble())
        qCDebug(conversionLog) << "Expected double in json value but got: " << value;
    return value.toDouble();
}

template<>
bool fromJsonValue<bool>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isBool())
        qCDebug(conversionLog) << "Expected bool in json value but got: " << value;
    return value.toBool();
}

template<>
QJsonArray fromJsonValue<QJsonArray>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isArray())
        qCDebug(conversionLog) << "Expected Array in json value but got: " << value;
    return value.toArray();
}

template<>
QJsonObject fromJsonValue<QJsonObject>(const QJsonValue &value)
{
    if (conversionLog().isDebugEnabled() && !value.isObject())
        qCDebug(conversionLog) << "Expected Object in json value but got: " << value;
    return value.toObject();
}

template<>
QJsonValue fromJsonValue<QJsonValue>(const QJsonValue &value)
{
    return value;
}

} // namespace LanguageServerProtocol
