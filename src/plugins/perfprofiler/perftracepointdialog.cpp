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

#include "perftracepointdialog.h"
#include "ui_perftracepointdialog.h"

#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>

#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>

#include <QPushButton>
#include <QTimer>

using namespace ProjectExplorer;
using namespace Utils;

namespace PerfProfiler {
namespace Internal {

PerfTracePointDialog::PerfTracePointDialog() :
    m_ui(new Ui::PerfTracePointDialog)
{
    m_ui->setupUi(this);

    if (const Target *target = SessionManager::startupTarget()) {
        const Kit *kit = target->kit();
        QTC_ASSERT(kit, return);

        m_device = DeviceKitAspect::device(kit);
        if (!m_device) {
            m_ui->textEdit->setPlainText(tr("Error: No device available for active target."));
            return;
        }
    }

    if (!m_device) {
        // There should at least be a desktop device.
        m_device = DeviceManager::defaultDesktopDevice();
        QTC_ASSERT(m_device, return);
    }

    QFile file(":/perfprofiler/tracepoints.sh");
    if (file.open(QIODevice::ReadOnly)) {
        m_ui->textEdit->setPlainText(QString::fromUtf8(file.readAll()));
    } else {
        m_ui->textEdit->setPlainText(tr("Error: Failed to load trace point script %1: %2.")
                                         .arg(file.fileName()).arg(file.errorString()));
    }

    m_ui->privilegesChooser->setCurrentText(m_device->type() == Constants::DESKTOP_DEVICE_TYPE
                                            ? QLatin1String("pkexec") : QLatin1String("n.a."));
}

PerfTracePointDialog::~PerfTracePointDialog() = default;

void PerfTracePointDialog::runScript()
{
    m_ui->label->setText(tr("Executing script..."));
    m_ui->textEdit->setReadOnly(true);
    m_ui->privilegesChooser->setEnabled(false);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_process.reset(new QtcProcess(this));
    m_process->setWriteData(m_ui->textEdit->toPlainText().toUtf8());
    m_ui->textEdit->clear();

    const QString elevate = m_ui->privilegesChooser->currentText();
    if (elevate != QLatin1String("n.a."))
        m_process->setCommand({m_device->filePath(elevate), {"sh"}});
    else
        m_process->setCommand({m_device->filePath("sh"), {}});

    connect(m_process.get(), &QtcProcess::done, this, &PerfTracePointDialog::handleProcessDone);
    m_process->start();
}

void PerfTracePointDialog::handleProcessDone()
{
    const QProcess::ProcessError error = m_process->error();
    QString message;
    if (error == QProcess::FailedToStart) {
        message = tr("Failed to run trace point script: %1").arg(error);
    } else if ((m_process->exitStatus() == QProcess::CrashExit) || (m_process->exitCode() != 0)) {
        message = tr("Failed to create trace points.");
    } else {
        message = tr("Created trace points for: %1").arg(QString::fromUtf8(
            m_process->readAllStandardOutput().trimmed().replace('\n', ", ")));
    }
    m_ui->label->setText(message);
    m_ui->textEdit->setHtml(QString::fromUtf8(m_process->readAllStandardError()));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
}

void PerfTracePointDialog::accept()
{
    if (m_process) {
        QTC_CHECK(m_process->state() == QProcess::NotRunning);
        QDialog::accept();
    } else {
        runScript();
    }
}

void PerfTracePointDialog::reject()
{
    if (m_process)
        m_process->kill();
    else
        QDialog::reject();
}

} // namespace Internal
} // namespace PerfProfiler
