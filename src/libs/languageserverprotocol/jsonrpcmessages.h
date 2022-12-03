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

#include "basemessage.h"
#include "lsptypes.h"
#include "jsonkeys.h"

#include <utils/optional.h>
#include <utils/qtcassert.h>
#include <utils/variant.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QCoreApplication>
#include <QUuid>

namespace LanguageServerProtocol {

class JsonRpcMessage;

class LANGUAGESERVERPROTOCOL_EXPORT MessageId : public Utils::variant<int, QString>
{
public:
    MessageId() : variant(QString()) {}
    explicit MessageId(int id) : variant(id) {}
    explicit MessageId(const QString &id) : variant(id) {}
    explicit MessageId(const QJsonValue &value)
    {
        if (value.isDouble())
            emplace<int>(value.toInt());
        else
            emplace<QString>(value.toString());
    }

    operator QJsonValue() const
    {
        if (auto id = Utils::get_if<int>(this))
            return *id;
        if (auto id = Utils::get_if<QString>(this))
            return *id;
        return QJsonValue();
    }

    bool isValid() const
    {
        if (Utils::holds_alternative<int>(*this))
            return true;
        const QString *id = Utils::get_if<QString>(this);
        QTC_ASSERT(id, return false);
        return !id->isEmpty();
    }

    QString toString() const
    {
        if (auto id = Utils::get_if<QString>(this))
            return *id;
        if (auto id = Utils::get_if<int>(this))
            return QString::number(*id);
        return {};
    }

    friend auto qHash(const MessageId &id)
    {
        if (Utils::holds_alternative<int>(id))
            return QT_PREPEND_NAMESPACE(qHash(Utils::get<int>(id)));
        if (Utils::holds_alternative<QString>(id))
            return QT_PREPEND_NAMESPACE(qHash(Utils::get<QString>(id)));
        return QT_PREPEND_NAMESPACE(qHash(0));
    }
};

template <typename Error>
inline QDebug operator<<(QDebug stream, const LanguageServerProtocol::MessageId &id)
{
    if (Utils::holds_alternative<int>(id))
        stream << Utils::get<int>(id);
    else
        stream << Utils::get<QString>(id);
    return stream;
}

struct ResponseHandler
{
    MessageId id;
    using Callback = std::function<void(const JsonRpcMessage &)>;
    Callback callback;
};

class LANGUAGESERVERPROTOCOL_EXPORT JsonRpcMessage
{
    Q_DECLARE_TR_FUNCTIONS(JsonRpcMessage)
public:
    JsonRpcMessage();
    explicit JsonRpcMessage(const BaseMessage &message);
    explicit JsonRpcMessage(const QJsonObject &jsonObject);
    explicit JsonRpcMessage(QJsonObject &&jsonObject);

    virtual ~JsonRpcMessage() = default;

    static QByteArray jsonRpcMimeType();

    QByteArray toRawData() const;
    virtual bool isValid(QString *errorMessage) const;

    const QJsonObject &toJsonObject() const;

    const QString parseError() { return m_parseError; }

    virtual Utils::optional<ResponseHandler> responseHandler() const
    { return Utils::nullopt; }

    BaseMessage toBaseMessage() const
    { return BaseMessage(jsonRpcMimeType(), toRawData()); }

protected:
    QJsonObject m_jsonObject;

private:
    QString m_parseError;
};

template <typename Params>
class Notification : public JsonRpcMessage
{
public:
    Notification(const QString &methodName, const Params &params)
    {
        setMethod(methodName);
        setParams(params);
    }

    explicit Notification(const QJsonObject &jsonObject) : JsonRpcMessage(jsonObject) {}
    explicit Notification(QJsonObject &&jsonObject) : JsonRpcMessage(std::move(jsonObject)) {}

    QString method() const
    { return fromJsonValue<QString>(m_jsonObject.value(methodKey)); }
    void setMethod(const QString &method)
    { m_jsonObject.insert(methodKey, method); }

    Utils::optional<Params> params() const
    {
        const QJsonValue &params = m_jsonObject.value(paramsKey);
        return params.isUndefined() ? Utils::nullopt : Utils::make_optional(Params(params));
    }
    void setParams(const Params &params)
    { m_jsonObject.insert(paramsKey, QJsonValue(params)); }
    void clearParams() { m_jsonObject.remove(paramsKey); }

    bool isValid(QString *errorMessage) const override
    {
        return JsonRpcMessage::isValid(errorMessage)
                && m_jsonObject.value(methodKey).isString()
                && parametersAreValid(errorMessage);
    }

    virtual bool parametersAreValid(QString *errorMessage) const
    {
        if (auto parameter = params())
            return parameter->isValid();
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LanguageServerProtocol::Notification",
                                                        "No parameters in \"%1\".").arg(method());
        return false;
    }
};

template <>
class Notification<std::nullptr_t> : public JsonRpcMessage
{
public:
    Notification() : Notification(QString()) {}
    explicit Notification(const QString &methodName, const std::nullptr_t &/*params*/ = nullptr)
    {
        setMethod(methodName);
        setParams(nullptr);
    }
    using JsonRpcMessage::JsonRpcMessage;

    QString method() const
    { return fromJsonValue<QString>(m_jsonObject.value(methodKey)); }
    void setMethod(const QString &method)
    { m_jsonObject.insert(methodKey, method); }

    Utils::optional<std::nullptr_t> params() const
    { return nullptr; }
    void setParams(const std::nullptr_t &/*params*/)
    { m_jsonObject.insert(paramsKey, QJsonValue::Null); }
    void clearParams() { m_jsonObject.remove(paramsKey); }

    bool isValid(QString *errorMessage) const override
    {
        return JsonRpcMessage::isValid(errorMessage)
               && m_jsonObject.value(methodKey).isString()
               && parametersAreValid(errorMessage);
    }

    virtual bool parametersAreValid(QString * /*errorMessage*/) const
    { return true; }
};

template <typename Error>
class ResponseError : public JsonObject
{
public:
    using JsonObject::JsonObject;

    int code() const { return typedValue<int>(codeKey); }
    void setCode(int code) { insert(codeKey, code); }

    QString message() const { return typedValue<QString>(messageKey); }
    void setMessage(const QString &message) { insert(messageKey, message); }

    Utils::optional<Error> data() const { return optionalValue<Error>(dataKey); }
    void setData(const Error &data) { insert(dataKey, data); }
    void clearData() { remove(dataKey); }

    bool isValid() const override { return contains(codeKey) && contains(messageKey); }

    QString toString() const { return errorCodesToString(code()) + ": " + message(); }

    // predefined error codes
    enum ErrorCodes {
        // Defined by JSON RPC
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        serverErrorStart = -32099,
        serverErrorEnd = -32000,
        ServerNotInitialized = -32002,
        UnknownErrorCode = -32001,

        // Defined by the protocol.
        RequestCancelled = -32800
    };

#define CASE_ERRORCODES(x) case ErrorCodes::x: return QLatin1String(#x)
    static QString errorCodesToString(int code)
    {
        switch (code) {
        CASE_ERRORCODES(ParseError);
        CASE_ERRORCODES(InvalidRequest);
        CASE_ERRORCODES(MethodNotFound);
        CASE_ERRORCODES(InvalidParams);
        CASE_ERRORCODES(InternalError);
        CASE_ERRORCODES(serverErrorStart);
        CASE_ERRORCODES(serverErrorEnd);
        CASE_ERRORCODES(ServerNotInitialized);
        CASE_ERRORCODES(UnknownErrorCode);
        CASE_ERRORCODES(RequestCancelled);
        default:
            return QCoreApplication::translate("LanguageClient::ResponseError",
                                               "Error %1").arg(code);
        }
    }
#undef CASE_ERRORCODES
};

template <typename Result, typename ErrorDataType>
class Response : public JsonRpcMessage
{
public:
    explicit Response(const MessageId &id) { setId(id); }
    using JsonRpcMessage::JsonRpcMessage;

    MessageId id() const
    { return MessageId(m_jsonObject.value(idKey)); }
    void setId(MessageId id)
    { this->m_jsonObject.insert(idKey, id); }

    Utils::optional<Result> result() const
    {
        const QJsonValue &result = m_jsonObject.value(resultKey);
        if (result.isUndefined())
            return Utils::nullopt;
        return Utils::make_optional(Result(result));
    }
    void setResult(const Result &result) { m_jsonObject.insert(resultKey, QJsonValue(result)); }
    void clearResult() { m_jsonObject.remove(resultKey); }

    using Error = ResponseError<ErrorDataType>;
    Utils::optional<Error> error() const
    {
        const QJsonValue &val = m_jsonObject.value(errorKey);
        return val.isUndefined() ? Utils::nullopt
                                 : Utils::make_optional(fromJsonValue<Error>(val));
    }
    void setError(const Error &error)
    { m_jsonObject.insert(errorKey, QJsonValue(error)); }
    void clearError() { m_jsonObject.remove(errorKey); }

    bool isValid(QString *errorMessage) const override
    { return JsonRpcMessage::isValid(errorMessage) && id().isValid(); }
};

template<typename ErrorDataType>
class Response<std::nullptr_t, ErrorDataType> : public JsonRpcMessage
{
public:
    explicit Response(const MessageId &id) { setId(id); }
    using JsonRpcMessage::JsonRpcMessage;

    MessageId id() const
    { return MessageId(m_jsonObject.value(idKey)); }
    void setId(MessageId id)
    { this->m_jsonObject.insert(idKey, id); }

    Utils::optional<std::nullptr_t> result() const
    {
        return m_jsonObject.value(resultKey).isNull() ? Utils::make_optional(nullptr)
                                                      : Utils::nullopt;
    }
    void setResult(const std::nullptr_t &) { m_jsonObject.insert(resultKey, QJsonValue::Null); }
    void clearResult() { m_jsonObject.remove(resultKey); }

    using Error = ResponseError<ErrorDataType>;
    Utils::optional<Error> error() const
    {
        const QJsonValue &val = m_jsonObject.value(errorKey);
        return val.isUndefined() ? Utils::nullopt
                                 : Utils::make_optional(fromJsonValue<Error>(val));
    }
    void setError(const Error &error)
    { m_jsonObject.insert(errorKey, QJsonValue(error)); }
    void clearError() { m_jsonObject.remove(errorKey); }

    bool isValid(QString *errorMessage) const override
    { return JsonRpcMessage::isValid(errorMessage) && id().isValid(); }
};

void LANGUAGESERVERPROTOCOL_EXPORT logElapsedTime(const QString &method, const QElapsedTimer &t);

template <typename Result, typename ErrorDataType, typename Params>
class Request : public Notification<Params>
{
public:
    Request(const QString &methodName, const Params &params)
        : Notification<Params>(methodName, params)
    { setId(MessageId(QUuid::createUuid().toString())); }
    explicit Request(const QJsonObject &jsonObject) : Notification<Params>(jsonObject) { }
    explicit Request(QJsonObject &&jsonObject) : Notification<Params>(std::move(jsonObject)) { }

    MessageId id() const
    { return MessageId(JsonRpcMessage::m_jsonObject.value(idKey)); }
    void setId(const MessageId &id)
    { JsonRpcMessage::m_jsonObject.insert(idKey, id); }

    using Response = LanguageServerProtocol::Response<Result, ErrorDataType>;
    using ResponseCallback = std::function<void(Response)>;
    void setResponseCallback(const ResponseCallback &callback)
    { m_callBack = callback; }

    Utils::optional<ResponseHandler> responseHandler() const final
    {
        QElapsedTimer timer;
        timer.start();
        auto callback = [callback = m_callBack, method = this->method(), t = std::move(timer)]
                (const JsonRpcMessage &message) {
            if (!callback)
                return;
            logElapsedTime(method, t);

            callback(Response(message.toJsonObject()));
        };
        return Utils::make_optional(ResponseHandler{id(), callback});
    }

    bool isValid(QString *errorMessage) const override
    {
        if (!Notification<Params>::isValid(errorMessage))
            return false;
        if (id().isValid())
            return true;
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("LanguageServerProtocol::Request",
                                                        "No ID set in \"%1\".").arg(this->method());
        }
        return false;
    }

private:
    ResponseCallback m_callBack;
};

class LANGUAGESERVERPROTOCOL_EXPORT CancelParameter : public JsonObject
{
public:
    explicit CancelParameter(const MessageId &id) { setId(id); }
    CancelParameter() = default;
    using JsonObject::JsonObject;

    MessageId id() const { return typedValue<MessageId>(idKey); }
    void setId(const MessageId &id) { insert(idKey, id); }

    bool isValid() const override { return contains(idKey); }
};

class LANGUAGESERVERPROTOCOL_EXPORT CancelRequest : public Notification<CancelParameter>
{
public:
    explicit CancelRequest(const CancelParameter &params);
    using Notification::Notification;
    constexpr static const char methodName[] = "$/cancelRequest";
};

} // namespace LanguageServerProtocol

template <typename Error>
inline QDebug operator<<(QDebug stream,
                const LanguageServerProtocol::ResponseError<Error> &error)
{
    stream.nospace() << error.toString();
    return stream;
}

Q_DECLARE_METATYPE(LanguageServerProtocol::JsonRpcMessage)
