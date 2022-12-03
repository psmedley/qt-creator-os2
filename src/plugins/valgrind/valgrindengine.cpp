/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Author: Nicolas Arnaud-Cormos, KDAB (nicolas.arnaud-cormos@kdab.com)
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

#include "valgrindengine.h"
#include "valgrindsettings.h"

#include <debugger/analyzer/analyzermanager.h>

#include <coreplugin/icore.h>
#include <coreplugin/ioutputpane.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <coreplugin/progressmanager/futureprogress.h>
#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorericons.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/runconfigurationaspects.h>

#include <QApplication>

#define VALGRIND_DEBUG_OUTPUT 0

using namespace Debugger;
using namespace Core;
using namespace Utils;
using namespace ProjectExplorer;

namespace Valgrind {
namespace Internal {

ValgrindToolRunner::ValgrindToolRunner(RunControl *runControl)
    : RunWorker(runControl)
{
    runControl->setIcon(ProjectExplorer::Icons::ANALYZER_START_SMALL_TOOLBAR);
    setSupportsReRunning(false);

    m_settings.fromMap(runControl->settingsData(ANALYZER_VALGRIND_SETTINGS));

    connect(&m_runner, &ValgrindRunner::appendMessage,
            this, &ValgrindToolRunner::appendMessage);
    connect(&m_runner, &ValgrindRunner::valgrindExecuted,
            this, [this](const QString &commandLine) {
        appendMessage(commandLine, NormalMessageFormat);
    });
    connect(&m_runner, &ValgrindRunner::processErrorReceived,
            this, &ValgrindToolRunner::receiveProcessError);
    connect(&m_runner, &ValgrindRunner::finished,
            this, &ValgrindToolRunner::runnerFinished);
}

void ValgrindToolRunner::start()
{
    FutureProgress *fp = ProgressManager::addTimedTask(m_progress, progressTitle(), "valgrind", 100);
    fp->setKeepOnFinish(FutureProgress::HideOnFinish);
    connect(fp, &FutureProgress::canceled,
            this, &ValgrindToolRunner::handleProgressCanceled);
    connect(fp, &FutureProgress::finished,
            this, &ValgrindToolRunner::handleProgressFinished);
    m_progress.reportStarted();

#if VALGRIND_DEBUG_OUTPUT
    emit outputReceived(tr("Valgrind options: %1").arg(toolArguments().join(' ')), LogMessageFormat);
    emit outputReceived(tr("Working directory: %1").arg(runnable().workingDirectory), LogMessageFormat);
    emit outputReceived(tr("Command line arguments: %1").arg(runnable().debuggeeArgs), LogMessageFormat);
#endif


    FilePath valgrindExecutable = m_settings.valgrindExecutable.filePath();
    if (IDevice::ConstPtr dev = DeviceKitAspect::device(runControl()->kit()))
        valgrindExecutable = dev->filePath(valgrindExecutable.path());

    CommandLine valgrind{valgrindExecutable};
    valgrind.addArgs(m_settings.valgrindArguments.value(), CommandLine::Raw);
    valgrind.addArgs(genericToolArguments());
    valgrind.addArgs(toolArguments());

    m_runner.setValgrindCommand(valgrind);
    m_runner.setDebuggee(runControl()->runnable());

    if (auto aspect = runControl()->aspect<TerminalAspect>())
        m_runner.setUseTerminal(aspect->useTerminal);

    if (!m_runner.start()) {
        m_progress.cancel();
        reportFailure();
        return;
    }

    reportStarted();
}

void ValgrindToolRunner::stop()
{
    m_isStopping = true;
    m_runner.stop();
}

QStringList ValgrindToolRunner::genericToolArguments() const
{
    QString smcCheckValue;

    switch (m_settings.selfModifyingCodeDetection.value()) {
    case ValgrindBaseSettings::DetectSmcNo:
        smcCheckValue = "none";
        break;
    case ValgrindBaseSettings::DetectSmcEverywhere:
        smcCheckValue = "all";
        break;
    case ValgrindBaseSettings::DetectSmcEverywhereButFile:
        smcCheckValue = "all-non-file";
        break;
    case ValgrindBaseSettings::DetectSmcStackOnly:
    default:
        smcCheckValue = "stack";
        break;
    }
    return {"--smc-check=" + smcCheckValue};
}

void ValgrindToolRunner::handleProgressCanceled()
{
    m_progress.reportCanceled();
    m_progress.reportFinished();
}

void ValgrindToolRunner::handleProgressFinished()
{
    QApplication::alert(ICore::dialogParent(), 3000);
}

void ValgrindToolRunner::runnerFinished()
{
    appendMessage(tr("Analyzing finished."), NormalMessageFormat);

    m_progress.reportFinished();

    reportStopped();
}

void ValgrindToolRunner::receiveProcessError(const QString &message, QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        const QString valgrind = m_settings.valgrindExecutable.value();
        if (!valgrind.isEmpty())
            appendMessage(tr("Error: \"%1\" could not be started: %2").arg(valgrind, message), ErrorMessageFormat);
        else
            appendMessage(tr("Error: no Valgrind executable set."), ErrorMessageFormat);
    } else if (m_isStopping && error == QProcess::Crashed) { // process gets killed on stop
        appendMessage(tr("Process terminated."), ErrorMessageFormat);
    } else {
        appendMessage(tr("Process exited with return value %1\n").arg(message), NormalMessageFormat);
    }

    if (m_isStopping)
        return;

    QObject *obj = ExtensionSystem::PluginManager::getObjectByName("AppOutputPane");
    if (auto pane = qobject_cast<IOutputPane *>(obj))
        pane->popup(IOutputPane::NoModeSwitch);
}

} // namespace Internal
} // namepsace Valgrind
