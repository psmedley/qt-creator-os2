/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "remotelinux_export.h"

#include "sshprocessinterface.h"

namespace RemoteLinux {

class LinuxDevice;
class SshProcessInterfacePrivate;

class REMOTELINUX_EXPORT LinuxProcessInterface : public SshProcessInterface
{
public:
    LinuxProcessInterface(const LinuxDevice *linuxDevice);
    ~LinuxProcessInterface();

private:
    void sendControlSignal(Utils::ControlSignal controlSignal) override;

    void handleStarted(qint64 processId) final;
    void handleDone(const Utils::ProcessResultData &resultData) final;
    void handleReadyReadStandardOutput(const QByteArray &outputData) final;
    void handleReadyReadStandardError(const QByteArray &errorData) final;

    QString fullCommandLine(const Utils::CommandLine &commandLine) const final;

    QByteArray m_output;
    QByteArray m_error;
    bool m_pidParsed = false;
};

} // namespace RemoteLinux
