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

#include "linuxdevice.h"

#include "genericlinuxdeviceconfigurationwidget.h"
#include "genericlinuxdeviceconfigurationwizard.h"
#include "linuxdevicetester.h"
#include "linuxprocessinterface.h"
#include "publickeydeploymentdialog.h"
#include "remotelinux_constants.h"
#include "remotelinuxsignaloperation.h"
#include "remotelinuxenvironmentreader.h"
#include "sshprocessinterface.h"

#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <projectexplorer/devicesupport/filetransfer.h>
#include <projectexplorer/devicesupport/filetransferinterface.h>
#include <projectexplorer/devicesupport/sshdeviceprocesslist.h>
#include <projectexplorer/devicesupport/sshparameters.h>
#include <projectexplorer/devicesupport/sshsettings.h>
#include <projectexplorer/runcontrol.h>

#include <utils/algorithm.h>
#include <utils/deviceshell.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/port.h>
#include <utils/processinfo.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/stringutils.h>
#include <utils/temporaryfile.h>

#include <QDateTime>
#include <QLoggingCategory>
#include <QMutex>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

using namespace ProjectExplorer;
using namespace Utils;

namespace RemoteLinux {

const QByteArray s_pidMarker = "__qtc";

const char Delimiter0[] = "x--";
const char Delimiter1[] = "---";

static Q_LOGGING_CATEGORY(linuxDeviceLog, "qtc.remotelinux.device", QtWarningMsg);
#define LOG(x) qCDebug(linuxDeviceLog) << x << '\n'
//#define DEBUG(x) qDebug() << x;
//#define DEBUG(x) LOG(x)
#define DEBUG(x)

class SshSharedConnection : public QObject
{
    Q_OBJECT

public:
    explicit SshSharedConnection(const SshParameters &sshParameters, QObject *parent = nullptr);
    ~SshSharedConnection() override;

    SshParameters sshParameters() const { return m_sshParameters; }
    void ref();
    void deref();
    void makeStale();

    void connectToHost();
    void disconnectFromHost();

    QProcess::ProcessState state() const;
    QString socketFilePath() const
    {
        QTC_ASSERT(m_masterSocketDir, return QString());
        return m_masterSocketDir->path() + "/cs";
    }

signals:
    void connected(const QString &socketFilePath);
    void disconnected(const ProcessResultData &result);

    void autoDestructRequested();

private:
    void emitConnected();
    void emitError(QProcess::ProcessError processError, const QString &errorString);
    void emitDisconnected();
    QString fullProcessError() const;
    QStringList connectionArgs(const FilePath &binary) const;

    const SshParameters m_sshParameters;
    std::unique_ptr<QtcProcess> m_masterProcess;
    std::unique_ptr<QTemporaryDir> m_masterSocketDir;
    QTimer m_timer;
    int m_ref = 0;
    bool m_stale = false;
    QProcess::ProcessState m_state = QProcess::NotRunning;
};

SshSharedConnection::SshSharedConnection(const SshParameters &sshParameters, QObject *parent)
    : QObject(parent), m_sshParameters(sshParameters)
{
}

SshSharedConnection::~SshSharedConnection()
{
    QTC_CHECK(m_ref == 0);
    disconnect();
    disconnectFromHost();
}

void SshSharedConnection::ref()
{
    ++m_ref;
    m_timer.stop();
}

void SshSharedConnection::deref()
{
    QTC_ASSERT(m_ref, return);
    if (--m_ref)
        return;
    if (m_stale) // no one uses it
        deleteLater();
    // not stale, so someone may reuse it
    m_timer.start(SshSettings::connectionSharingTimeout() * 1000 * 60);
}

void SshSharedConnection::makeStale()
{
    m_stale = true;
    if (!m_ref) // no one uses it
        deleteLater();
}

void SshSharedConnection::connectToHost()
{
    if (state() != QProcess::NotRunning)
        return;

    const FilePath sshBinary = SshSettings::sshFilePath();
    if (!sshBinary.exists()) {
        emitError(QProcess::FailedToStart, tr("Cannot establish SSH connection: ssh binary "
                  "\"%1\" does not exist.").arg(sshBinary.toUserOutput()));
        return;
    }

    m_masterSocketDir.reset(new QTemporaryDir);
    if (!m_masterSocketDir->isValid()) {
        emitError(QProcess::FailedToStart, tr("Cannot establish SSH connection: Failed to create temporary "
                     "directory for control socket: %1")
                  .arg(m_masterSocketDir->errorString()));
        m_masterSocketDir.reset();
        return;
    }

    m_masterProcess.reset(new QtcProcess);
    SshParameters::setupSshEnvironment(m_masterProcess.get());
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &SshSharedConnection::autoDestructRequested);
    connect(m_masterProcess.get(), &QtcProcess::readyReadStandardOutput, [this] {
        const QByteArray reply = m_masterProcess->readAllStandardOutput();
        if (reply == "\n")
            emitConnected();
        // TODO: otherwise emitError and finish master process?
    });
    // TODO: in case of refused connection we are getting the following on stdErr:
    // ssh: connect to host 127.0.0.1 port 22: Connection refused\r\n
    connect(m_masterProcess.get(), &QtcProcess::done, [this] {
        const ProcessResult result = m_masterProcess->result();
        const ProcessResultData resultData = m_masterProcess->resultData();
        if (result == ProcessResult::StartFailed) {
            emitError(QProcess::FailedToStart, tr("Cannot establish SSH connection.\n"
                                                  "Control process failed to start."));
            return;
        } else if (result == ProcessResult::FinishedWithError) {
            emitError(resultData.m_error, fullProcessError());
            return;
        }
        emit disconnected(resultData);
    });

    QStringList args = QStringList{"-M", "-N", "-o", "ControlPersist=no",
            "-o", "PermitLocalCommand=yes", // Enable local command
            "-o", "LocalCommand=echo"}      // Local command is executed after successfully
                                            // connecting to the server. "echo" will print "\n"
                                            // on the process output if everything went fine.
            << connectionArgs(sshBinary);
    if (!m_sshParameters.x11DisplayName.isEmpty()) {
        args.prepend("-X");
        Environment env = m_masterProcess->environment();
        env.set("DISPLAY", m_sshParameters.x11DisplayName);
        m_masterProcess->setEnvironment(env);
    }
    m_masterProcess->setCommand(CommandLine(sshBinary, args));
    m_masterProcess->start();
}

void SshSharedConnection::disconnectFromHost()
{
    m_masterProcess.reset();
    m_masterSocketDir.reset();
}

QProcess::ProcessState SshSharedConnection::state() const
{
    return m_state;
}

void SshSharedConnection::emitConnected()
{
    m_state = QProcess::Running;
    emit connected(socketFilePath());
}

void SshSharedConnection::emitError(QProcess::ProcessError error, const QString &errorString)
{
    m_state = QProcess::NotRunning;
    ProcessResultData resultData = m_masterProcess->resultData();
    resultData.m_error = error;
    resultData.m_errorString = errorString;
    emit disconnected(resultData);
}

void SshSharedConnection::emitDisconnected()
{
    m_state = QProcess::NotRunning;
    emit disconnected(m_masterProcess->resultData());
}

QString SshSharedConnection::fullProcessError() const
{
    const QString errorString = m_masterProcess->exitStatus() == QProcess::CrashExit
            ? m_masterProcess->errorString() : QString();
    const QString standardError = m_masterProcess->cleanedStdErr();
    const QString errorPrefix = errorString.isEmpty() && standardError.isEmpty()
            ? tr("SSH connection failure.") : tr("SSH connection failure:");
    QStringList allErrors {errorPrefix, errorString, standardError};
    allErrors.removeAll({});
    return allErrors.join('\n');
}

QStringList SshSharedConnection::connectionArgs(const FilePath &binary) const
{
    return m_sshParameters.connectionOptions(binary) << "-o" << ("ControlPath=" + socketFilePath())
                                                     << m_sshParameters.host();
}

// SshConnectionHandle

class SshConnectionHandle : public QObject
{
    Q_OBJECT
public:
    SshConnectionHandle(const IDevice::ConstPtr &device) : m_device(device) {}
    ~SshConnectionHandle() override { emit detachFromSharedConnection(); }

signals:
    // direction: connection -> caller
    void connected(const QString &socketFilePath);
    void disconnected(const ProcessResultData &result);
    // direction: caller -> connection
    void detachFromSharedConnection();

private:
    // Store the IDevice::ConstPtr in order to extend the lifetime of device for as long
    // as this object is alive.
    IDevice::ConstPtr m_device;
};

static QString visualizeNull(QString s)
{
    return s.replace(QLatin1Char('\0'), QLatin1String("<null>"));
}

class LinuxDeviceProcessList : public SshDeviceProcessList
{
public:
    LinuxDeviceProcessList(const IDevice::ConstPtr &device, QObject *parent)
            : SshDeviceProcessList(device, parent)
    {
    }

private:
    QString listProcessesCommandLine() const override
    {
        return QString::fromLatin1(
            "for dir in `ls -d /proc/[0123456789]*`; do "
                "test -d $dir || continue;" // Decrease the likelihood of a race condition.
                "echo $dir;"
                "cat $dir/cmdline;echo;" // cmdline does not end in newline
                "cat $dir/stat;"
                "readlink $dir/exe;"
                "printf '%1''%2';"
            "done").arg(QLatin1String(Delimiter0)).arg(QLatin1String(Delimiter1));
    }

    QList<ProcessInfo> buildProcessList(const QString &listProcessesReply) const override
    {
        QList<ProcessInfo> processes;
        const QStringList lines = listProcessesReply.split(QString::fromLatin1(Delimiter0)
                + QString::fromLatin1(Delimiter1), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList elements = line.split(QLatin1Char('\n'));
            if (elements.count() < 4) {
                qDebug("%s: Expected four list elements, got %d. Line was '%s'.", Q_FUNC_INFO,
                       int(elements.count()), qPrintable(visualizeNull(line)));
                continue;
            }
            bool ok;
            const int pid = elements.first().mid(6).toInt(&ok);
            if (!ok) {
                qDebug("%s: Expected number in %s. Line was '%s'.", Q_FUNC_INFO,
                       qPrintable(elements.first()), qPrintable(visualizeNull(line)));
                continue;
            }
            QString command = elements.at(1);
            command.replace(QLatin1Char('\0'), QLatin1Char(' '));
            if (command.isEmpty()) {
                const QString &statString = elements.at(2);
                const int openParenPos = statString.indexOf(QLatin1Char('('));
                const int closedParenPos = statString.indexOf(QLatin1Char(')'), openParenPos);
                if (openParenPos == -1 || closedParenPos == -1)
                    continue;
                command = QLatin1Char('[')
                        + statString.mid(openParenPos + 1, closedParenPos - openParenPos - 1)
                        + QLatin1Char(']');
            }

            ProcessInfo process;
            process.processId = pid;
            process.commandLine = command;
            process.executable = elements.at(3);
            processes.append(process);
        }

        Utils::sort(processes);
        return processes;
    }
};


// LinuxDevicePrivate

class ShellThreadHandler;

class LinuxDevicePrivate
{
public:
    explicit LinuxDevicePrivate(LinuxDevice *parent);
    ~LinuxDevicePrivate();

    bool setupShell();
    bool runInShell(const CommandLine &cmd, const QByteArray &data = {});
    QByteArray outputForRunInShell(const CommandLine &cmd);
    void attachToSharedConnection(SshConnectionHandle *connectionHandle,
                                  const SshParameters &sshParameters);

    LinuxDevice *q = nullptr;
    QThread m_shellThread;
    ShellThreadHandler *m_handler = nullptr;
    mutable QMutex m_shellMutex;
    QList<QtcProcess *> m_terminals;
};

// SshProcessImpl

class SshProcessInterfacePrivate : public QObject
{
    Q_OBJECT

public:
    SshProcessInterfacePrivate(SshProcessInterface *sshInterface, LinuxDevicePrivate *devicePrivate);

    void start();

    void handleConnected(const QString &socketFilePath);
    void handleDisconnected(const ProcessResultData &result);

    void handleStarted();
    void handleDone();
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();

    void clearForStart();
    void doStart();
    CommandLine fullLocalCommandLine() const;

    SshProcessInterface *q = nullptr;

    qint64 m_processId = 0;
    // Store the IDevice::ConstPtr in order to extend the lifetime of device for as long
    // as this object is alive.
    IDevice::ConstPtr m_device;
    std::unique_ptr<SshConnectionHandle> m_connectionHandle;
    QtcProcess m_process;
    LinuxDevicePrivate *m_devicePrivate = nullptr;

    QString m_socketFilePath;
    SshParameters m_sshParameters;
    bool m_connecting = false;
    bool m_killed = false;

    ProcessResultData m_result;
};

SshProcessInterface::SshProcessInterface(const LinuxDevice *linuxDevice)
    : d(new SshProcessInterfacePrivate(this, linuxDevice->d))
{
}

SshProcessInterface::~SshProcessInterface()
{
    delete d;
}

void SshProcessInterface::handleStarted(qint64 processId)
{
    emitStarted(processId);
}

void SshProcessInterface::handleDone(const ProcessResultData &resultData)
{
    emit done(resultData);
}

void SshProcessInterface::handleReadyReadStandardOutput(const QByteArray &outputData)
{
    emit readyRead(outputData, {});
}

void SshProcessInterface::handleReadyReadStandardError(const QByteArray &errorData)
{
    emit readyRead({}, errorData);
}

void SshProcessInterface::emitStarted(qint64 processId)
{
    d->m_processId = processId;
    emit started(processId);
}

void SshProcessInterface::killIfRunning()
{
    if (d->m_killed || d->m_process.state() != QProcess::Running)
        return;
    sendControlSignal(ControlSignal::Kill);
    d->m_killed = true;
}

qint64 SshProcessInterface::processId() const
{
    return d->m_processId;
}

bool SshProcessInterface::runInShell(const CommandLine &command, const QByteArray &data)
{
    return d->m_devicePrivate->runInShell(command, data);
}

void SshProcessInterface::start()
{
    d->start();
}

qint64 SshProcessInterface::write(const QByteArray &data)
{
    return d->m_process.writeRaw(data);
}

LinuxProcessInterface::LinuxProcessInterface(const LinuxDevice *linuxDevice)
    : SshProcessInterface(linuxDevice)
{
}

LinuxProcessInterface::~LinuxProcessInterface()
{
    killIfRunning();
}

void LinuxProcessInterface::sendControlSignal(ControlSignal controlSignal)
{
    QTC_ASSERT(controlSignal != ControlSignal::KickOff, return);
    const qint64 pid = processId();
    QTC_ASSERT(pid, return); // TODO: try sending a signal based on process name
    const QString args = QString::fromLatin1("-%1 %2")
            .arg(controlSignalToInt(controlSignal)).arg(pid);
    CommandLine command = { "kill", args, CommandLine::Raw };
    // Note: This blocking call takes up to 2 ms for local remote.
    runInShell(command);
}

QString LinuxProcessInterface::fullCommandLine(const CommandLine &commandLine) const
{
    CommandLine cmd;

    if (!commandLine.isEmpty()) {
        const QStringList rcFilesToSource = {"/etc/profile", "$HOME/.profile"};
        for (const QString &filePath : rcFilesToSource) {
            cmd.addArgs({"test", "-f", filePath});
            cmd.addArgs("&&", CommandLine::Raw);
            cmd.addArgs({".", filePath});
            cmd.addArgs(";", CommandLine::Raw);
        }
    }

    if (!m_setup.m_workingDirectory.isEmpty()) {
        cmd.addArgs({"cd", m_setup.m_workingDirectory.path()});
        cmd.addArgs("&&", CommandLine::Raw);
    }

    if (m_setup.m_terminalMode == TerminalMode::Off)
        cmd.addArgs(QString("echo ") + s_pidMarker + "$$" + s_pidMarker + " && ", CommandLine::Raw);

    const Environment &env = m_setup.m_environment;
    for (auto it = env.constBegin(); it != env.constEnd(); ++it)
        cmd.addArgs(env.key(it) + "='" + env.expandedValueForKey(env.key(it)) + '\'', CommandLine::Raw);

    if (m_setup.m_terminalMode == TerminalMode::Off)
        cmd.addArg("exec");

    if (!commandLine.isEmpty())
        cmd.addCommandLineAsArgs(commandLine, CommandLine::Raw);
    return cmd.arguments();
}

void LinuxProcessInterface::handleStarted(qint64 processId)
{
    // Don't emit started() when terminal is off,
    // it's being done later inside handleReadyReadStandardOutput().
    if (m_setup.m_terminalMode == TerminalMode::Off)
        return;

    m_pidParsed = true;
    emitStarted(processId);
}

void LinuxProcessInterface::handleDone(const ProcessResultData &resultData)
{
    ProcessResultData finalData = resultData;
    if (!m_pidParsed) {
        finalData.m_error = QProcess::FailedToStart;
        if (!m_error.isEmpty()) {
            if (!finalData.m_errorString.isEmpty())
                finalData.m_errorString += "\n";
            finalData.m_errorString += QString::fromLocal8Bit(m_error);
        }
    }
    emit done(finalData);
}

void LinuxProcessInterface::handleReadyReadStandardOutput(const QByteArray &outputData)
{
    if (m_pidParsed) {
        emit readyRead(outputData, {});
        return;
    }

    m_output.append(outputData);

    static const QByteArray endMarker = s_pidMarker + '\n';
    const int endMarkerOffset = m_output.indexOf(endMarker);
    if (endMarkerOffset == -1)
        return;
    const int startMarkerOffset = m_output.indexOf(s_pidMarker);
    if (startMarkerOffset == endMarkerOffset) // Only theoretically possible.
        return;
    const int pidStart = startMarkerOffset + s_pidMarker.length();
    const QByteArray pidString = m_output.mid(pidStart, endMarkerOffset - pidStart);
    m_pidParsed = true;
    const qint64 processId = pidString.toLongLong();

    // We don't want to show output from e.g. /etc/profile.
    m_output = m_output.mid(endMarkerOffset + endMarker.length());

    emitStarted(processId);

    if (!m_output.isEmpty() || !m_error.isEmpty())
        emit readyRead(m_output, m_error);

    m_output.clear();
    m_error.clear();
}

void LinuxProcessInterface::handleReadyReadStandardError(const QByteArray &errorData)
{
    if (m_pidParsed) {
        emit readyRead({}, errorData);
        return;
    }
    m_error.append(errorData);
}

SshProcessInterfacePrivate::SshProcessInterfacePrivate(SshProcessInterface *sshInterface,
                                                       LinuxDevicePrivate *devicePrivate)
    : QObject(sshInterface)
    , q(sshInterface)
    , m_device(devicePrivate->q->sharedFromThis())
    , m_process(this)
    , m_devicePrivate(devicePrivate)
{
    connect(&m_process, &QtcProcess::started, this, &SshProcessInterfacePrivate::handleStarted);
    connect(&m_process, &QtcProcess::done, this, &SshProcessInterfacePrivate::handleDone);
    connect(&m_process, &QtcProcess::readyReadStandardOutput,
            this, &SshProcessInterfacePrivate::handleReadyReadStandardOutput);
    connect(&m_process, &QtcProcess::readyReadStandardError,
            this, &SshProcessInterfacePrivate::handleReadyReadStandardError);
}

void SshProcessInterfacePrivate::start()
{
    clearForStart();

    m_sshParameters = m_devicePrivate->q->sshParameters();
    // TODO: Do we really need it for master process?
    m_sshParameters.x11DisplayName
            = q->m_setup.m_extraData.value("Ssh.X11ForwardToDisplay").toString();
    if (SshSettings::connectionSharingEnabled()) {
        m_connecting = true;
        m_connectionHandle.reset(new SshConnectionHandle(m_devicePrivate->q->sharedFromThis()));
        m_connectionHandle->setParent(this);
        connect(m_connectionHandle.get(), &SshConnectionHandle::connected,
                this, &SshProcessInterfacePrivate::handleConnected);
        connect(m_connectionHandle.get(), &SshConnectionHandle::disconnected,
                this, &SshProcessInterfacePrivate::handleDisconnected);
        m_devicePrivate->attachToSharedConnection(m_connectionHandle.get(), m_sshParameters);
    } else {
        doStart();
    }
}

void SshProcessInterfacePrivate::handleConnected(const QString &socketFilePath)
{
    m_connecting = false;
    m_socketFilePath = socketFilePath;
    doStart();
}

void SshProcessInterfacePrivate::handleDisconnected(const ProcessResultData &result)
{
    ProcessResultData resultData = result;
    if (m_connecting)
        resultData.m_error = QProcess::FailedToStart;

    m_connecting = false;
    if (m_connectionHandle) // TODO: should it disconnect from signals first?
        m_connectionHandle.release()->deleteLater();

    if (resultData.m_error != QProcess::UnknownError || m_process.state() != QProcess::NotRunning)
        emit q->done(resultData); // TODO: don't emit done() on process finished afterwards
}

void SshProcessInterfacePrivate::handleStarted()
{
    const qint64 processId = m_process.usesTerminal() ? m_process.processId() : 0;
    // By default emits started signal, Linux impl doesn't emit it when terminal is off.
    q->handleStarted(processId);
}

void SshProcessInterfacePrivate::handleDone()
{
    if (m_connectionHandle) // TODO: should it disconnect from signals first?
        m_connectionHandle.release()->deleteLater();
    q->handleDone(m_process.resultData());
}

void SshProcessInterfacePrivate::handleReadyReadStandardOutput()
{
    // By default emits signal. LinuxProcessImpl does custom parsing for processId
    // and emits delayed start() - only when terminal is off.
    q->handleReadyReadStandardOutput(m_process.readAllStandardOutput());
}

void SshProcessInterfacePrivate::handleReadyReadStandardError()
{
    // By default emits signal. LinuxProcessImpl buffers the error channel until
    // it emits delayed start() - only when terminal is off.
    q->handleReadyReadStandardError(m_process.readAllStandardError());
}

void SshProcessInterfacePrivate::clearForStart()
{
    m_result = {};
}

void SshProcessInterfacePrivate::doStart()
{
    m_process.setProcessImpl(q->m_setup.m_processImpl);
    m_process.setProcessMode(q->m_setup.m_processMode);
    m_process.setTerminalMode(q->m_setup.m_terminalMode);
    m_process.setReaperTimeout(q->m_setup.m_reaperTimeout);
    m_process.setWriteData(q->m_setup.m_writeData);
    // TODO: what about other fields from m_setup?
    SshParameters::setupSshEnvironment(&m_process);
    if (!m_sshParameters.x11DisplayName.isEmpty()) {
        Environment env = m_process.controlEnvironment();
        // Note: it seems this is no-op when shared connection is used.
        // In this case the display is taken from master process.
        env.set("DISPLAY", m_sshParameters.x11DisplayName);
        m_process.setControlEnvironment(env);
    }
    m_process.setCommand(fullLocalCommandLine());
    m_process.start();
}

CommandLine SshProcessInterfacePrivate::fullLocalCommandLine() const
{
    CommandLine cmd{SshSettings::sshFilePath()};

    if (!m_sshParameters.x11DisplayName.isEmpty())
        cmd.addArg("-X");
    if (q->m_setup.m_terminalMode != TerminalMode::Off)
        cmd.addArg("-tt");

    cmd.addArg("-q");

    QStringList options = m_sshParameters.connectionOptions(SshSettings::sshFilePath());
    if (!m_socketFilePath.isEmpty())
        options << "-o" << ("ControlPath=" + m_socketFilePath);
    options << m_sshParameters.host();
    cmd.addArgs(options);

    CommandLine remoteWithLocalPath = q->m_setup.m_commandLine;
    FilePath executable;
    executable.setPath(remoteWithLocalPath.executable().path());
    remoteWithLocalPath.setExecutable(executable);

    cmd.addArg(q->fullCommandLine(remoteWithLocalPath));
    return cmd;
}

// ShellThreadHandler

static SshParameters displayless(const SshParameters &sshParameters)
{
    SshParameters parameters = sshParameters;
    parameters.x11DisplayName.clear();
    return parameters;
}

class ShellThreadHandler : public QObject
{
    class LinuxDeviceShell : public DeviceShell
    {
    public:
        LinuxDeviceShell(const CommandLine &cmdLine)
            : m_cmdLine(cmdLine)
        {
        }

    private:
        void setupShellProcess(QtcProcess *shellProcess) override
        {
            SshParameters::setupSshEnvironment(shellProcess);
            shellProcess->setCommand(m_cmdLine);
        }

    private:
        const CommandLine m_cmdLine;
    };

public:
    ~ShellThreadHandler()
    {
        closeShell();
        qDeleteAll(m_connections);
    }

    void closeShell()
    {
        m_shell.reset();
    }

    // Call me with shell mutex locked
    bool start(const SshParameters &parameters)
    {
        closeShell();
        setSshParameters(parameters);

        const FilePath sshPath = SshSettings::sshFilePath();
        CommandLine cmd { sshPath };
        cmd.addArg("-q");
        cmd.addArgs(m_displaylessSshParameters.connectionOptions(sshPath)
                    << m_displaylessSshParameters.host());
        cmd.addArg("/bin/sh");

        m_shell.reset(new LinuxDeviceShell(cmd));
        connect(m_shell.get(), &DeviceShell::done, this, [this] { m_shell.reset(); });
        return m_shell->start();
    }

    // Call me with shell mutex locked
    bool runInShell(const CommandLine &cmd, const QByteArray &data = {})
    {
        QTC_ASSERT(m_shell, return false);
        return m_shell->runInShell(cmd, data);
    }

    // Call me with shell mutex locked
    QByteArray outputForRunInShell(const CommandLine &cmd)
    {
        QTC_ASSERT(m_shell, return {});
        return m_shell->outputForRunInShell(cmd).stdOut;
    }

    void setSshParameters(const SshParameters &sshParameters)
    {
        QMutexLocker locker(&m_mutex);
        const SshParameters displaylessSshParameters = displayless(sshParameters);

        if (m_displaylessSshParameters == displaylessSshParameters)
            return;

        // If displayless sshParameters don't match the old connections' sshParameters, then stale
        // old connections (don't delete, as the last deref() to each one will delete them).
        for (SshSharedConnection *connection : qAsConst(m_connections))
            connection->makeStale();
        m_connections.clear();
        m_displaylessSshParameters = displaylessSshParameters;
    }

    QString attachToSharedConnection(SshConnectionHandle *connectionHandle,
                                     const SshParameters &sshParameters)
    {
        setSshParameters(sshParameters);

        SshSharedConnection *matchingConnection = nullptr;

        // Find the matching connection
        for (SshSharedConnection *connection : qAsConst(m_connections)) {
            if (connection->sshParameters() == sshParameters) {
                matchingConnection = connection;
                break;
            }
        }

        // If no matching connection has been found, create a new one
        if (!matchingConnection) {
            matchingConnection = new SshSharedConnection(sshParameters);
            connect(matchingConnection, &SshSharedConnection::autoDestructRequested,
                    this, [this, matchingConnection] {
                // This slot is just for removing the matchingConnection from the connection list.
                // The SshSharedConnection could have deleted itself otherwise.
                m_connections.removeOne(matchingConnection);
                matchingConnection->deleteLater();
            });
            m_connections.append(matchingConnection);
        }

        matchingConnection->ref();

        connect(matchingConnection, &SshSharedConnection::connected,
                connectionHandle, &SshConnectionHandle::connected);
        connect(matchingConnection, &SshSharedConnection::disconnected,
                connectionHandle, &SshConnectionHandle::disconnected);

        connect(connectionHandle, &SshConnectionHandle::detachFromSharedConnection,
                matchingConnection, &SshSharedConnection::deref,
                Qt::BlockingQueuedConnection); // Ensure the signal is delivered before sender's
                                               // destruction, otherwise we may get out of sync
                                               // with ref count.

        if (matchingConnection->state() == QProcess::Running)
            return matchingConnection->socketFilePath();

        if (matchingConnection->state() == QProcess::NotRunning)
            matchingConnection->connectToHost();

        return {};
    }

    // Call me with shell mutex locked, called from other thread
    bool isRunning(const SshParameters &sshParameters) const
    {
        if (!m_shell)
           return false;
        QMutexLocker locker(&m_mutex);
        if (m_displaylessSshParameters != displayless(sshParameters))
           return false;
        return true;
    }
private:
    mutable QMutex m_mutex;
    SshParameters m_displaylessSshParameters;
    QList<SshSharedConnection *> m_connections;
    std::unique_ptr<LinuxDeviceShell> m_shell;
};

// LinuxDevice

LinuxDevice::LinuxDevice()
    : d(new LinuxDevicePrivate(this))
{
    setDisplayType(tr("Remote Linux"));
    setDefaultDisplayName(tr("Remote Linux Device"));
    setOsType(OsTypeLinux);

    addDeviceAction({tr("Deploy Public Key..."), [](const IDevice::Ptr &device, QWidget *parent) {
        if (auto d = PublicKeyDeploymentDialog::createDialog(device, parent)) {
            d->exec();
            delete d;
        }
    }});

    setOpenTerminal([this](const Environment &env, const FilePath &workingDir) {
        QtcProcess * const proc = new QtcProcess;
        d->m_terminals.append(proc);
        QObject::connect(proc, &QtcProcess::done, [this, proc] {
            if (proc->error() != QProcess::UnknownError) {
                const QString errorString = proc->errorString();
                QString message;
                if (proc->error() == QProcess::FailedToStart)
                    message = tr("Error starting remote shell.");
                else if (errorString.isEmpty())
                    message = tr("Error running remote shell.");
                else
                    message = tr("Error running remote shell: %1").arg(errorString);
                Core::MessageManager::writeDisrupting(message);
            }
            proc->deleteLater();
            d->m_terminals.removeOne(proc);
        });

        proc->setCommand({filePath({}), {}});
        proc->setTerminalMode(TerminalMode::On);
        proc->setEnvironment(env);
        proc->setWorkingDirectory(workingDir);
        proc->start();
    });

    addDeviceAction({tr("Open Remote Shell"), [](const IDevice::Ptr &device, QWidget *) {
                         device->openTerminal(Environment(), FilePath());
                     }});
}

LinuxDevice::~LinuxDevice()
{
    delete d;
}

IDeviceWidget *LinuxDevice::createWidget()
{
    return new GenericLinuxDeviceConfigurationWidget(sharedFromThis());
}

bool LinuxDevice::canAutoDetectPorts() const
{
    return true;
}

PortsGatheringMethod LinuxDevice::portsGatheringMethod() const
{
    return {
        [this](QAbstractSocket::NetworkLayerProtocol protocol) -> CommandLine {
            // We might encounter the situation that protocol is given IPv6
            // but the consumer of the free port information decides to open
            // an IPv4(only) port. As a result the next IPv6 scan will
            // report the port again as open (in IPv6 namespace), while the
            // same port in IPv4 namespace might still be blocked, and
            // re-use of this port fails.
            // GDBserver behaves exactly like this.

            Q_UNUSED(protocol)

            // /proc/net/tcp* covers /proc/net/tcp and /proc/net/tcp6
            return {filePath("sed"),
                    "-e 's/.*: [[:xdigit:]]*:\\([[:xdigit:]]\\{4\\}\\).*/\\1/g' /proc/net/tcp*",
                    CommandLine::Raw};
        },

        &Port::parseFromSedOutput
    };
}

DeviceProcessList *LinuxDevice::createProcessListModel(QObject *parent) const
{
    return new LinuxDeviceProcessList(sharedFromThis(), parent);
}

DeviceTester *LinuxDevice::createDeviceTester() const
{
    return new GenericLinuxDeviceTester;
}

DeviceProcessSignalOperation::Ptr LinuxDevice::signalOperation() const
{
    return DeviceProcessSignalOperation::Ptr(new RemoteLinuxSignalOperation(sharedFromThis()));
}

class LinuxDeviceEnvironmentFetcher : public DeviceEnvironmentFetcher
{
public:
    LinuxDeviceEnvironmentFetcher(const IDevice::ConstPtr &device)
        : m_reader(device)
    {
        connect(&m_reader, &Internal::RemoteLinuxEnvironmentReader::finished,
                this, &LinuxDeviceEnvironmentFetcher::readerFinished);
        connect(&m_reader, &Internal::RemoteLinuxEnvironmentReader::error,
                this, &LinuxDeviceEnvironmentFetcher::readerError);
    }

private:
    void start() override { m_reader.start(); }
    void readerFinished() { emit finished(m_reader.remoteEnvironment(), true); }
    void readerError() { emit finished(Environment(), false); }

    Internal::RemoteLinuxEnvironmentReader m_reader;
};

DeviceEnvironmentFetcher::Ptr LinuxDevice::environmentFetcher() const
{
    return DeviceEnvironmentFetcher::Ptr(new LinuxDeviceEnvironmentFetcher(sharedFromThis()));
}

QString LinuxDevice::userAtHost() const
{
    return sshParameters().userAtHost();
}

bool LinuxDevice::handlesFile(const FilePath &filePath) const
{
    if (filePath.scheme() == "device" && filePath.host() == id().toString())
        return true;
    if (filePath.scheme() == "ssh" && filePath.host() == userAtHost())
        return true;
    return false;
}

ProcessInterface *LinuxDevice::createProcessInterface() const
{
    return new LinuxProcessInterface(this);
}

Environment LinuxDevice::systemEnvironment() const
{
    return {}; // FIXME. See e.g. Docker implementation.
}

LinuxDevicePrivate::LinuxDevicePrivate(LinuxDevice *parent)
    : q(parent)
{
    m_handler = new ShellThreadHandler();
    m_handler->moveToThread(&m_shellThread);
    QObject::connect(&m_shellThread, &QThread::finished, m_handler, &QObject::deleteLater);
    m_shellThread.start();
}

LinuxDevicePrivate::~LinuxDevicePrivate()
{
    qDeleteAll(m_terminals);
    auto closeShell = [this] {
        m_shellThread.quit();
        m_shellThread.wait();
    };
    if (QThread::currentThread() == m_shellThread.thread())
        closeShell();
    else // We might be in a non-main thread now due to extended lifetime of IDevice::Ptr
        QMetaObject::invokeMethod(&m_shellThread, closeShell, Qt::BlockingQueuedConnection);
}

// Call me with shell mutex locked
bool LinuxDevicePrivate::setupShell()
{
    const SshParameters sshParameters = q->sshParameters();
    if (m_handler->isRunning(sshParameters))
        return true;

    bool ok = false;
    QMetaObject::invokeMethod(m_handler, [this, sshParameters] {
        return m_handler->start(sshParameters);
    }, Qt::BlockingQueuedConnection, &ok);
    return ok;
}

bool LinuxDevicePrivate::runInShell(const CommandLine &cmd, const QByteArray &data)
{
    QMutexLocker locker(&m_shellMutex);
    DEBUG(cmd.toUserOutput());
    QTC_ASSERT(setupShell(), return false);

    bool ret = false;
    QMetaObject::invokeMethod(m_handler, [this, &cmd, &data] {
        return m_handler->runInShell(cmd, data);
    }, Qt::BlockingQueuedConnection, &ret);
    return ret;
}

QByteArray LinuxDevicePrivate::outputForRunInShell(const CommandLine &cmd)
{
    QMutexLocker locker(&m_shellMutex);
    DEBUG(cmd);
    QTC_ASSERT(setupShell(), return {});

    QByteArray ret;
    QMetaObject::invokeMethod(m_handler, [this, &cmd] {
        return m_handler->outputForRunInShell(cmd);
    }, Qt::BlockingQueuedConnection, &ret);
    return ret;
}

void LinuxDevicePrivate::attachToSharedConnection(SshConnectionHandle *connectionHandle,
                                                  const SshParameters &sshParameters)
{
    QString socketFilePath;
    QMetaObject::invokeMethod(m_handler, [this, connectionHandle, sshParameters] {
        return m_handler->attachToSharedConnection(connectionHandle, sshParameters);
    }, Qt::BlockingQueuedConnection, &socketFilePath);
    if (!socketFilePath.isEmpty())
        emit connectionHandle->connected(socketFilePath);
}

bool LinuxDevice::isExecutableFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-x", path}});
}

bool LinuxDevice::isReadableFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-r", path, "-a", "-f", path}});
}

bool LinuxDevice::isWritableFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-w", path, "-a", "-f", path}});
}

bool LinuxDevice::isReadableDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-r", path, "-a", "-d", path}});
}

bool LinuxDevice::isWritableDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-w", path, "-a", "-d", path}});
}

bool LinuxDevice::isFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-f", path}});
}

bool LinuxDevice::isDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-d", path}});
}

bool LinuxDevice::createDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"mkdir", {"-p", path}});
}

bool LinuxDevice::exists(const FilePath &filePath) const
{
    DEBUG("filepath " << filePath.path());
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"test", {"-e", path}});
}

bool LinuxDevice::ensureExistingFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const QString path = filePath.path();
    return d->runInShell({"touch", {path}});
}

bool LinuxDevice::removeFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return d->runInShell({"rm", {filePath.path()}});
}

bool LinuxDevice::removeRecursively(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    QTC_ASSERT(filePath.path().startsWith('/'), return false);

    const QString path = filePath.cleanPath().path();
    // We are expecting this only to be called in a context of build directories or similar.
    // Chicken out in some cases that _might_ be user code errors.
    QTC_ASSERT(path.startsWith('/'), return false);
    const int levelsNeeded = path.startsWith("/home/") ? 3 : 2;
    QTC_ASSERT(path.count('/') >= levelsNeeded, return false);

    return d->runInShell({"rm", {"-rf", "--", path}});
}

bool LinuxDevice::copyFile(const FilePath &filePath, const FilePath &target) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    QTC_ASSERT(handlesFile(target), return false);
    return d->runInShell({"cp", {filePath.path(), target.path()}});
}

bool LinuxDevice::renameFile(const FilePath &filePath, const FilePath &target) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    QTC_ASSERT(handlesFile(target), return false);
    return d->runInShell({"mv", {filePath.path(), target.path()}});
}

QDateTime LinuxDevice::lastModified(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    const QByteArray output = d->outputForRunInShell({"stat", {"-L", "-c", "%Y", filePath.path()}});
    const qint64 secs = output.toLongLong();
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(secs, Qt::UTC);
    return dt;
}

FilePath LinuxDevice::symLinkTarget(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    const QByteArray output = d->outputForRunInShell({"readlink", {"-n", "-e", filePath.path()}});
    const QString out = QString::fromUtf8(output.data(), output.size());
    return output.isEmpty() ? FilePath() : filePath.withNewPath(out);
}

qint64 LinuxDevice::fileSize(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return -1);
    const QByteArray output = d->outputForRunInShell({"stat", {"-L", "-c", "%s", filePath.path()}});
    return output.toLongLong();
}

qint64 LinuxDevice::bytesAvailable(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return -1);
    CommandLine cmd("df", {"-k"});
    cmd.addArg(filePath.path());
    cmd.addArgs("|tail -n 1 |sed 's/  */ /g'|cut -d ' ' -f 4", CommandLine::Raw);
    const QByteArray output = d->outputForRunInShell(cmd);
    bool ok = false;
    const qint64 size = output.toLongLong(&ok);
    if (ok)
        return size * 1024;
    return -1;
}

QFileDevice::Permissions LinuxDevice::permissions(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    const QByteArray output = d->outputForRunInShell({"stat", {"-L", "-c", "%a", filePath.path()}});
    const uint bits = output.toUInt(nullptr, 8);
    QFileDevice::Permissions perm = {};
#define BIT(n, p) if (bits & (1<<n)) perm |= QFileDevice::p
    BIT(0, ExeOther);
    BIT(1, WriteOther);
    BIT(2, ReadOther);
    BIT(3, ExeGroup);
    BIT(4, WriteGroup);
    BIT(5, ReadGroup);
    BIT(6, ExeUser);
    BIT(7, WriteUser);
    BIT(8, ReadUser);
#undef BIT
    return perm;
}

bool LinuxDevice::setPermissions(const FilePath &filePath, QFileDevice::Permissions permissions) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    const int flags = int(permissions);
    return d->runInShell({"chmod", {QString::number(flags, 16), filePath.path()}});
}

void LinuxDevice::iterateDirectory(const FilePath &filePath,
                                   const std::function<bool(const FilePath &)> &callBack,
                                   const FileFilter &filter) const
{
    QTC_ASSERT(handlesFile(filePath), return);
    // if we do not have find - use ls as fallback
    const QByteArray output = d->outputForRunInShell({"ls", {"-1", "-b", "--", filePath.path()}});
    const QStringList entries = QString::fromUtf8(output).split('\n', Qt::SkipEmptyParts);
    FileUtils::iterateLsOutput(filePath, entries, filter, callBack);
}

QByteArray LinuxDevice::fileContents(const FilePath &filePath, qint64 limit, qint64 offset) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    QString args = "if=" + filePath.path() + " status=none";
    if (limit > 0 || offset > 0) {
        const qint64 gcd = std::gcd(limit, offset);
        args += QString(" bs=%1 count=%2 seek=%3").arg(gcd).arg(limit / gcd).arg(offset / gcd);
    }
    CommandLine cmd(FilePath::fromString("dd"), args, CommandLine::Raw);

    const QByteArray output = d->outputForRunInShell(cmd);
    DEBUG(output << QByteArray::fromHex(output));
    return output;
}

bool LinuxDevice::writeFileContents(const FilePath &filePath, const QByteArray &data) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return d->runInShell({"dd", {"of=" + filePath.path()}}, data);
}

static FilePaths dirsToCreate(const FilesToTransfer &files)
{
    FilePaths dirs;
    for (const FileToTransfer &file : files) {
        FilePath parentDir = file.m_target.parentDir();
        while (true) {
            if (dirs.contains(parentDir) || QDir(parentDir.path()).isRoot())
                break;
            dirs << parentDir;
            parentDir = parentDir.parentDir();
        }
    }
    sort(dirs);
    return dirs;
}

static QByteArray transferCommand(const FileTransferDirection direction, bool link)
{
    if (direction == FileTransferDirection::Upload)
        return link ? "ln -s" : "put";
    if (direction == FileTransferDirection::Download)
        return "get";
    return {};
}

class SshTransferInterface : public FileTransferInterface
{
    Q_OBJECT

protected:
    SshTransferInterface(const FileTransferSetupData &setup, LinuxDevicePrivate *devicePrivate)
        : FileTransferInterface(setup)
        , m_device(devicePrivate->q->sharedFromThis())
        , m_devicePrivate(devicePrivate)
        , m_process(this)
    {
        m_direction = m_setup.m_files.isEmpty() ? FileTransferDirection::Invalid
                                                : m_setup.m_files.first().direction();
        SshParameters::setupSshEnvironment(&m_process);
        connect(&m_process, &QtcProcess::readyReadStandardOutput, this, [this] {
            emit progress(QString::fromLocal8Bit(m_process.readAllStandardOutput()));
        });
        connect(&m_process, &QtcProcess::done, this, &SshTransferInterface::doneImpl);
    }

    bool handleError()
    {
        ProcessResultData resultData = m_process.resultData();
        if (resultData.m_error == QProcess::FailedToStart) {
            resultData.m_errorString = tr("\"%1\" failed to start: %2")
                    .arg(FileTransfer::transferMethodName(m_setup.m_method), resultData.m_errorString);
        } else if (resultData.m_exitStatus != QProcess::NormalExit) {
            resultData.m_errorString = tr("\"%1\" crashed.")
                    .arg(FileTransfer::transferMethodName(m_setup.m_method));
        } else if (resultData.m_exitCode != 0) {
            resultData.m_errorString = QString::fromLocal8Bit(m_process.readAllStandardError());
        } else {
            return false;
        }
        emit done(resultData);
        return true;
    }

    void handleDone()
    {
        if (!handleError())
            emit done(m_process.resultData());
    }

    QStringList fullConnectionOptions() const
    {
        QStringList options = m_sshParameters.connectionOptions(SshSettings::sshFilePath());
        if (!m_socketFilePath.isEmpty())
            options << "-o" << ("ControlPath=" + m_socketFilePath);
        return options;
    }

    QString host() const { return m_sshParameters.host(); }
    QString userAtHost() const { return m_sshParameters.userName() + '@' + m_sshParameters.host(); }

    QtcProcess &process() { return m_process; }
    FileTransferDirection direction() const { return m_direction; }

private:
    virtual void startImpl() = 0;
    virtual void doneImpl() = 0;

    void start() final
    {
        m_sshParameters = displayless(m_device->sshParameters());
        if (SshSettings::connectionSharingEnabled()) {
            m_connecting = true;
            m_connectionHandle.reset(new SshConnectionHandle(m_device));
            m_connectionHandle->setParent(this);
            connect(m_connectionHandle.get(), &SshConnectionHandle::connected,
                    this, &SshTransferInterface::handleConnected);
            connect(m_connectionHandle.get(), &SshConnectionHandle::disconnected,
                    this, &SshTransferInterface::handleDisconnected);
            m_devicePrivate->attachToSharedConnection(m_connectionHandle.get(), m_sshParameters);
        } else {
            startImpl();
        }
    }

    void handleConnected(const QString &socketFilePath)
    {
        m_connecting = false;
        m_socketFilePath = socketFilePath;
        startImpl();
    }

    void handleDisconnected(const ProcessResultData &result)
    {
        ProcessResultData resultData = result;
        if (m_connecting)
            resultData.m_error = QProcess::FailedToStart;

        m_connecting = false;
        if (m_connectionHandle) // TODO: should it disconnect from signals first?
            m_connectionHandle.release()->deleteLater();

        if (resultData.m_error != QProcess::UnknownError || m_process.state() != QProcess::NotRunning)
            emit done(resultData); // TODO: don't emit done() on process finished afterwards
    }

    IDevice::ConstPtr m_device;
    LinuxDevicePrivate *m_devicePrivate = nullptr;
    SshParameters m_sshParameters;
    FileTransferDirection m_direction = FileTransferDirection::Invalid; // helper

    // ssh shared connection related
    std::unique_ptr<SshConnectionHandle> m_connectionHandle;
    QString m_socketFilePath;
    bool m_connecting = false;

    QtcProcess m_process;
};

class SftpTransferImpl : public SshTransferInterface
{
public:
    SftpTransferImpl(const FileTransferSetupData &setup, LinuxDevicePrivate *devicePrivate)
        : SshTransferInterface(setup, devicePrivate) { }

private:
    void startImpl() final
    {
        const FilePath sftpBinary = SshSettings::sftpFilePath();
        if (!sftpBinary.exists()) {
            startFailed(SshTransferInterface::tr("\"sftp\" binary \"%1\" does not exist.")
                            .arg(sftpBinary.toUserOutput()));
            return;
        }

        m_batchFile.reset(new QTemporaryFile(this));
        if (!m_batchFile->isOpen() && !m_batchFile->open()) {
            startFailed(SshTransferInterface::tr("Could not create temporary file: %1")
                            .arg(m_batchFile->errorString()));
            return;
        }

        const FilePaths dirs = dirsToCreate(m_setup.m_files);
        for (const FilePath &dir : dirs) {
            if (direction() == FileTransferDirection::Upload) {
                m_batchFile->write("-mkdir " + ProcessArgs::quoteArgUnix(dir.path()).toLocal8Bit()
                                + '\n');
            } else if (direction() == FileTransferDirection::Download) {
                if (!QDir::root().mkpath(dir.path())) {
                    startFailed(SshTransferInterface::tr("Failed to create local directory \"%1\".")
                                    .arg(QDir::toNativeSeparators(dir.path())));
                    return;
                }
            }
        }

        for (const FileToTransfer &file : m_setup.m_files) {
            FilePath sourceFileOrLinkTarget = file.m_source;
            bool link = false;
            if (direction() == FileTransferDirection::Upload) {
                const QFileInfo fi(file.m_source.toFileInfo());
                if (fi.isSymLink()) {
                    link = true;
                    m_batchFile->write("-rm " + ProcessArgs::quoteArgUnix(
                                        file.m_target.path()).toLocal8Bit() + '\n');
                     // see QTBUG-5817.
                    sourceFileOrLinkTarget.setPath(fi.dir().relativeFilePath(fi.symLinkTarget()));
                }
             }
             m_batchFile->write(transferCommand(direction(), link) + ' '
                 + ProcessArgs::quoteArgUnix(sourceFileOrLinkTarget.path()).toLocal8Bit() + ' '
                 + ProcessArgs::quoteArgUnix(file.m_target.path()).toLocal8Bit() + '\n');
        }
        m_batchFile->close();
        process().setCommand(CommandLine(sftpBinary, fullConnectionOptions()
                                         << "-b" << m_batchFile->fileName() << host()));
        process().start();
    }

    void doneImpl() final { handleDone(); }

    std::unique_ptr<QTemporaryFile> m_batchFile;
};

class RsyncTransferImpl : public SshTransferInterface
{
public:
    RsyncTransferImpl(const FileTransferSetupData &setup, LinuxDevicePrivate *devicePrivate)
        : SshTransferInterface(setup, devicePrivate)
    { }

private:
    void startImpl() final
    {
        m_currentIndex = 0;
        startNextFile();
    }

    void doneImpl() final
    {
        if (m_setup.m_files.size() == 0 || m_currentIndex == m_setup.m_files.size() - 1)
            return handleDone();

        if (handleError())
            return;

        ++m_currentIndex;
        startNextFile();
    }

    void startNextFile()
    {
        process().close();

        const QString sshCmdLine = ProcessArgs::joinArgs(
                    QStringList{SshSettings::sshFilePath().toUserOutput()}
                    << fullConnectionOptions(), OsTypeLinux);
        QStringList options{"-e", sshCmdLine, m_setup.m_rsyncFlags};

        if (!m_setup.m_files.isEmpty()) { // NormalRun
            const FileToTransfer file = m_setup.m_files.at(m_currentIndex);
            const FileToTransfer fixedFile = fixLocalFileOnWindows(file, options);
            const auto fixedPaths = fixPaths(fixedFile, userAtHost());

            options << fixedPaths.first << fixedPaths.second;
        } else { // TestRun
            options << "-n" << "--exclude=*" << (userAtHost() + ":/tmp");
        }
        // TODO: Get rsync location from settings?
        process().setCommand(CommandLine("rsync", options));
        process().start();
    }

    // On Windows, rsync is either from msys or cygwin. Neither work with the other's ssh.exe.
    FileToTransfer fixLocalFileOnWindows(const FileToTransfer &file, const QStringList &options) const
    {
        if (!HostOsInfo::isWindowsHost())
            return file;

        QString localFilePath = direction() == FileTransferDirection::Upload
                ? file.m_source.path() : file.m_target.path();
        localFilePath = '/' + localFilePath.at(0) + localFilePath.mid(2);
        if (anyOf(options, [](const QString &opt) {
                return opt.contains("cygwin", Qt::CaseInsensitive); })) {
            localFilePath.prepend("/cygdrive");
        }

        FileToTransfer fixedFile = file;
        (direction() == FileTransferDirection::Upload) ? fixedFile.m_source.setPath(localFilePath)
                                                       : fixedFile.m_target.setPath(localFilePath);
        return fixedFile;
    }

    QPair<QString, QString> fixPaths(const FileToTransfer &file, const QString &remoteHost) const
    {
        FilePath localPath;
        FilePath remotePath;
        if (direction() == FileTransferDirection::Upload) {
            localPath = file.m_source;
            remotePath = file.m_target;
        } else {
            remotePath = file.m_source;
            localPath = file.m_target;
        }
        const QString local = (localPath.isDir() && localPath.path().back() != '/')
                ? localPath.path() + '/' : localPath.path();
        const QString remote = remoteHost + ':' + remotePath.path();

        return direction() == FileTransferDirection::Upload ? qMakePair(local, remote)
                                                            : qMakePair(remote, local);
    }

    int m_currentIndex = 0;
};

FileTransferInterface *LinuxDevice::createFileTransferInterface(
        const FileTransferSetupData &setup) const
{
    switch (setup.m_method) {
    case FileTransferMethod::Sftp:  return new SftpTransferImpl(setup, d);
    case FileTransferMethod::Rsync: return new RsyncTransferImpl(setup, d);
    }
    QTC_CHECK(false);
    return {};
}

namespace Internal {

// Factory

LinuxDeviceFactory::LinuxDeviceFactory()
    : IDeviceFactory(Constants::GenericLinuxOsType)
{
    setDisplayName(LinuxDevice::tr("Remote Linux Device"));
    setIcon(QIcon());
    setConstructionFunction(&LinuxDevice::create);
    setCreator([] {
        GenericLinuxDeviceConfigurationWizard wizard(Core::ICore::dialogParent());
        if (wizard.exec() != QDialog::Accepted)
            return IDevice::Ptr();
        return wizard.device();
    });
}

} // namespace Internal
} // namespace RemoteLinux

#include "linuxdevice.moc"
