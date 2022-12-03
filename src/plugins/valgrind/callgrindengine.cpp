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

#include "callgrindengine.h"

#include "valgrindsettings.h"

#include <valgrind/callgrind/callgrindparser.h>
#include <valgrind/valgrindrunner.h>

#include <debugger/analyzer/analyzermanager.h>

#include <utils/filepath.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/temporaryfile.h>

#include <QDebug>

#define CALLGRIND_CONTROL_DEBUG 0

using namespace ProjectExplorer;
using namespace Valgrind::Callgrind;
using namespace Utils;

namespace Valgrind {
namespace Internal {

const char CALLGRIND_CONTROL_BINARY[] = "callgrind_control";

void setupCallgrindRunner(CallgrindToolRunner *);

CallgrindToolRunner::CallgrindToolRunner(RunControl *runControl)
    : ValgrindToolRunner(runControl)
{
    setId("CallgrindToolRunner");

    connect(&m_runner, &ValgrindRunner::valgrindStarted, this, [this](qint64 pid) {
        m_pid = pid;
    });
    connect(&m_runner, &ValgrindRunner::finished, this, [this] {
        triggerParse();
        emit parserDataReady(this);
    });
    connect(&m_parser, &Callgrind::Parser::parserDataReady, this, [this] {
        emit parserDataReady(this);
    });

    m_valgrindRunnable = runControl->runnable();

    static int fileCount = 100;
    m_valgrindOutputFile = runControl->workingDirectory() / QString("callgrind.out.f%1").arg(++fileCount);

    setupCallgrindRunner(this);
}

CallgrindToolRunner::~CallgrindToolRunner()
{
    cleanupTempFile();
}

QStringList CallgrindToolRunner::toolArguments() const
{
    QStringList arguments = {"--tool=callgrind"};

    if (m_settings.enableCacheSim.value())
        arguments << "--cache-sim=yes";

    if (m_settings.enableBranchSim.value())
        arguments << "--branch-sim=yes";

    if (m_settings.collectBusEvents.value())
        arguments << "--collect-bus=yes";

    if (m_settings.collectSystime.value())
        arguments << "--collect-systime=yes";

    if (m_markAsPaused)
        arguments << "--instr-atstart=no";

    // add extra arguments
    if (!m_argumentForToggleCollect.isEmpty())
        arguments << m_argumentForToggleCollect;

    arguments << "--callgrind-out-file=" + m_valgrindOutputFile.path();

    arguments << ProcessArgs::splitArgs(m_settings.callgrindArguments.value());

    return arguments;
}

QString CallgrindToolRunner::progressTitle() const
{
    return tr("Profiling");
}

void CallgrindToolRunner::start()
{
    const FilePath executable = runControl()->commandLine().executable();
    appendMessage(tr("Profiling %1").arg(executable.toUserOutput()), NormalMessageFormat);
    return ValgrindToolRunner::start();
}

void CallgrindToolRunner::setPaused(bool paused)
{
    if (m_markAsPaused == paused)
        return;

    m_markAsPaused = paused;

    // call controller only if it is attached to a valgrind process
    if (paused)
        pause();
    else
        unpause();
}

void CallgrindToolRunner::setToggleCollectFunction(const QString &toggleCollectFunction)
{
    if (toggleCollectFunction.isEmpty())
        return;

    m_argumentForToggleCollect = "--toggle-collect=" + toggleCollectFunction;
}

Callgrind::ParseData *CallgrindToolRunner::takeParserData()
{
    return m_parser.takeData();
}

void CallgrindToolRunner::showStatusMessage(const QString &message)
{
    Debugger::showPermanentStatusMessage(message);
}

static QString toOptionString(CallgrindToolRunner::Option option)
{
    /* callgrind_control help from v3.9.0

    Options:
    -h --help        Show this help text
    --version        Show version
    -s --stat        Show statistics
    -b --back        Show stack/back trace
    -e [<A>,...]     Show event counters for <A>,... (default: all)
    --dump[=<s>]     Request a dump optionally using <s> as description
    -z --zero        Zero all event counters
    -k --kill        Kill
    --instr=<on|off> Switch instrumentation state on/off
    */

    switch (option) {
        case CallgrindToolRunner::Dump:
            return QLatin1String("--dump");
        case CallgrindToolRunner::ResetEventCounters:
            return QLatin1String("--zero");
        case CallgrindToolRunner::Pause:
            return QLatin1String("--instr=off");
        case CallgrindToolRunner::UnPause:
            return QLatin1String("--instr=on");
        default:
            return QString(); // never reached
    }
}

void CallgrindToolRunner::run(Option option)
{
    if (m_controllerProcess) {
        showStatusMessage(tr("Previous command has not yet finished."));
        return;
    }

    // save back current running operation
    m_lastOption = option;

    m_controllerProcess.reset(new QtcProcess);

    switch (option) {
        case CallgrindToolRunner::Dump:
            showStatusMessage(tr("Dumping profile data..."));
            break;
        case CallgrindToolRunner::ResetEventCounters:
            showStatusMessage(tr("Resetting event counters..."));
            break;
        case CallgrindToolRunner::Pause:
            showStatusMessage(tr("Pausing instrumentation..."));
            break;
        case CallgrindToolRunner::UnPause:
            showStatusMessage(tr("Unpausing instrumentation..."));
            break;
        default:
            break;
    }

#if CALLGRIND_CONTROL_DEBUG
    m_controllerProcess->setProcessChannelMode(QProcess::ForwardedChannels);
#endif
    connect(m_controllerProcess.get(), &QtcProcess::done,
            this, &CallgrindToolRunner::controllerProcessDone);

    const FilePath control =
            FilePath(CALLGRIND_CONTROL_BINARY).onDevice(m_valgrindRunnable.command.executable());
    m_controllerProcess->setCommand({control, {toOptionString(option), QString::number(m_pid)}});
    m_controllerProcess->setWorkingDirectory(m_valgrindRunnable.workingDirectory);
    m_controllerProcess->setEnvironment(m_valgrindRunnable.environment);
    m_controllerProcess->start();
}

void CallgrindToolRunner::controllerProcessDone()
{
    const QString error = m_controllerProcess->errorString();
    const ProcessResult result = m_controllerProcess->result();

    m_controllerProcess.release()->deleteLater();

    if (result != ProcessResult::FinishedWithSuccess) {
        showStatusMessage(tr("An error occurred while trying to run %1: %2").arg(CALLGRIND_CONTROL_BINARY).arg(error));
        qWarning() << "Controller exited abnormally:" << error;
        return;
    }

    // this call went fine, we might run another task after this
    switch (m_lastOption) {
        case ResetEventCounters:
            // lets dump the new reset profiling info
            run(Dump);
            return;
        case Pause:
            m_paused = true;
            break;
        case Dump:
            showStatusMessage(tr("Callgrind dumped profiling info"));
            triggerParse();
            break;
        case UnPause:
            m_paused = false;
            showStatusMessage(tr("Callgrind unpaused."));
            break;
        default:
            break;
    }

    m_lastOption = Unknown;
}

void CallgrindToolRunner::triggerParse()
{
    cleanupTempFile();
    {
        TemporaryFile dataFile("callgrind.out");
        if (!dataFile.open()) {
            showStatusMessage(tr("Failed opening temp file..."));
            return;
        }
        m_hostOutputFile = FilePath::fromString(dataFile.fileName());
    }

    const auto afterCopy = [this](bool res) {
        QTC_CHECK(res);
        QTC_ASSERT(m_hostOutputFile.exists(), return);
        showStatusMessage(tr("Parsing Profile Data..."));
        m_parser.parse(m_hostOutputFile);
    };
    m_valgrindOutputFile.asyncCopyFile(afterCopy, m_hostOutputFile);
}

void CallgrindToolRunner::cleanupTempFile()
{
    if (!m_hostOutputFile.isEmpty() && m_hostOutputFile.exists())
        m_hostOutputFile.removeFile();

    m_hostOutputFile.clear();
}

} // Internal
} // Valgrind
