/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "url.h"

#include "temporaryfile.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QUrl>

namespace Utils {

QUrl urlFromLocalHostAndFreePort()
{
    QUrl serverUrl;
    QTcpServer server;
    serverUrl.setScheme(urlTcpScheme());
    if (server.listen(QHostAddress::LocalHost) || server.listen(QHostAddress::LocalHostIPv6)) {
        serverUrl.setHost(server.serverAddress().toString());
        serverUrl.setPort(server.serverPort());
    }
    return serverUrl;
}

QUrl urlFromLocalSocket()
{
    QUrl serverUrl;
    serverUrl.setScheme(urlSocketScheme());
    TemporaryFile file("qtc-socket");
    // see "man unix" for unix socket file name size limitations
    if (file.fileName().size() > 104) {
        qWarning().nospace()
            << "Socket file name \"" << file.fileName()
            << "\" is larger than 104 characters, which will not work on Darwin/macOS/Linux!";
    }
    if (file.open())
        serverUrl.setPath(file.fileName());
    return serverUrl;
}

QString urlSocketScheme()
{
    return QString("socket");
}

QString urlTcpScheme()
{
    return QString("tcp");
}

}
