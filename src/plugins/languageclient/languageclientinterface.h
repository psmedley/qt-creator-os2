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

#include "languageclient_global.h"

#include <languageserverprotocol/jsonrpcmessages.h>

#include <utils/environment.h>
#include <utils/qtcprocess.h>

#include <QBuffer>

namespace LanguageClient {

class StdIOSettings;

class LANGUAGECLIENT_EXPORT BaseClientInterface : public QObject
{
    Q_OBJECT
public:
    BaseClientInterface();

    ~BaseClientInterface() override;

    void sendMessage(const LanguageServerProtocol::JsonRpcMessage message);
    void start() { startImpl(); }

    void resetBuffer();

signals:
    void messageReceived(const LanguageServerProtocol::JsonRpcMessage message);
    void finished();
    void error(const QString &message);
    void started();

protected:
    virtual void startImpl() { emit started(); }
    virtual void sendData(const QByteArray &data) = 0;
    void parseData(const QByteArray &data);
    virtual void parseCurrentMessage();

private:
    QBuffer m_buffer;
    LanguageServerProtocol::BaseMessage m_currentMessage;
};

class LANGUAGECLIENT_EXPORT StdIOClientInterface : public BaseClientInterface
{
    Q_OBJECT
public:
    StdIOClientInterface() = default;
    ~StdIOClientInterface() override;

    StdIOClientInterface(const StdIOClientInterface &) = delete;
    StdIOClientInterface(StdIOClientInterface &&) = delete;
    StdIOClientInterface &operator=(const StdIOClientInterface &) = delete;
    StdIOClientInterface &operator=(StdIOClientInterface &&) = delete;

    void startImpl() override;

    // These functions only have an effect if they are called before start
    void setCommandLine(const Utils::CommandLine &cmd);
    void setWorkingDirectory(const Utils::FilePath &workingDirectory);
    void setEnvironment(const Utils::Environment &environment);

protected:
    void sendData(const QByteArray &data) final;
    Utils::CommandLine m_cmd;
    Utils::FilePath m_workingDirectory;
    Utils::QtcProcess *m_process = nullptr;
    Utils::Environment m_env;

private:
    void readError();
    void readOutput();
};

} // namespace LanguageClient
