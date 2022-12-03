/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "runcontrol.h"

#include "buildconfiguration.h"
#include "customparser.h"
#include "devicesupport/desktopdevice.h"
#include "devicesupport/devicemanager.h"
#include "devicesupport/idevice.h"
#include "devicesupport/sshsettings.h"
#include "kitinformation.h"
#include "project.h"
#include "projectexplorer.h"
#include "projectexplorersettings.h"
#include "runconfigurationaspects.h"
#include "runcontrol.h"
#include "session.h"
#include "target.h"
#include "windebuginterface.h"

#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <utils/algorithm.h>
#include <utils/checkablemessagebox.h>
#include <utils/detailswidget.h>
#include <utils/fileinprojectfinder.h>
#include <utils/fileutils.h>
#include <utils/outputformatter.h>
#include <utils/processinterface.h>
#include <utils/processinterface.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/utilsicons.h>
#include <utils/variablechooser.h>

#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <QDir>
#include <QFormLayout>
#include <QHash>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSettings>
#include <QTextCodec>
#include <QTimer>


#if defined (WITH_JOURNALD)
#include "journaldwatcher.h"
#endif

using namespace ProjectExplorer::Internal;
using namespace Utils;

namespace {
static Q_LOGGING_CATEGORY(statesLog, "qtc.projectmanager.states", QtWarningMsg)
}

namespace ProjectExplorer {

// RunWorkerFactory

static QList<RunWorkerFactory *> g_runWorkerFactories;

static QSet<Utils::Id> g_runModes;
static QSet<Utils::Id> g_runConfigs;

RunWorkerFactory::RunWorkerFactory()
{
    g_runWorkerFactories.append(this);
}

RunWorkerFactory::RunWorkerFactory(const WorkerCreator &producer,
                                   const QList<Utils::Id> &runModes,
                                   const QList<Utils::Id> &runConfigs,
                                   const QList<Utils::Id> &deviceTypes)
        : m_producer(producer),
          m_supportedRunModes(runModes),
          m_supportedRunConfigurations(runConfigs),
          m_supportedDeviceTypes(deviceTypes)
{
    g_runWorkerFactories.append(this);

    // Debugging only.
    for (Utils::Id runMode : runModes)
        g_runModes.insert(runMode);
    for (Utils::Id runConfig : runConfigs)
        g_runConfigs.insert(runConfig);
}

RunWorkerFactory::~RunWorkerFactory()
{
    g_runWorkerFactories.removeOne(this);
}

void RunWorkerFactory::setProducer(const WorkerCreator &producer)
{
    m_producer = producer;
}

void RunWorkerFactory::addSupportedRunMode(Utils::Id runMode)
{
    m_supportedRunModes.append(runMode);
}

void RunWorkerFactory::addSupportedRunConfig(Utils::Id runConfig)
{
    m_supportedRunConfigurations.append(runConfig);
}

void RunWorkerFactory::addSupportedDeviceType(Utils::Id deviceType)
{
    m_supportedDeviceTypes.append(deviceType);
}

bool RunWorkerFactory::canRun(Utils::Id runMode,
                              Utils::Id deviceType,
                              const QString &runConfigId) const
{
    if (!m_supportedRunModes.contains(runMode))
        return false;

    if (!m_supportedRunConfigurations.isEmpty()) {
        // FIXME: That's to be used after mangled ids are gone.
        //if (!m_supportedRunConfigurations.contains(runConfigId)
        // return false;
        bool ok = false;
        for (const Utils::Id &id : m_supportedRunConfigurations) {
            if (runConfigId.startsWith(id.toString())) {
                ok = true;
                break;
            }
        }

        if (!ok)
            return false;
    }

    if (!m_supportedDeviceTypes.isEmpty())
        return m_supportedDeviceTypes.contains(deviceType);

    return true;
}

void RunWorkerFactory::dumpAll()
{
    const QList<Utils::Id> devices =
            Utils::transform(IDeviceFactory::allDeviceFactories(), &IDeviceFactory::deviceType);

    for (Utils::Id runMode : qAsConst(g_runModes)) {
        qDebug() << "";
        for (Utils::Id device : devices) {
            for (Utils::Id runConfig : qAsConst(g_runConfigs)) {
                const auto check = std::bind(&RunWorkerFactory::canRun,
                                             std::placeholders::_1,
                                             runMode,
                                             device,
                                             runConfig.toString());
                const auto factory = Utils::findOrDefault(g_runWorkerFactories, check);
                qDebug() << "MODE:" << runMode << device << runConfig << factory;
            }
        }
    }
}

/*!
    \class ProjectExplorer::RunControl
    \brief The RunControl class instances represent one item that is run.
*/

/*!
    \fn QIcon ProjectExplorer::RunControl::icon() const
    Returns the icon to be shown in the Outputwindow.

    TODO the icon differs currently only per "mode", so this is more flexible
    than it needs to be.
*/


namespace Internal {

enum class RunWorkerState
{
    Initialized, Starting, Running, Stopping, Done
};

static QString stateName(RunWorkerState s)
{
#    define SN(x) case x: return QLatin1String(#x);
    switch (s) {
        SN(RunWorkerState::Initialized)
        SN(RunWorkerState::Starting)
        SN(RunWorkerState::Running)
        SN(RunWorkerState::Stopping)
        SN(RunWorkerState::Done)
    }
    return QString("<unknown: %1>").arg(int(s));
#    undef SN
}

class RunWorkerPrivate : public QObject
{
public:
    RunWorkerPrivate(RunWorker *runWorker, RunControl *runControl);

    bool canStart() const;
    bool canStop() const;
    void timerEvent(QTimerEvent *ev) override;

    void killStartWatchdog()
    {
        if (startWatchdogTimerId != -1) {
            killTimer(startWatchdogTimerId);
            startWatchdogTimerId = -1;
        }
    }

    void killStopWatchdog()
    {
        if (stopWatchdogTimerId != -1) {
            killTimer(stopWatchdogTimerId);
            stopWatchdogTimerId = -1;
        }
    }

    void startStartWatchdog()
    {
        killStartWatchdog();
        killStopWatchdog();

        if (startWatchdogInterval != 0)
            startWatchdogTimerId = startTimer(startWatchdogInterval);
    }

    void startStopWatchdog()
    {
        killStopWatchdog();
        killStartWatchdog();

        if (stopWatchdogInterval != 0)
            stopWatchdogTimerId = startTimer(stopWatchdogInterval);
    }

    RunWorker *q;
    RunWorkerState state = RunWorkerState::Initialized;
    const QPointer<RunControl> runControl;
    QList<RunWorker *> startDependencies;
    QList<RunWorker *> stopDependencies;
    QString id;

    QVariantMap data;
    int startWatchdogInterval = 0;
    int startWatchdogTimerId = -1;
    std::function<void()> startWatchdogCallback;
    int stopWatchdogInterval = 0; // 5000;
    int stopWatchdogTimerId = -1;
    std::function<void()> stopWatchdogCallback;
    bool supportsReRunning = true;
    bool essential = false;
};

enum class RunControlState
{
    Initialized,      // Default value after creation.
    Starting,         // Actual process/tool starts.
    Running,          // All good and running.
    Stopping,         // initiateStop() was called, stop application/tool
    Stopped,          // all good, but stopped. Can possibly be re-started
    Finishing,        // Application tab manually closed
    Finished          // Final state, will self-destruct with deleteLater()
};

static QString stateName(RunControlState s)
{
#    define SN(x) case x: return QLatin1String(#x);
    switch (s) {
        SN(RunControlState::Initialized)
        SN(RunControlState::Starting)
        SN(RunControlState::Running)
        SN(RunControlState::Stopping)
        SN(RunControlState::Stopped)
        SN(RunControlState::Finishing)
        SN(RunControlState::Finished)
    }
    return QString("<unknown: %1>").arg(int(s));
#    undef SN
}

class RunControlPrivateData
{
public:
    QString displayName;
    Runnable runnable;
    IDevice::ConstPtr device;
    Utils::Icon icon;
    const MacroExpander *macroExpander = nullptr;
    AspectContainerData aspectData;
    QString buildKey;
    QMap<Utils::Id, QVariantMap> settingsData;
    Utils::Id runConfigId;
    BuildTargetInfo buildTargetInfo;
    FilePath buildDirectory;
    Environment buildEnvironment;
    Kit *kit = nullptr; // Not owned.
    QPointer<Target> target; // Not owned.
    QPointer<Project> project; // Not owned.
    std::function<bool(bool*)> promptToStop;
    std::vector<RunWorkerFactory> m_factories;

    // A handle to the actual application process.
    Utils::ProcessHandle applicationProcessHandle;

    RunControlState state = RunControlState::Initialized;

    QList<QPointer<RunWorker>> m_workers;
};

class RunControlPrivate : public QObject, public RunControlPrivateData
{
    Q_OBJECT

public:
    RunControlPrivate(RunControl *parent, Utils::Id mode)
        : q(parent), runMode(mode)
    {
        icon = Icons::RUN_SMALL_TOOLBAR;
    }

    ~RunControlPrivate() override
    {
        QTC_CHECK(state == RunControlState::Finished || state == RunControlState::Initialized);
        disconnect();
        q = nullptr;
        qDeleteAll(m_workers);
        m_workers.clear();
    }

    void copyData(RunControlPrivateData *other) { RunControlPrivateData::operator=(*other); }

    Q_ENUM(RunControlState)

    void checkState(RunControlState expectedState);
    void setState(RunControlState state);

    void debugMessage(const QString &msg);

    void initiateStart();
    void initiateReStart();
    void continueStart();
    void initiateStop();
    void forceStop();
    void continueStopOrFinish();
    void initiateFinish();

    void onWorkerStarted(RunWorker *worker);
    void onWorkerStopped(RunWorker *worker);
    void onWorkerFailed(RunWorker *worker, const QString &msg);

    void showError(const QString &msg);

    static bool isAllowedTransition(RunControlState from, RunControlState to);
    bool supportsReRunning() const;

    RunControl *q;
    Utils::Id runMode;
};

} // Internal

using namespace Internal;

RunControl::RunControl(Utils::Id mode) :
    d(std::make_unique<RunControlPrivate>(this,  mode))
{
}

void RunControl::copyDataFromRunControl(RunControl *runControl)
{
    QTC_ASSERT(runControl, return);
    d->copyData(runControl->d.get());
}

void RunControl::copyDataFromRunConfiguration(RunConfiguration *runConfig)
{
    QTC_ASSERT(runConfig, return);
    d->runConfigId = runConfig->id();
    d->runnable = runConfig->runnable();
    d->displayName = runConfig->expandedDisplayName();
    d->buildKey = runConfig->buildKey();
    d->settingsData = runConfig->settingsData();
    d->aspectData = runConfig->aspectData();

    setTarget(runConfig->target());

    d->macroExpander = runConfig->macroExpander();
}

void RunControl::setTarget(Target *target)
{
    QTC_ASSERT(target, return);
    QTC_CHECK(!d->target);
    d->target = target;

    if (!d->buildKey.isEmpty() && target->buildSystem())
        d->buildTargetInfo = target->buildTarget(d->buildKey);

    if (auto bc = target->activeBuildConfiguration()) {
        d->buildDirectory = bc->buildDirectory();
        d->buildEnvironment = bc->environment();
    }

    setKit(target->kit());
    d->macroExpander = target->macroExpander();
    d->project = target->project();
}

void RunControl::setKit(Kit *kit)
{
    QTC_ASSERT(kit, return);
    QTC_CHECK(!d->kit);
    d->kit = kit;
    d->macroExpander = kit->macroExpander();

    if (!d->runnable.command.isEmpty())
        setDevice(DeviceManager::deviceForPath(d->runnable.command.executable()));
    else
        setDevice(DeviceKitAspect::device(kit));
}

void RunControl::setDevice(const IDevice::ConstPtr &device)
{
    QTC_CHECK(!d->device);
    d->device = device;
#ifdef WITH_JOURNALD
    if (!device.isNull() && device->type() == ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE) {
        JournaldWatcher::instance()->subscribe(this, [this](const JournaldWatcher::LogEntry &entry) {

            if (entry.value("_MACHINE_ID") != JournaldWatcher::instance()->machineId())
                return;

            const QByteArray pid = entry.value("_PID");
            if (pid.isEmpty())
                return;

            const qint64 pidNum = static_cast<qint64>(QString::fromLatin1(pid).toInt());
            if (pidNum != d->applicationProcessHandle.pid())
                return;

            const QString message = QString::fromUtf8(entry.value("MESSAGE")) + "\n";
            appendMessage(message, Utils::OutputFormat::LogMessageFormat);
        });
    }
#endif
}

RunControl::~RunControl()
{
#ifdef WITH_JOURNALD
    JournaldWatcher::instance()->unsubscribe(this);
#endif
}

void RunControl::initiateStart()
{
    emit aboutToStart();
    d->initiateStart();
}

void RunControl::initiateReStart()
{
    emit aboutToStart();
    d->initiateReStart();
}

void RunControl::initiateStop()
{
    d->initiateStop();
}

void RunControl::forceStop()
{
    d->forceStop();
}

void RunControl::initiateFinish()
{
    QTimer::singleShot(0, d.get(), &RunControlPrivate::initiateFinish);
}

RunWorker *RunControl::createWorker(Utils::Id workerId)
{
    const auto check = std::bind(&RunWorkerFactory::canRun,
                                 std::placeholders::_1,
                                 workerId,
                                 DeviceTypeKitAspect::deviceTypeId(d->kit),
                                 QString{});
    RunWorkerFactory *factory = Utils::findOrDefault(g_runWorkerFactories, check);
    return factory ? factory->producer()(this) : nullptr;
}

bool RunControl::createMainWorker()
{
    const auto canRun = std::bind(&RunWorkerFactory::canRun,
                                  std::placeholders::_1,
                                  d->runMode,
                                  DeviceTypeKitAspect::deviceTypeId(d->kit),
                                  d->runConfigId.toString());

    const QList<RunWorkerFactory *> candidates = Utils::filtered(g_runWorkerFactories, canRun);
    // There might be combinations that cannot run. But that should have been checked
    // with canRun below.
    QTC_ASSERT(!candidates.empty(), return false);

    // There should be at most one top-level producer feeling responsible per combination.
    // Breaking a tie should be done by tightening the restrictions on one of them.
    QTC_CHECK(candidates.size() == 1);
    return candidates.front()->producer()(this) != nullptr;
}

bool RunControl::canRun(Utils::Id runMode, Utils::Id deviceType, Utils::Id runConfigId)
{
    for (const RunWorkerFactory *factory : qAsConst(g_runWorkerFactories)) {
        if (factory->canRun(runMode, deviceType, runConfigId.toString()))
            return true;
    }
    return false;
}

void RunControlPrivate::initiateStart()
{
    checkState(RunControlState::Initialized);
    setState(RunControlState::Starting);
    debugMessage("Queue: Starting");

    continueStart();
}

void RunControlPrivate::initiateReStart()
{
    checkState(RunControlState::Stopped);

    // Re-set worked on re-runs.
    for (RunWorker *worker : qAsConst(m_workers)) {
        if (worker->d->state == RunWorkerState::Done)
            worker->d->state = RunWorkerState::Initialized;
    }

    setState(RunControlState::Starting);
    debugMessage("Queue: ReStarting");

    continueStart();
}

void RunControlPrivate::continueStart()
{
    checkState(RunControlState::Starting);
    bool allDone = true;
    debugMessage("Looking for next worker");
    for (RunWorker *worker : qAsConst(m_workers)) {
        if (worker) {
            const QString &workerId = worker->d->id;
            debugMessage("  Examining worker " + workerId);
            switch (worker->d->state) {
                case RunWorkerState::Initialized:
                    debugMessage("  " + workerId + " is not done yet.");
                    if (worker->d->canStart()) {
                        debugMessage("Starting " + workerId);
                        worker->d->state = RunWorkerState::Starting;
                        QTimer::singleShot(0, worker, &RunWorker::initiateStart);
                        return;
                    }
                    allDone = false;
                    debugMessage("  " + workerId + " cannot start.");
                    break;
                case RunWorkerState::Starting:
                    debugMessage("  " + workerId + " currently starting");
                    allDone = false;
                    break;
                case RunWorkerState::Running:
                    debugMessage("  " + workerId + " currently running");
                    break;
                case RunWorkerState::Stopping:
                    debugMessage("  " + workerId + " currently stopping");
                    continue;
                case RunWorkerState::Done:
                    debugMessage("  " + workerId + " was done before");
                    break;
            }
        } else {
            debugMessage("Found unknown deleted worker while starting");
        }
    }
    if (allDone)
        setState(RunControlState::Running);
}

void RunControlPrivate::initiateStop()
{
    if (state != RunControlState::Starting && state != RunControlState::Running)
        qDebug() << "Unexpected initiateStop() in state" << stateName(state);

    setState(RunControlState::Stopping);
    debugMessage("Queue: Stopping for all workers");

    continueStopOrFinish();
}

void RunControlPrivate::continueStopOrFinish()
{
    bool allDone = true;

    auto queueStop = [this](RunWorker *worker, const QString &message) {
        if (worker->d->canStop()) {
            debugMessage(message);
            worker->d->state = RunWorkerState::Stopping;
            QTimer::singleShot(0, worker, &RunWorker::initiateStop);
        } else {
            debugMessage(" " + worker->d->id + " is waiting for dependent workers to stop");
        }
    };

    for (RunWorker *worker : qAsConst(m_workers)) {
        if (worker) {
            const QString &workerId = worker->d->id;
            debugMessage("  Examining worker " + workerId);
            switch (worker->d->state) {
                case RunWorkerState::Initialized:
                    debugMessage("  " + workerId + " was Initialized, setting to Done");
                    worker->d->state = RunWorkerState::Done;
                    break;
                case RunWorkerState::Stopping:
                    debugMessage("  " + workerId + " was already Stopping. Keeping it that way");
                    allDone = false;
                    break;
                case RunWorkerState::Starting:
                    queueStop(worker, "  " + workerId + " was Starting, queuing stop");
                    allDone = false;
                    break;
                case RunWorkerState::Running:
                    queueStop(worker, "  " + workerId + " was Running, queuing stop");
                    allDone = false;
                    break;
                case RunWorkerState::Done:
                    debugMessage("  " + workerId + " was Done. Good.");
                    break;
            }
        } else {
            debugMessage("Found unknown deleted worker");
        }
    }

    RunControlState targetState;
    if (state == RunControlState::Finishing) {
        targetState = RunControlState::Finished;
    } else {
        checkState(RunControlState::Stopping);
        targetState = RunControlState::Stopped;
    }

    if (allDone) {
        debugMessage("All Stopped");
        setState(targetState);
    } else {
        debugMessage("Not all workers Stopped. Waiting...");
    }
}

void RunControlPrivate::forceStop()
{
    if (state == RunControlState::Finished) {
        debugMessage("Was finished, too late to force Stop");
        return;
    }
    for (RunWorker *worker : qAsConst(m_workers)) {
        if (worker) {
            const QString &workerId = worker->d->id;
            debugMessage("  Examining worker " + workerId);
            switch (worker->d->state) {
                case RunWorkerState::Initialized:
                    debugMessage("  " + workerId + " was Initialized, setting to Done");
                    break;
                case RunWorkerState::Stopping:
                    debugMessage("  " + workerId + " was already Stopping. Set it forcefully to Done.");
                    break;
                case RunWorkerState::Starting:
                    debugMessage("  " + workerId + " was Starting. Set it forcefully to Done.");
                    break;
                case RunWorkerState::Running:
                    debugMessage("  " + workerId + " was Running. Set it forcefully to Done.");
                    break;
                case RunWorkerState::Done:
                    debugMessage("  " + workerId + " was Done. Good.");
                    break;
            }
            worker->d->state = RunWorkerState::Done;
        } else {
            debugMessage("Found unknown deleted worker");
        }
    }

    setState(RunControlState::Stopped);
    debugMessage("All Stopped");
}

void RunControlPrivate::initiateFinish()
{
    setState(RunControlState::Finishing);
    debugMessage("Ramping down");

    continueStopOrFinish();
}

void RunControlPrivate::onWorkerStarted(RunWorker *worker)
{
    worker->d->state = RunWorkerState::Running;

    if (state == RunControlState::Starting) {
        debugMessage(worker->d->id + " start succeeded");
        continueStart();
        return;
    }
    showError(RunControl::tr("Unexpected run control state %1 when worker %2 started.")
                  .arg(stateName(state))
                  .arg(worker->d->id));
}

void RunControlPrivate::onWorkerFailed(RunWorker *worker, const QString &msg)
{
    worker->d->state = RunWorkerState::Done;

    showError(msg);
    switch (state) {
    case RunControlState::Initialized:
        // FIXME 1: We don't have an output pane yet, so use some other mechanism for now.
        // FIXME 2: Translation...
        QMessageBox::critical(Core::ICore::dialogParent(),
             QCoreApplication::translate("TaskHub", "Error"),
             QString("Failure during startup. Aborting.") + "<p>" + msg);
        continueStopOrFinish();
        break;
    case RunControlState::Starting:
    case RunControlState::Running:
        initiateStop();
        break;
    case RunControlState::Stopping:
    case RunControlState::Finishing:
        continueStopOrFinish();
        break;
    case RunControlState::Stopped:
    case RunControlState::Finished:
        QTC_CHECK(false); // Should not happen.
        continueStopOrFinish();
        break;
    }
}

void RunControlPrivate::onWorkerStopped(RunWorker *worker)
{
    const QString &workerId = worker->d->id;
    switch (worker->d->state) {
    case RunWorkerState::Running:
        // That was a spontaneous stop.
        worker->d->state = RunWorkerState::Done;
        debugMessage(workerId + " stopped spontaneously.");
        break;
    case RunWorkerState::Stopping:
        worker->d->state = RunWorkerState::Done;
        debugMessage(workerId + " stopped expectedly.");
        break;
    case RunWorkerState::Done:
        worker->d->state = RunWorkerState::Done;
        debugMessage(workerId + " stopped twice. Huh? But harmless.");
        return; // Sic!
    default:
        debugMessage(workerId + " stopped unexpectedly in state"
                     + stateName(worker->d->state));
        worker->d->state = RunWorkerState::Done;
        break;
    }

    if (state == RunControlState::Finishing || state == RunControlState::Stopping) {
        continueStopOrFinish();
        return;
    } else if (worker->isEssential()) {
        debugMessage(workerId + " is essential. Stopping all others.");
        initiateStop();
        return;
    }

    for (RunWorker *dependent : qAsConst(worker->d->stopDependencies)) {
        switch (dependent->d->state) {
        case RunWorkerState::Done:
            break;
        case RunWorkerState::Initialized:
            dependent->d->state = RunWorkerState::Done;
            break;
        default:
            debugMessage("Killing " + dependent->d->id + " as it depends on stopped " + workerId);
            dependent->d->state = RunWorkerState::Stopping;
            QTimer::singleShot(0, dependent, &RunWorker::initiateStop);
            break;
        }
    }

    debugMessage("Checking whether all stopped");
    bool allDone = true;
    for (RunWorker *worker : qAsConst(m_workers)) {
        if (worker) {
            const QString &workerId = worker->d->id;
            debugMessage("  Examining worker " + workerId);
            switch (worker->d->state) {
                case RunWorkerState::Initialized:
                    debugMessage("  " + workerId + " was Initialized.");
                    break;
                case RunWorkerState::Starting:
                    debugMessage("  " + workerId + " was Starting, waiting for its response");
                    allDone = false;
                    break;
                case RunWorkerState::Running:
                    debugMessage("  " + workerId + " was Running, waiting for its response");
                    allDone = false;
                    break;
                case RunWorkerState::Stopping:
                    debugMessage("  " + workerId + " was already Stopping. Keeping it that way");
                    allDone = false;
                    break;
                case RunWorkerState::Done:
                    debugMessage("  " + workerId + " was Done. Good.");
                    break;
            }
        } else {
            debugMessage("Found unknown deleted worker");
        }
    }

    if (allDone) {
        if (state == RunControlState::Stopped) {
            debugMessage("All workers stopped, but runControl was already stopped.");
        } else {
            debugMessage("All workers stopped. Set runControl to Stopped");
            setState(RunControlState::Stopped);
        }
    } else {
        debugMessage("Not all workers stopped. Waiting...");
    }
}

void RunControlPrivate::showError(const QString &msg)
{
    if (!msg.isEmpty())
        emit q->appendMessage(msg + '\n', ErrorMessageFormat);
}

void RunControl::setupFormatter(OutputFormatter *formatter) const
{
    QList<Utils::OutputLineParser *> parsers = OutputFormatterFactory::createFormatters(target());
    if (const auto customParsersAspect = aspect<CustomParsersAspect>()) {
        for (const Id id : qAsConst(customParsersAspect->parsers)) {
            if (CustomParser * const parser = CustomParser::createFromId(id))
                parsers << parser;
        }
    }
    formatter->setLineParsers(parsers);
    if (project()) {
        Utils::FileInProjectFinder fileFinder;
        fileFinder.setProjectDirectory(project()->projectDirectory());
        fileFinder.setProjectFiles(project()->files(Project::AllFiles));
        formatter->setFileFinder(fileFinder);
    }
}

Utils::Id RunControl::runMode() const
{
    return d->runMode;
}

const Runnable &RunControl::runnable() const
{
    return d->runnable;
}

const CommandLine &RunControl::commandLine() const
{
    return d->runnable.command;
}

void RunControl::setCommandLine(const CommandLine &command)
{
    d->runnable.command = command;
}

const FilePath &RunControl::workingDirectory() const
{
    return d->runnable.workingDirectory;
}

void RunControl::setWorkingDirectory(const FilePath &workingDirectory)
{
    d->runnable.workingDirectory = workingDirectory;
}

const Environment &RunControl::environment() const
{
    return d->runnable.environment;
}

void RunControl::setEnvironment(const Environment &environment)
{
    d->runnable.environment = environment;
}

const QVariantHash &RunControl::extraData() const
{
    return d->runnable.extraData;
}

void RunControl::setExtraData(const QVariantHash &extraData)
{
    d->runnable.extraData = extraData;
}

QString RunControl::displayName() const
{
    if (d->displayName.isEmpty())
        return d->runnable.command.executable().toUserOutput();
    return d->displayName;
}

void RunControl::setDisplayName(const QString &displayName)
{
    d->displayName = displayName;
}

void RunControl::setIcon(const Utils::Icon &icon)
{
    d->icon = icon;
}

Utils::Icon RunControl::icon() const
{
    return d->icon;
}

IDevice::ConstPtr RunControl::device() const
{
   return d->device;
}

Target *RunControl::target() const
{
    return d->target;
}

Project *RunControl::project() const
{
    return d->project;
}

Kit *RunControl::kit() const
{
    return d->kit;
}

const MacroExpander *RunControl::macroExpander() const
{
    return d->macroExpander;
}

const BaseAspect::Data *RunControl::aspect(Id instanceId) const
{
    return d->aspectData.aspect(instanceId);
}

const BaseAspect::Data *RunControl::aspect(BaseAspect::Data::ClassId classId) const
{
    return d->aspectData.aspect(classId);
}

QVariantMap RunControl::settingsData(Id id) const
{
    return d->settingsData.value(id);
}

QString RunControl::buildKey() const
{
    return d->buildKey;
}

FilePath RunControl::buildDirectory() const
{
    return d->buildDirectory;
}

Environment RunControl::buildEnvironment() const
{
    return d->buildEnvironment;
}

FilePath RunControl::targetFilePath() const
{
    return d->buildTargetInfo.targetFilePath;
}

FilePath RunControl::projectFilePath() const
{
    return d->buildTargetInfo.projectFilePath;
}

/*!
    A handle to the application process.

    This is typically a process id, but should be treated as
    opaque handle to the process controled by this \c RunControl.
*/

ProcessHandle RunControl::applicationProcessHandle() const
{
    return d->applicationProcessHandle;
}

void RunControl::setApplicationProcessHandle(const ProcessHandle &handle)
{
    if (d->applicationProcessHandle != handle) {
        d->applicationProcessHandle = handle;
        emit applicationProcessHandleChanged(QPrivateSignal());
    }
}

/*!
    Prompts to stop. If \a optionalPrompt is passed, a \gui {Do not ask again}
    checkbox is displayed and the result is returned in \a *optionalPrompt.
*/

bool RunControl::promptToStop(bool *optionalPrompt) const
{
    QTC_ASSERT(isRunning(), return true);
    if (optionalPrompt && !*optionalPrompt)
        return true;

    // Overridden.
    if (d->promptToStop)
        return d->promptToStop(optionalPrompt);

    const QString msg = tr("<html><head/><body><center><i>%1</i> is still running.<center/>"
                           "<center>Force it to quit?</center></body></html>").arg(displayName());
    return showPromptToStopDialog(tr("Application Still Running"), msg,
                                  tr("Force &Quit"), tr("&Keep Running"),
                                  optionalPrompt);
}

void RunControl::setPromptToStop(const std::function<bool (bool *)> &promptToStop)
{
    d->promptToStop = promptToStop;
}

bool RunControl::supportsReRunning() const
{
    return d->supportsReRunning();
}

bool RunControlPrivate::supportsReRunning() const
{
    for (RunWorker *worker : m_workers) {
        if (!worker->d->supportsReRunning)
            return false;
        if (worker->d->state != RunWorkerState::Done)
            return false;
    }
    return true;
}

bool RunControl::isRunning() const
{
    return d->state == RunControlState::Running;
}

bool RunControl::isStarting() const
{
    return d->state == RunControlState::Starting;
}

bool RunControl::isStopping() const
{
    return d->state == RunControlState::Stopping;
}

bool RunControl::isStopped() const
{
    return d->state == RunControlState::Stopped;
}

/*!
    Prompts to terminate the application with the \gui {Do not ask again}
    checkbox.
*/

bool RunControl::showPromptToStopDialog(const QString &title,
                                        const QString &text,
                                        const QString &stopButtonText,
                                        const QString &cancelButtonText,
                                        bool *prompt)
{
    // Show a question message box where user can uncheck this
    // question for this class.
    Utils::CheckableMessageBox messageBox(Core::ICore::dialogParent());
    messageBox.setWindowTitle(title);
    messageBox.setText(text);
    messageBox.setStandardButtons(QDialogButtonBox::Yes|QDialogButtonBox::Cancel);
    if (!stopButtonText.isEmpty())
        messageBox.button(QDialogButtonBox::Yes)->setText(stopButtonText);
    if (!cancelButtonText.isEmpty())
        messageBox.button(QDialogButtonBox::Cancel)->setText(cancelButtonText);
    messageBox.setDefaultButton(QDialogButtonBox::Yes);
    if (prompt) {
        messageBox.setCheckBoxText(Utils::CheckableMessageBox::msgDoNotAskAgain());
        messageBox.setChecked(false);
    } else {
        messageBox.setCheckBoxVisible(false);
    }
    messageBox.exec();
    const bool close = messageBox.clickedStandardButton() == QDialogButtonBox::Yes;
    if (close && prompt && messageBox.isChecked())
        *prompt = false;
    return close;
}

void RunControl::provideAskPassEntry(Environment &env)
{
    if (env.value("SUDO_ASKPASS").isEmpty()) {
        const FilePath askpass = SshSettings::askpassFilePath();
        if (askpass.exists())
            env.set("SUDO_ASKPASS", askpass.toUserOutput());
    }
}

bool RunControlPrivate::isAllowedTransition(RunControlState from, RunControlState to)
{
    switch (from) {
    case RunControlState::Initialized:
        return to == RunControlState::Starting
            || to == RunControlState::Finishing;
    case RunControlState::Starting:
        return to == RunControlState::Running
            || to == RunControlState::Stopping
            || to == RunControlState::Finishing;
    case RunControlState::Running:
        return to == RunControlState::Stopping
            || to == RunControlState::Stopped
            || to == RunControlState::Finishing;
    case RunControlState::Stopping:
        return to == RunControlState::Stopped
            || to == RunControlState::Finishing;
    case RunControlState::Stopped:
        return to == RunControlState::Starting
            || to == RunControlState::Finishing;
    case RunControlState::Finishing:
        return to == RunControlState::Finished;
    case RunControlState::Finished:
        return false;
    }
    return false;
}

void RunControlPrivate::checkState(RunControlState expectedState)
{
    if (state != expectedState)
        qDebug() << "Unexpected run control state " << stateName(expectedState)
                 << " have: " << stateName(state);
}

void RunControlPrivate::setState(RunControlState newState)
{
    if (!isAllowedTransition(state, newState))
        qDebug() << "Invalid run control state transition from" << stateName(state)
                 << "to" << stateName(newState);

    state = newState;

    debugMessage("Entering state " + stateName(newState));

    // Extra reporting.
    switch (state) {
    case RunControlState::Running:
        emit q->started();
        break;
    case RunControlState::Stopped:
        q->setApplicationProcessHandle(Utils::ProcessHandle());
        emit q->stopped();
        break;
    case RunControlState::Finished:
        emit q->finished();
        debugMessage("All finished. Deleting myself");
        q->deleteLater();
        break;
    default:
        break;
    }
}

void RunControlPrivate::debugMessage(const QString &msg)
{
    qCDebug(statesLog()) << msg;
}


// SimpleTargetRunnerPrivate

namespace Internal {

class SimpleTargetRunnerPrivate : public QObject
{
    Q_OBJECT

public:
    explicit SimpleTargetRunnerPrivate(SimpleTargetRunner *parent);
    ~SimpleTargetRunnerPrivate() override;

    void start();
    void stop();

    Utils::ProcessHandle applicationPID() const;

    enum State { Inactive, Run };

    void handleStandardOutput();
    void handleStandardError();
    void handleDone();

    // Local
    qint64 privateApplicationPID() const;
    bool isRunning() const;

    SimpleTargetRunner *q = nullptr;

    bool m_runAsRoot = false;

    QtcProcess m_process;

    QTextCodec *m_outputCodec = nullptr;
    QTextCodec::ConverterState m_outputCodecState;
    QTextCodec::ConverterState m_errorCodecState;

    State m_state = Inactive;
    bool m_stopRequested = false;

    Utils::CommandLine m_command;
    Utils::FilePath m_workingDirectory;
    Utils::Environment m_environment;
    QVariantHash m_extraData;

    ProcessResultData m_resultData;

    std::function<void()> m_startModifier;

    bool m_stopReported = false;
    bool m_stopForced = false;

    void forwardStarted();
    void forwardDone();
};

} // Internal

static QProcess::ProcessChannelMode defaultProcessChannelMode()
{
    return ProjectExplorerPlugin::appOutputSettings().mergeChannels
            ? QProcess::MergedChannels : QProcess::SeparateChannels;
}

SimpleTargetRunnerPrivate::SimpleTargetRunnerPrivate(SimpleTargetRunner *parent)
    : q(parent)
{
    m_process.setProcessChannelMode(defaultProcessChannelMode());
    connect(&m_process, &QtcProcess::started, this, &SimpleTargetRunnerPrivate::forwardStarted);
    connect(&m_process, &QtcProcess::done, this, &SimpleTargetRunnerPrivate::handleDone);
    connect(&m_process, &QtcProcess::readyReadStandardError,
                this, &SimpleTargetRunnerPrivate::handleStandardError);
    connect(&m_process, &QtcProcess::readyReadStandardOutput,
                this, &SimpleTargetRunnerPrivate::handleStandardOutput);

    if (WinDebugInterface::instance()) {
        connect(WinDebugInterface::instance(), &WinDebugInterface::cannotRetrieveDebugOutput,
            this, [this] {
                disconnect(WinDebugInterface::instance(), nullptr, this, nullptr);
                q->appendMessage(tr("Cannot retrieve debugging output.")
                          + QLatin1Char('\n'), ErrorMessageFormat);
        });

        connect(WinDebugInterface::instance(), &WinDebugInterface::debugOutput,
            this, [this](qint64 pid, const QString &message) {
            if (privateApplicationPID() == pid)
                q->appendMessage(message, DebugFormat);
        });
    }
}

SimpleTargetRunnerPrivate::~SimpleTargetRunnerPrivate()
{
    if (m_state == Run)
        forwardDone();
}

void SimpleTargetRunnerPrivate::stop()
{
    m_resultData.m_exitStatus = QProcess::CrashExit;

    const bool isLocal = !m_command.executable().needsDevice();
    if (isLocal) {
        if (!isRunning())
            return;
        m_process.stop();
        m_process.waitForFinished();
        QTimer::singleShot(100, this, [this] { forwardDone(); });
    } else {
        if (m_stopRequested)
            return;
        m_stopRequested = true;
        q->appendMessage(tr("User requested stop. Shutting down..."), NormalMessageFormat);
        switch (m_state) {
            case Run:
                m_process.stop();
                m_process.waitForFinished();
                break;
            case Inactive:
                break;
        }
    }
}

bool SimpleTargetRunnerPrivate::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 SimpleTargetRunnerPrivate::privateApplicationPID() const
{
    if (!isRunning())
        return 0;

    return m_process.processId();
}

void SimpleTargetRunnerPrivate::handleDone()
{
    m_resultData = m_process.resultData();
    QTC_ASSERT(m_state == Run, forwardDone(); return);

    m_state = Inactive;
    forwardDone();
}

void SimpleTargetRunnerPrivate::handleStandardOutput()
{
    const QByteArray data = m_process.readAllStandardOutput();
    const QString msg = m_outputCodec->toUnicode(
                data.constData(), data.length(), &m_outputCodecState);
    q->appendMessageChunk(msg, StdOutFormat);
}

void SimpleTargetRunnerPrivate::handleStandardError()
{
    const QByteArray data = m_process.readAllStandardError();
    const QString msg = m_outputCodec->toUnicode(
                data.constData(), data.length(), &m_errorCodecState);
    q->appendMessageChunk(msg, StdErrFormat);
}

void SimpleTargetRunnerPrivate::start()
{
    const bool isLocal = !m_command.executable().needsDevice();

    CommandLine cmdLine = m_command;
    Environment env = m_environment;

    m_resultData = {};
    QTC_ASSERT(m_state == Inactive, return);

    if (isLocal) {
        if (m_runAsRoot)
            RunControl::provideAskPassEntry(env);

        WinDebugInterface::startIfNeeded();

        if (HostOsInfo::isMacHost()) {
            CommandLine disclaim(Core::ICore::libexecPath("disclaim"));
            disclaim.addCommandLineAsArgs(cmdLine);
            cmdLine = disclaim;
        }

        m_process.setRunAsRoot(m_runAsRoot);
    }

    const IDevice::ConstPtr device = DeviceManager::deviceForPath(m_command.executable());
    if (device && !device->isEmptyCommandAllowed() && m_command.isEmpty()) {
        m_resultData.m_errorString = tr("Cannot run: No command given.");
        m_resultData.m_error = QProcess::FailedToStart;
        m_resultData.m_exitStatus = QProcess::CrashExit;
        forwardDone();
        return;
    }

    m_stopRequested = false;

    m_process.setCommand(cmdLine);
    m_process.setEnvironment(env);
    m_process.setExtraData(m_extraData);

    m_state = Run;
    m_process.setWorkingDirectory(m_workingDirectory);

    if (isLocal)
        m_outputCodec = QTextCodec::codecForLocale();
    else
        m_outputCodec = QTextCodec::codecForName("utf8");

    m_process.start();
}


/*!
    \class ProjectExplorer::SimpleTargetRunner

    \brief The SimpleTargetRunner class is the application launcher of the
    ProjectExplorer plugin.

    Encapsulates processes running in a console or as GUI processes,
    captures debug output of GUI processes on Windows (outputDebugString()).

    \sa Utils::QtcProcess
*/

SimpleTargetRunner::SimpleTargetRunner(RunControl *runControl)
    : RunWorker(runControl), d(new Internal::SimpleTargetRunnerPrivate(this))
{
    setId("SimpleTargetRunner");
}

SimpleTargetRunner::~SimpleTargetRunner() = default;

void SimpleTargetRunnerPrivate::forwardDone()
{
    if (m_stopReported)
        return;
    const QString executable = m_command.executable().displayName();
    QString msg = tr("%1 exited with code %2").arg(executable).arg(m_resultData.m_exitCode);
    if (m_resultData.m_exitStatus == QProcess::CrashExit)
        msg = tr("%1 crashed.").arg(executable);
    else if (m_stopForced)
        msg = tr("The process was ended forcefully.");
    else if (m_resultData.m_error != QProcess::UnknownError)
        msg = RunWorker::userMessageForProcessError(m_resultData.m_error, m_command.executable());
    q->appendMessage(msg, NormalMessageFormat);
    m_stopReported = true;
    q->reportStopped();
}

void SimpleTargetRunnerPrivate::forwardStarted()
{
    const bool isDesktop = !m_command.executable().needsDevice();
    if (isDesktop) {
        // Console processes only know their pid after being started
        ProcessHandle pid{privateApplicationPID()};
        q->runControl()->setApplicationProcessHandle(pid);
        pid.activate();
    }

    q->reportStarted();
}

void SimpleTargetRunner::start()
{
    d->m_command = runControl()->commandLine();
    d->m_workingDirectory = runControl()->workingDirectory();
    d->m_environment = runControl()->environment();
    d->m_extraData = runControl()->extraData();

    if (d->m_startModifier)
        d->m_startModifier();

    bool useTerminal = false;
    if (auto terminalAspect = runControl()->aspect<TerminalAspect>())
        useTerminal = terminalAspect->useTerminal;

    bool runAsRoot = false;
    if (auto runAsRootAspect = runControl()->aspect<RunAsRootAspect>())
        runAsRoot = runAsRootAspect->value;

    d->m_stopForced = false;
    d->m_stopReported = false;
    d->disconnect(this);
    d->m_process.setTerminalMode(useTerminal ? Utils::TerminalMode::On : Utils::TerminalMode::Off);
    d->m_runAsRoot = runAsRoot;

    const QString msg = RunControl::tr("Starting %1...").arg(d->m_command.displayName());
    appendMessage(msg, NormalMessageFormat);

    const bool isDesktop = !d->m_command.executable().needsDevice();
    if (isDesktop && d->m_command.isEmpty()) {
        reportFailure(RunControl::tr("No executable specified."));
        return;
    }
    d->start();
}

void SimpleTargetRunner::stop()
{
    d->m_stopForced = true;
    d->stop();
}

void SimpleTargetRunner::setStartModifier(const std::function<void ()> &startModifier)
{
    d->m_startModifier = startModifier;
}

CommandLine SimpleTargetRunner::commandLine() const
{
    return d->m_command;
}

void SimpleTargetRunner::setCommandLine(const Utils::CommandLine &commandLine)
{
    d->m_command = commandLine;
}

void SimpleTargetRunner::setEnvironment(const Environment &environment)
{
    d->m_environment = environment;
}

void SimpleTargetRunner::setWorkingDirectory(const FilePath &workingDirectory)
{
    d->m_workingDirectory = workingDirectory;
}

void SimpleTargetRunner::forceRunOnHost()
{
    const FilePath executable = d->m_command.executable();
    if (executable.needsDevice()) {
        QTC_CHECK(false);
        d->m_command.setExecutable(FilePath::fromString(executable.path()));
    }
}

// RunWorkerPrivate

RunWorkerPrivate::RunWorkerPrivate(RunWorker *runWorker, RunControl *runControl)
    : q(runWorker), runControl(runControl)
{
    runControl->d->m_workers.append(runWorker);
}

bool RunWorkerPrivate::canStart() const
{
    if (state != RunWorkerState::Initialized)
        return false;
    for (RunWorker *worker : startDependencies) {
        QTC_ASSERT(worker, continue);
        if (worker->d->state != RunWorkerState::Done
                && worker->d->state != RunWorkerState::Running)
            return false;
    }
    return true;
}

bool RunWorkerPrivate::canStop() const
{
    if (state != RunWorkerState::Starting && state != RunWorkerState::Running)
        return false;
    for (RunWorker *worker : stopDependencies) {
        QTC_ASSERT(worker, continue);
        if (worker->d->state != RunWorkerState::Done)
            return false;
    }
    return true;
}

void RunWorkerPrivate::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == startWatchdogTimerId) {
        if (startWatchdogCallback) {
            killStartWatchdog();
            startWatchdogCallback();
        } else {
            q->reportFailure(RunWorker::tr("Worker start timed out."));
        }
        return;
    }
    if (ev->timerId() == stopWatchdogTimerId) {
        if (stopWatchdogCallback) {
            killStopWatchdog();
            stopWatchdogCallback();
        } else {
            q->reportFailure(RunWorker::tr("Worker stop timed out."));
        }
        return;
    }
}

/*!
    \class ProjectExplorer::RunWorker

    \brief The RunWorker class encapsulates a task that forms part, or
    the whole of the operation of a tool for a certain \c RunConfiguration
    according to some \c RunMode.

    A typical example for a \c RunWorker is a process, either the
    application process itself, or a helper process, such as a watchdog
    or a log parser.

    A \c RunWorker has a simple state model covering the \c Initialized,
    \c Starting, \c Running, \c Stopping, and \c Done states.

    In the course of the operation of tools several \c RunWorkers
    may co-operate and form a combined state that is presented
    to the user as \c RunControl, with direct interaction made
    possible through the buttons in the \uicontrol{Application Output}
    pane.

    RunWorkers are typically created together with their RunControl.
    The startup order of RunWorkers under a RunControl can be
    specified by making a RunWorker dependent on others.

    When a RunControl starts, it calls \c initiateStart() on RunWorkers
    with fulfilled dependencies until all workers are \c Running, or in case
    of short-lived helper tasks, \c Done.

    A RunWorker can stop spontaneously, for example when the main application
    process ends. In this case, it typically calls \c initiateStop()
    on its RunControl, which in turn passes this to all sibling
    RunWorkers.

    Pressing the stop button in the \uicontrol{Application Output} pane
    also calls \c initiateStop on the RunControl.
*/

RunWorker::RunWorker(RunControl *runControl)
    : d(std::make_unique<RunWorkerPrivate>(this, runControl))
{ }

RunWorker::~RunWorker() = default;

/*!
 * This function is called by the RunControl once all dependencies
 * are fulfilled.
 */
void RunWorker::initiateStart()
{
    d->startStartWatchdog();
    d->runControl->d->debugMessage("Initiate start for " + d->id);
    start();
}

/*!
 * This function has to be called by a RunWorker implementation
 * to notify its RunControl about the successful start of this RunWorker.
 *
 * The RunControl may start other RunWorkers in response.
 */
void RunWorker::reportStarted()
{
    d->killStartWatchdog();
    d->runControl->d->onWorkerStarted(this);
    emit started();
}

/*!
 * This function is called by the RunControl in its own \c initiateStop
 * implementation, which is triggered in response to pressing the
 * stop button in the \uicontrol{Application Output} pane or on direct
 * request of one of the sibling RunWorkers.
 */
void RunWorker::initiateStop()
{
    d->startStopWatchdog();
    d->runControl->d->debugMessage("Initiate stop for " + d->id);
    stop();
}

/*!
 * This function has to be called by a RunWorker implementation
 * to notify its RunControl about this RunWorker having stopped.
 *
 * The stop can be spontaneous, or in response to an initiateStop()
 * or an initiateFinish() call.
 *
 * The RunControl will adjust its global state in response.
 */
void RunWorker::reportStopped()
{
    d->killStopWatchdog();
    d->runControl->d->onWorkerStopped(this);
    emit stopped();
}

/*!
 * This function can be called by a RunWorker implementation for short-lived
 * tasks to notify its RunControl about this task being successful finished.
 * Dependent startup tasks can proceed, in cases of spontaneous or scheduled
 * stops, the effect is the same as \c reportStopped().
 *
 */
void RunWorker::reportDone()
{
    d->killStartWatchdog();
    d->killStopWatchdog();
    switch (d->state) {
        case RunWorkerState::Initialized:
            QTC_CHECK(false);
            d->state = RunWorkerState::Done;
            break;
        case RunWorkerState::Starting:
            reportStarted();
            reportStopped();
            break;
        case RunWorkerState::Running:
        case RunWorkerState::Stopping:
            reportStopped();
            break;
        case RunWorkerState::Done:
            break;
    }
}

/*!
 * This function can be called by a RunWorker implementation to
 * signal a problem in the operation in this worker. The
 * RunControl will start to ramp down through initiateStop().
 */
void RunWorker::reportFailure(const QString &msg)
{
    d->killStartWatchdog();
    d->killStopWatchdog();
    d->runControl->d->onWorkerFailed(this, msg);
}

/*!
 * Appends a message in the specified \a format to
 * the owning RunControl's \uicontrol{Application Output} pane.
 */
void RunWorker::appendMessage(const QString &msg, OutputFormat format)
{
    if (msg.endsWith('\n'))
        emit d->runControl->appendMessage(msg, format);
    else
        emit d->runControl->appendMessage(msg + '\n', format);
}

void RunWorker::appendMessageChunk(const QString &msg, OutputFormat format)
{
    emit d->runControl->appendMessage(msg, format);
}

IDevice::ConstPtr RunWorker::device() const
{
    return d->runControl->device();
}

void RunWorker::addStartDependency(RunWorker *dependency)
{
    d->startDependencies.append(dependency);
}

void RunWorker::addStopDependency(RunWorker *dependency)
{
    d->stopDependencies.append(dependency);
}

RunControl *RunWorker::runControl() const
{
    return d->runControl;
}

void RunWorker::setId(const QString &id)
{
    d->id = id;
}

void RunWorker::setStartTimeout(int ms, const std::function<void()> &callback)
{
    d->startWatchdogInterval = ms;
    d->startWatchdogCallback = callback;
}

void RunWorker::setStopTimeout(int ms, const std::function<void()> &callback)
{
    d->stopWatchdogInterval = ms;
    d->stopWatchdogCallback = callback;
}

void RunWorker::recordData(const QString &channel, const QVariant &data)
{
    d->data[channel] = data;
}

QVariant RunWorker::recordedData(const QString &channel) const
{
    return d->data[channel];
}

void RunWorker::setSupportsReRunning(bool reRunningSupported)
{
    d->supportsReRunning = reRunningSupported;
}

bool RunWorker::supportsReRunning() const
{
    return d->supportsReRunning;
}

QString RunWorker::userMessageForProcessError(QProcess::ProcessError error, const FilePath &program)
{
    QString failedToStart = tr("The process failed to start.");
    QString msg = tr("An unknown error in the process occurred.");
    switch (error) {
        case QProcess::FailedToStart:
            msg = failedToStart + ' ' + tr("Either the "
                "invoked program \"%1\" is missing, or you may have insufficient "
                "permissions to invoke the program.").arg(program.toUserOutput());
            break;
        case QProcess::Crashed:
            msg = tr("The process crashed.");
            break;
        case QProcess::Timedout:
            // "The last waitFor...() function timed out. "
            //   "The state of QProcess is unchanged, and you can try calling "
            // "waitFor...() again."
            return QString(); // sic!
        case QProcess::WriteError:
            msg = tr("An error occurred when attempting to write "
                "to the process. For example, the process may not be running, "
                "or it may have closed its input channel.");
            break;
        case QProcess::ReadError:
            msg = tr("An error occurred when attempting to read from "
                "the process. For example, the process may not be running.");
            break;
        case QProcess::UnknownError:
            break;
    }
    return msg;
}

bool RunWorker::isEssential() const
{
    return d->essential;
}

void RunWorker::setEssential(bool essential)
{
    d->essential = essential;
}

void RunWorker::start()
{
    reportStarted();
}

void RunWorker::stop()
{
    reportStopped();
}

// OutputFormatterFactory

static QList<OutputFormatterFactory *> g_outputFormatterFactories;

OutputFormatterFactory::OutputFormatterFactory()
{
    g_outputFormatterFactories.append(this);
}

OutputFormatterFactory::~OutputFormatterFactory()
{
    g_outputFormatterFactories.removeOne(this);
}

QList<OutputLineParser *> OutputFormatterFactory::createFormatters(Target *target)
{
    QList<OutputLineParser *> formatters;
    for (auto factory : qAsConst(g_outputFormatterFactories))
        formatters << factory->m_creator(target);
    return formatters;
}

void OutputFormatterFactory::setFormatterCreator(const FormatterCreator &creator)
{
    m_creator = creator;
}

} // namespace ProjectExplorer

#include "runcontrol.moc"
