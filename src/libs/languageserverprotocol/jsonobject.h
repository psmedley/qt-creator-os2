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

#pragma once

#include "languageserverprotocol_global.h"
#include "lsputils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>

namespace LanguageServerProtocol {

class LANGUAGESERVERPROTOCOL_EXPORT JsonObject
{
    Q_DECLARE_TR_FUNCTIONS(LanguageServerProtocol::JsonObject)

public:
    using iterator = QJsonObject::iterator;
    using const_iterator = QJsonObject::const_iterator;

    JsonObject() = default;

    explicit JsonObject(const QJsonObject &object) : m_jsonObject(object) { }
    explicit JsonObject(QJsonObject &&object) : m_jsonObject(std::move(object)) { }

    JsonObject(const JsonObject &object) : m_jsonObject(object.m_jsonObject) { }
    JsonObject(JsonObject &&object) : m_jsonObject(std::move(object.m_jsonObject)) { }

    explicit JsonObject(const QJsonValue &value) : m_jsonObject(value.toObject()) { }

    virtual ~JsonObject() = default;

    JsonObject &operator=(const JsonObject &other);
    JsonObject &operator=(JsonObject &&other);

    bool operator==(const JsonObject &other) const { return m_jsonObject == other.m_jsonObject; }

    operator const QJsonObject&() const { return m_jsonObject; }

    virtual bool isValid() const { return true; }

    iterator end() { return m_jsonObject.end(); }
    const_iterator end() const { return m_jsonObject.end(); }

protected:
    iterator insert(const QStringView key, const JsonObject &value);
    iterator insert(const QStringView key, const QJsonValue &value);

    template <typename T, typename V>
    iterator insertVariant(const QStringView key, const V &variant);
    template <typename T1, typename T2, typename... Args, typename V>
    iterator insertVariant(const QStringView key, const V &variant);

    // QJSonObject redirections
    QJsonValue value(const QStringView key) const { return m_jsonObject.value(key); }
    bool contains(const QStringView key) const { return m_jsonObject.contains(key); }
    iterator find(const QStringView key) { return m_jsonObject.find(key); }
    const_iterator find(const QStringView key) const { return m_jsonObject.find(key); }
    void remove(const QStringView key) { m_jsonObject.remove(key); }
    QStringList keys() const { return m_jsonObject.keys(); }

    // convenience value access
    template<typename T>
    T typedValue(const QStringView key) const;
    template<typename T>
    Utils::optional<T> optionalValue(const QStringView key) const;
    template<typename T>
    LanguageClientValue<T> clientValue(const QStringView key) const;
    template<typename T>
    Utils::optional<LanguageClientValue<T>> optionalClientValue(const QStringView key) const;
    template<typename T>
    QList<T> array(const QStringView key) const;
    template<typename T>
    Utils::optional<QList<T>> optionalArray(const QStringView key) const;
    template<typename T>
    LanguageClientArray<T> clientArray(const QStringView key) const;
    template<typename T>
    Utils::optional<LanguageClientArray<T>> optionalClientArray(const QStringView key) const;
    template<typename T>
    void insertArray(const QStringView key, const QList<T> &array);
    template<typename>
    void insertArray(const QStringView key, const QList<JsonObject> &array);

private:
    QJsonObject m_jsonObject;
};

template<typename T, typename V>
JsonObject::iterator JsonObject::insertVariant(const QStringView key, const V &variant)
{
    return Utils::holds_alternative<T>(variant) ? insert(key, Utils::get<T>(variant)) : end();
}

template<typename T1, typename T2, typename... Args, typename V>
JsonObject::iterator JsonObject::insertVariant(const QStringView key, const V &variant)
{
    auto result = insertVariant<T1>(key, variant);
    return result != end() ? result : insertVariant<T2, Args...>(key, variant);
}

template<typename T>
T JsonObject::typedValue(const QStringView key) const
{
    return fromJsonValue<T>(value(key));
}

template<typename T>
Utils::optional<T> JsonObject::optionalValue(const QStringView key) const
{
    const QJsonValue &val = value(key);
    return val.isUndefined() ? Utils::nullopt : Utils::make_optional(fromJsonValue<T>(val));
}

template<typename T>
LanguageClientValue<T> JsonObject::clientValue(const QStringView key) const
{
    return LanguageClientValue<T>(value(key));
}

template<typename T>
Utils::optional<LanguageClientValue<T>> JsonObject::optionalClientValue(const QStringView key) const
{
    return contains(key) ? Utils::make_optional(clientValue<T>(key)) : Utils::nullopt;
}

template<typename T>
QList<T> JsonObject::array(const QStringView key) const
{
    if (const Utils::optional<QList<T>> &array = optionalArray<T>(key))
        return *array;
    qCDebug(conversionLog) << QString("Expected array under %1 in:").arg(key) << *this;
    return {};
}

template<typename T>
Utils::optional<QList<T>> JsonObject::optionalArray(const QStringView key) const
{
    const QJsonValue &jsonValue = value(key);
    if (jsonValue.isUndefined())
        return Utils::nullopt;
    return Utils::transform<QList<T>>(jsonValue.toArray(), &fromJsonValue<T>);
}

template<typename T>
LanguageClientArray<T> JsonObject::clientArray(const QStringView key) const
{
    return LanguageClientArray<T>(value(key));
}

template<typename T>
Utils::optional<LanguageClientArray<T>> JsonObject::optionalClientArray(const QStringView key) const
{
    const QJsonValue &val = value(key);
    return !val.isUndefined() ? Utils::make_optional(LanguageClientArray<T>(value(key)))
                              : Utils::nullopt;
}

template<typename T>
void JsonObject::insertArray(const QStringView key, const QList<T> &array)
{
    QJsonArray jsonArray;
    for (const T &item : array)
        jsonArray.append(QJsonValue(item));
    insert(key, jsonArray);
}

template<typename >
void JsonObject::insertArray(const QStringView key, const QList<JsonObject> &array)
{
    QJsonArray jsonArray;
    for (const JsonObject &item : array)
        jsonArray.append(item.m_jsonObject);
    insert(key, jsonArray);
}

} // namespace LanguageServerProtocol
