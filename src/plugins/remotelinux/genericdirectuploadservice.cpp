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

#include "genericdirectuploadservice.h"

#include <projectexplorer/deployablefile.h>
#include <projectexplorer/devicesupport/filetransfer.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <utils/hostosinfo.h>
#include <utils/processinterface.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QQueue>
#include <QString>

using namespace ProjectExplorer;
using namespace Utils;

namespace RemoteLinux {
namespace Internal {

enum State { Inactive, PreChecking, Uploading, PostProcessing };

const int MaxConcurrentStatCalls = 10;

class GenericDirectUploadServicePrivate
{
public:
    DeployableFile getFileForProcess(QtcProcess *proc)
    {
        const auto it = remoteProcs.find(proc);
        QTC_ASSERT(it != remoteProcs.end(), return DeployableFile());
        const DeployableFile file = *it;
        remoteProcs.erase(it);
        return file;
    }

    IncrementalDeployment incremental = IncrementalDeployment::NotSupported;
    bool ignoreMissingFiles = false;
    QHash<QtcProcess *, DeployableFile> remoteProcs;
    QQueue<DeployableFile> filesToStat;
    State state = Inactive;
    QList<DeployableFile> filesToUpload;
    FileTransfer uploader;
    QList<DeployableFile> deployableFiles;
};

} // namespace Internal

using namespace Internal;

GenericDirectUploadService::GenericDirectUploadService(QObject *parent)
    : AbstractRemoteLinuxDeployService(parent), d(new GenericDirectUploadServicePrivate)
{
    connect(&d->uploader, &FileTransfer::done, this, [this](const ProcessResultData &result) {
        QTC_ASSERT(d->state == Uploading, return);
        if (result.m_error != QProcess::UnknownError) {
            emit errorMessage(result.m_errorString);
            setFinished();
            handleDeploymentDone();
            return;
        }
        d->state = PostProcessing;
        chmod();
        queryFiles();
    });
    connect(&d->uploader, &FileTransfer::progress,
            this, &GenericDirectUploadService::progressMessage);
}

GenericDirectUploadService::~GenericDirectUploadService()
{
    delete d;
}

void GenericDirectUploadService::setDeployableFiles(const QList<DeployableFile> &deployableFiles)
{
    d->deployableFiles = deployableFiles;
}

void GenericDirectUploadService::setIncrementalDeployment(IncrementalDeployment incremental)
{
    d->incremental = incremental;
}

void GenericDirectUploadService::setIgnoreMissingFiles(bool ignoreMissingFiles)
{
    d->ignoreMissingFiles = ignoreMissingFiles;
}

bool GenericDirectUploadService::isDeploymentNecessary() const
{
    QTC_ASSERT(d->filesToUpload.isEmpty(), d->filesToUpload.clear());
    QList<DeployableFile> collected;
    for (int i = 0; i < d->deployableFiles.count(); ++i)
        collected.append(collectFilesToUpload(d->deployableFiles.at(i)));

    QTC_CHECK(collected.size() >= d->deployableFiles.size());
    d->deployableFiles = collected;
    return !d->deployableFiles.isEmpty();
}

void GenericDirectUploadService::doDeploy()
{
    QTC_ASSERT(d->state == Inactive, setFinished(); return);
    d->state = PreChecking;
    queryFiles();
}

QDateTime GenericDirectUploadService::timestampFromStat(const DeployableFile &file,
                                                        QtcProcess *statProc)
{
    bool succeeded = false;
    QString error;
    if (statProc->error() == QProcess::FailedToStart) {
        error = tr("Failed to start \"stat\": %1").arg(statProc->errorString());
    } else if (statProc->exitStatus() == QProcess::CrashExit) {
        error = tr("\"stat\" crashed.");
    } else if (statProc->exitCode() != 0) {
        error = tr("\"stat\" failed with exit code %1: %2")
                .arg(statProc->exitCode()).arg(statProc->cleanedStdErr());
    } else {
        succeeded = true;
    }
    if (!succeeded) {
        emit warningMessage(tr("Failed to retrieve remote timestamp for file \"%1\". "
                               "Incremental deployment will not work. Error message was: %2")
                            .arg(file.remoteFilePath(), error));
        return QDateTime();
    }
    const QByteArray output = statProc->readAllStandardOutput().trimmed();
    const QString warningString(tr("Unexpected stat output for remote file \"%1\": %2")
                                .arg(file.remoteFilePath()).arg(QString::fromUtf8(output)));
    if (!output.startsWith(file.remoteFilePath().toUtf8())) {
        emit warningMessage(warningString);
        return QDateTime();
    }
    const QByteArrayList columns = output.mid(file.remoteFilePath().toUtf8().size() + 1).split(' ');
    if (columns.size() < 14) { // Normal Linux stat: 16 columns in total, busybox stat: 15 columns
        emit warningMessage(warningString);
        return QDateTime();
    }
    bool isNumber;
    const qint64 secsSinceEpoch = columns.at(11).toLongLong(&isNumber);
    if (!isNumber) {
        emit warningMessage(warningString);
        return QDateTime();
    }
    return QDateTime::fromSecsSinceEpoch(secsSinceEpoch);
}

void GenericDirectUploadService::checkForStateChangeOnRemoteProcFinished()
{
    if (d->remoteProcs.size() < MaxConcurrentStatCalls && !d->filesToStat.isEmpty())
        runStat(d->filesToStat.dequeue());
    if (!d->remoteProcs.isEmpty())
        return;
    if (d->state == PreChecking) {
        uploadFiles();
        return;
    }
    QTC_ASSERT(d->state == PostProcessing, return);
    emit progressMessage(tr("All files successfully deployed."));
    setFinished();
    handleDeploymentDone();
}

void GenericDirectUploadService::stopDeployment()
{
    QTC_ASSERT(d->state != Inactive, return);

    setFinished();
    handleDeploymentDone();
}

void GenericDirectUploadService::runStat(const DeployableFile &file)
{
    // We'd like to use --format=%Y, but it's not supported by busybox.
    QtcProcess * const statProc = new QtcProcess(this);
    statProc->setCommand({deviceConfiguration()->filePath("stat"),
              {"-t", Utils::ProcessArgs::quoteArgUnix(file.remoteFilePath())}});
    connect(statProc, &QtcProcess::done, this, [this, statProc, state = d->state] {
        QTC_ASSERT(d->state == state, return);
        const DeployableFile file = d->getFileForProcess(statProc);
        QTC_ASSERT(file.isValid(), return);
        const QDateTime timestamp = timestampFromStat(file, statProc);
        statProc->deleteLater();
        switch (state) {
        case PreChecking:
            if (!timestamp.isValid() || hasRemoteFileChanged(file, timestamp))
                d->filesToUpload.append(file);
            break;
        case PostProcessing:
            if (timestamp.isValid())
                saveDeploymentTimeStamp(file, timestamp);
            break;
        case Inactive:
        case Uploading:
            QTC_CHECK(false);
            break;
        }
        checkForStateChangeOnRemoteProcFinished();
    });
    d->remoteProcs.insert(statProc, file);
    statProc->start();
}

QList<DeployableFile> GenericDirectUploadService::collectFilesToUpload(
        const DeployableFile &deployable) const
{
    QList<DeployableFile> collected;
    FilePath localFile = deployable.localFilePath();
    if (localFile.isDir()) {
        const FilePaths files = localFile.dirEntries(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        const QString remoteDir = deployable.remoteDirectory() + '/' + localFile.fileName();
        for (const FilePath &localFilePath : files)
            collected.append(collectFilesToUpload(DeployableFile(localFilePath, remoteDir)));
    } else {
        collected << deployable;
    }
    return collected;
}

void GenericDirectUploadService::setFinished()
{
    d->state = Inactive;
    d->filesToStat.clear();
    for (auto it = d->remoteProcs.begin(); it != d->remoteProcs.end(); ++it) {
        it.key()->disconnect();
        it.key()->terminate();
    }
    d->remoteProcs.clear();
    d->uploader.stop();
    d->filesToUpload.clear();
}

void GenericDirectUploadService::queryFiles()
{
    QTC_ASSERT(d->state == PreChecking || d->state == PostProcessing, return);
    QTC_ASSERT(d->state == PostProcessing || d->remoteProcs.isEmpty(), return);

    const QList<DeployableFile> &filesToCheck = d->state == PreChecking
            ? d->deployableFiles : d->filesToUpload;
    for (const DeployableFile &file : filesToCheck) {
        if (d->state == PreChecking && (d->incremental != IncrementalDeployment::Enabled
                                        || hasLocalFileChanged(file))) {
            d->filesToUpload.append(file);
            continue;
        }
        if (d->incremental == IncrementalDeployment::NotSupported)
            continue;
        if (d->remoteProcs.size() >= MaxConcurrentStatCalls)
            d->filesToStat << file;
        else
            runStat(file);
    }
    checkForStateChangeOnRemoteProcFinished();
}

void GenericDirectUploadService::uploadFiles()
{
    QTC_ASSERT(d->state == PreChecking, return);
    d->state = Uploading;
    if (d->filesToUpload.empty()) {
        emit progressMessage(tr("No files need to be uploaded."));
        setFinished();
        handleDeploymentDone();
        return;
    }
    emit progressMessage(tr("%n file(s) need to be uploaded.", "", d->filesToUpload.size()));
    FilesToTransfer files;
    for (const DeployableFile &file : qAsConst(d->filesToUpload)) {
        if (!file.localFilePath().exists()) {
            const QString message = tr("Local file \"%1\" does not exist.")
                    .arg(file.localFilePath().toUserOutput());
            if (d->ignoreMissingFiles) {
                emit warningMessage(message);
                continue;
            } else {
                emit errorMessage(message);
                setFinished();
                handleDeploymentDone();
                return;
            }
        }
        files.append({file.localFilePath(),
                      deviceConfiguration()->filePath(file.remoteFilePath())});
    }

    d->uploader.setFilesToTransfer(files);
    d->uploader.start();
}

void GenericDirectUploadService::chmod()
{
    QTC_ASSERT(d->state == PostProcessing, return);
    if (!Utils::HostOsInfo::isWindowsHost())
        return;
    for (const DeployableFile &f : qAsConst(d->filesToUpload)) {
        if (!f.isExecutable())
            continue;
        QtcProcess * const chmodProc = new QtcProcess(this);
        chmodProc->setCommand({deviceConfiguration()->filePath("chmod"),
                {"a+x", Utils::ProcessArgs::quoteArgUnix(f.remoteFilePath())}});
        connect(chmodProc, &QtcProcess::done, this, [this, chmodProc, state = d->state] {
            QTC_ASSERT(state == d->state, return);
            const DeployableFile file = d->getFileForProcess(chmodProc);
            QTC_ASSERT(file.isValid(), return);
            const QString error = chmodProc->errorString();
            if (!error.isEmpty()) {
                emit warningMessage(tr("Remote chmod failed for file \"%1\": %2")
                                    .arg(file.remoteFilePath(), error));
            } else if (chmodProc->exitCode() != 0) {
                emit warningMessage(tr("Remote chmod failed for file \"%1\": %2")
                                    .arg(file.remoteFilePath(),
                                         QString::fromUtf8(chmodProc->readAllStandardError())));
            }
            chmodProc->deleteLater();
            checkForStateChangeOnRemoteProcFinished();
        });
        d->remoteProcs.insert(chmodProc, f);
        chmodProc->start();
    }
}

} //namespace RemoteLinux
