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

#pragma once

#include "devicesupport/idevicefwd.h"
#include "runconfiguration.h"

#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/outputformatter.h>
#include <utils/processhandle.h>
#include <utils/qtcassert.h>

#include <QHash>
#include <QProcess> // FIXME: Remove
#include <QVariant>

#include <functional>
#include <memory>

namespace Utils {
class Icon;
class MacroExpander;
class OutputLineParser;
} // Utils

namespace ProjectExplorer {
class RunConfiguration;
class RunControl;
class Target;

namespace Internal {
class RunControlPrivate;
class RunWorkerPrivate;
class SimpleTargetRunnerPrivate;
} // Internal


class PROJECTEXPLORER_EXPORT Runnable
{
public:
    Runnable() = default;

    Utils::CommandLine command;
    Utils::FilePath workingDirectory;
    Utils::Environment environment;
    QVariantHash extraData;
};

class PROJECTEXPLORER_EXPORT RunWorker : public QObject
{
    Q_OBJECT

public:
    explicit RunWorker(RunControl *runControl);
    ~RunWorker() override;

    RunControl *runControl() const;

    void addStartDependency(RunWorker *dependency);
    void addStopDependency(RunWorker *dependency);

    void setId(const QString &id);

    void setStartTimeout(int ms, const std::function<void()> &callback = {});
    void setStopTimeout(int ms, const std::function<void()> &callback = {});

    void recordData(const QString &channel, const QVariant &data);
    QVariant recordedData(const QString &channel) const;

    // Part of read-only interface of RunControl for convenience.
    void appendMessage(const QString &msg, Utils::OutputFormat format);
    void appendMessageChunk(const QString &msg, Utils::OutputFormat format);
    IDeviceConstPtr device() const;

    // States
    void initiateStart();
    void reportStarted();

    void initiateStop();
    void reportStopped();

    void reportDone();

    void reportFailure(const QString &msg = QString());
    void setSupportsReRunning(bool reRunningSupported);
    bool supportsReRunning() const;

    static QString userMessageForProcessError(QProcess::ProcessError,
                                              const Utils::FilePath &programName);

    bool isEssential() const;
    void setEssential(bool essential);

signals:
    void started();
    void stopped();

protected:
    void virtual start();
    void virtual stop();
    void virtual onFinished() {}

private:
    friend class Internal::RunControlPrivate;
    friend class Internal::RunWorkerPrivate;
    const std::unique_ptr<Internal::RunWorkerPrivate> d;
};

class PROJECTEXPLORER_EXPORT RunWorkerFactory
{
public:
    using WorkerCreator = std::function<RunWorker *(RunControl *)>;

    RunWorkerFactory();
    RunWorkerFactory(const WorkerCreator &producer,
                     const QList<Utils::Id> &runModes,
                     const QList<Utils::Id> &runConfigs = {},
                     const QList<Utils::Id> &deviceTypes = {});

    ~RunWorkerFactory();

    bool canRun(Utils::Id runMode, Utils::Id deviceType, const QString &runConfigId) const;
    WorkerCreator producer() const { return m_producer; }

    template <typename Worker>
    static WorkerCreator make()
    {
        return [](RunControl *runControl) { return new Worker(runControl); };
    }

    // For debugging only.
    static void dumpAll();

protected:
    template <typename Worker>
    void setProduct() { setProducer([](RunControl *rc) { return new Worker(rc); }); }
    void setProducer(const WorkerCreator &producer);
    void addSupportedRunMode(Utils::Id runMode);
    void addSupportedRunConfig(Utils::Id runConfig);
    void addSupportedDeviceType(Utils::Id deviceType);

private:
    WorkerCreator m_producer;
    QList<Utils::Id> m_supportedRunModes;
    QList<Utils::Id> m_supportedRunConfigurations;
    QList<Utils::Id> m_supportedDeviceTypes;
};

/**
 * A RunControl controls the running of an application or tool
 * on a target device. It controls start and stop, and handles
 * application output.
 *
 * RunControls are created by RunControlFactories.
 */

class PROJECTEXPLORER_EXPORT RunControl : public QObject
{
    Q_OBJECT

public:
    explicit RunControl(Utils::Id mode);
    ~RunControl() override;

    void setTarget(Target *target);
    void setKit(Kit *kit);

    void copyDataFromRunConfiguration(RunConfiguration *runConfig);
    void copyDataFromRunControl(RunControl *runControl);

    void initiateStart();
    void initiateReStart();
    void initiateStop();
    void forceStop();
    void initiateFinish();

    bool promptToStop(bool *optionalPrompt = nullptr) const;
    void setPromptToStop(const std::function<bool(bool *)> &promptToStop);

    bool supportsReRunning() const;

    QString displayName() const;
    void setDisplayName(const QString &displayName);

    bool isRunning() const;
    bool isStarting() const;
    bool isStopping() const;
    bool isStopped() const;

    void setIcon(const Utils::Icon &icon);
    Utils::Icon icon() const;

    Utils::ProcessHandle applicationProcessHandle() const;
    void setApplicationProcessHandle(const Utils::ProcessHandle &handle);
    IDeviceConstPtr device() const;

    // FIXME: Try to cut down to amount of functions.
    Target *target() const;
    Project *project() const;
    Kit *kit() const;
    const Utils::MacroExpander *macroExpander() const;

    const Utils::BaseAspect::Data *aspect(Utils::Id instanceId) const;
    const Utils::BaseAspect::Data *aspect(Utils::BaseAspect::Data::ClassId classId) const;
    template <typename T> const typename T::Data *aspect() const {
        return dynamic_cast<const typename T::Data *>(aspect(&T::staticMetaObject));
    }

    QString buildKey() const;
    Utils::FilePath buildDirectory() const;
    Utils::Environment buildEnvironment() const;

    QVariantMap settingsData(Utils::Id id) const;

    Utils::FilePath targetFilePath() const;
    Utils::FilePath projectFilePath() const;

    void setupFormatter(Utils::OutputFormatter *formatter) const;
    Utils::Id runMode() const;

    const Runnable &runnable() const;

    const Utils::CommandLine &commandLine() const;
    void setCommandLine(const Utils::CommandLine &command);

    const Utils::FilePath &workingDirectory() const;
    void setWorkingDirectory(const Utils::FilePath &workingDirectory);

    const Utils::Environment &environment() const;
    void setEnvironment(const Utils::Environment &environment);

    const QVariantHash &extraData() const;
    void setExtraData(const QVariantHash &extraData);

    static bool showPromptToStopDialog(const QString &title, const QString &text,
                                       const QString &stopButtonText = QString(),
                                       const QString &cancelButtonText = QString(),
                                       bool *prompt = nullptr);

    static void provideAskPassEntry(Utils::Environment &env);

    RunWorker *createWorker(Utils::Id workerId);

    bool createMainWorker();
    static bool canRun(Utils::Id runMode, Utils::Id deviceType, Utils::Id runConfigId);

signals:
    void appendMessage(const QString &msg, Utils::OutputFormat format);
    void aboutToStart();
    void started();
    void stopped();
    void finished();
    void applicationProcessHandleChanged(QPrivateSignal); // Use setApplicationProcessHandle

private:
    void setDevice(const IDeviceConstPtr &device);

    friend class RunWorker;
    friend class Internal::RunWorkerPrivate;

    const std::unique_ptr<Internal::RunControlPrivate> d;
};


/**
 * A simple TargetRunner for cases where a plain ApplicationLauncher is
 * sufficient for running purposes.
 */

class PROJECTEXPLORER_EXPORT SimpleTargetRunner : public RunWorker
{
    Q_OBJECT

public:
    explicit SimpleTargetRunner(RunControl *runControl);
    ~SimpleTargetRunner() override;

protected:
    void setStartModifier(const std::function<void()> &startModifier);

    Utils::CommandLine commandLine() const;
    void setCommandLine(const Utils::CommandLine &commandLine);

    void setEnvironment(const Utils::Environment &environment);
    void setWorkingDirectory(const Utils::FilePath &workingDirectory);

    void forceRunOnHost();

private:
    void start() final;
    void stop() final;

    const Runnable &runnable() const = delete;
    void setRunnable(const Runnable &) = delete;

    const std::unique_ptr<Internal::SimpleTargetRunnerPrivate> d;
};

class PROJECTEXPLORER_EXPORT OutputFormatterFactory
{
protected:
    OutputFormatterFactory();

public:
    virtual ~OutputFormatterFactory();

    static QList<Utils::OutputLineParser *> createFormatters(Target *target);

protected:
    using FormatterCreator = std::function<QList<Utils::OutputLineParser *>(Target *)>;
    void setFormatterCreator(const FormatterCreator &creator);

private:
    FormatterCreator m_creator;
};

} // namespace ProjectExplorer
