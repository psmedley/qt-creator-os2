/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: BlackBerry (qt@blackberry.com), KDAB (info@kdab.com)
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

#include "qnxdeployqtlibrariesdialog.h"
#include "ui_qnxdeployqtlibrariesdialog.h"

#include "qnxconstants.h"
#include "qnxqtversion.h"

#include <projectexplorer/deployablefile.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <qtsupport/qtversionmanager.h>
#include <remotelinux/genericdirectuploadservice.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QDir>
#include <QMessageBox>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace RemoteLinux;
using namespace Utils;

namespace Qnx {
namespace Internal {

QnxDeployQtLibrariesDialog::QnxDeployQtLibrariesDialog(const IDevice::ConstPtr &device,
                                                       QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::QnxDeployQtLibrariesDialog)
    , m_device(device)
{
    m_ui->setupUi(this);

    const QList<QtVersion*> qtVersions = QtVersionManager::sortVersions(
                QtVersionManager::versions(QtVersion::isValidPredicate(
                equal(&QtVersion::type, QString::fromLatin1(Constants::QNX_QNX_QT)))));
    for (QtVersion *v : qtVersions)
        m_ui->qtLibraryCombo->addItem(v->displayName(), v->uniqueId());

    m_ui->basePathLabel->setText(QString());
    m_ui->remoteDirectory->setText(QLatin1String("/qt"));

    m_uploadService = new GenericDirectUploadService(this);
    m_uploadService->setDevice(m_device);

    connect(m_uploadService, &AbstractRemoteLinuxDeployService::progressMessage,
            this, &QnxDeployQtLibrariesDialog::updateProgress);
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::progressMessage,
            m_ui->deployLogWindow, &QPlainTextEdit::appendPlainText);
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::errorMessage,
            m_ui->deployLogWindow, &QPlainTextEdit::appendPlainText);
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::warningMessage,
            this, [this](const QString &message) {
        if (!message.contains("stat:"))
            m_ui->deployLogWindow->appendPlainText(message);
    });
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::stdOutData,
            m_ui->deployLogWindow, &QPlainTextEdit::appendPlainText);
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::stdErrData,
            m_ui->deployLogWindow, &QPlainTextEdit::appendPlainText);
    connect(m_uploadService, &AbstractRemoteLinuxDeployService::finished,
            this, &QnxDeployQtLibrariesDialog::handleUploadFinished);

    connect(&m_checkDirProcess, &QtcProcess::done,
            this, &QnxDeployQtLibrariesDialog::handleCheckDirDone);
    connect(&m_removeDirProcess, &QtcProcess::done,
            this, &QnxDeployQtLibrariesDialog::handleRemoveDirDone);

    connect(m_ui->deployButton, &QAbstractButton::clicked,
            this, &QnxDeployQtLibrariesDialog::deployLibraries);
    connect(m_ui->closeButton, &QAbstractButton::clicked,
            this, &QWidget::close);
}

QnxDeployQtLibrariesDialog::~QnxDeployQtLibrariesDialog()
{
    delete m_ui;
}

int QnxDeployQtLibrariesDialog::execAndDeploy(int qtVersionId, const QString &remoteDirectory)
{
    m_ui->remoteDirectory->setText(remoteDirectory);
    m_ui->qtLibraryCombo->setCurrentIndex(m_ui->qtLibraryCombo->findData(qtVersionId));

    deployLibraries();
    return exec();
}

void QnxDeployQtLibrariesDialog::closeEvent(QCloseEvent *event)
{
    // A disabled Deploy button indicates the upload is still running
    if (!m_ui->deployButton->isEnabled()) {
        const int answer = QMessageBox::question(this, windowTitle(),
            tr("Closing the dialog will stop the deployment. Are you sure you want to do this?"),
            QMessageBox::Yes | QMessageBox::No);
        if (answer == QMessageBox::No)
            event->ignore();
        else if (answer == QMessageBox::Yes)
            m_uploadService->stop();
    }
}

void QnxDeployQtLibrariesDialog::deployLibraries()
{
    QTC_ASSERT(m_state == Inactive, return);

    if (m_ui->remoteDirectory->text().isEmpty()) {
        QMessageBox::warning(this, windowTitle(),
                             tr("Please input a remote directory to deploy to."));
        return;
    }

    QTC_ASSERT(!m_device.isNull(), return);

    m_progressCount = 0;
    m_ui->deployProgress->setValue(0);
    m_ui->remoteDirectory->setEnabled(false);
    m_ui->deployButton->setEnabled(false);
    m_ui->qtLibraryCombo->setEnabled(false);
    m_ui->deployLogWindow->clear();

    startCheckDirProcess();
}

void QnxDeployQtLibrariesDialog::startUpload()
{
    QTC_CHECK(m_state == CheckingRemoteDirectory || m_state == RemovingRemoteDirectory);

    m_state = Uploading;

    QList<DeployableFile> filesToUpload = gatherFiles();

    m_ui->deployProgress->setRange(0, filesToUpload.count());

    m_uploadService->setDeployableFiles(filesToUpload);
    m_uploadService->start();
}

void QnxDeployQtLibrariesDialog::updateProgress(const QString &progressMessage)
{
    QTC_CHECK(m_state == Uploading);

    const int progress = progressMessage.count("sftp> put") + progressMessage.count("sftp> ln -s");
    if (progress != 0) {
        m_progressCount += progress;
        m_ui->deployProgress->setValue(m_progressCount);
    }
}

void QnxDeployQtLibrariesDialog::handleUploadFinished()
{
    m_ui->remoteDirectory->setEnabled(true);
    m_ui->deployButton->setEnabled(true);
    m_ui->qtLibraryCombo->setEnabled(true);

    m_state = Inactive;
}

QList<DeployableFile> QnxDeployQtLibrariesDialog::gatherFiles()
{
    QList<DeployableFile> result;

    const int qtVersionId =
            m_ui->qtLibraryCombo->itemData(m_ui->qtLibraryCombo->currentIndex()).toInt();


    auto qtVersion = dynamic_cast<const QnxQtVersion *>(QtVersionManager::version(qtVersionId));

    QTC_ASSERT(qtVersion, return result);

    if (HostOsInfo::isWindowsHost()) {
        result.append(gatherFiles(qtVersion->libraryPath().toString(),
                                  QString(),
                                  QStringList() << QLatin1String("*.so.?")));
        result.append(gatherFiles(qtVersion->libraryPath().toString() + QLatin1String("/fonts")));
    } else {
        result.append(gatherFiles(qtVersion->libraryPath().toString()));
    }

    result.append(gatherFiles(qtVersion->pluginPath().toString()));
    result.append(gatherFiles(qtVersion->importsPath().toString()));
    result.append(gatherFiles(qtVersion->qmlPath().toString()));
    return result;
}

QList<DeployableFile> QnxDeployQtLibrariesDialog::gatherFiles(
        const QString &dirPath, const QString &baseDirPath, const QStringList &nameFilters)
{
    QList<DeployableFile> result;
    if (dirPath.isEmpty())
        return result;

    static const QStringList unusedDirs = {"include", "mkspecs", "cmake", "pkgconfig"};
    const QString dp = dirPath.endsWith('/') ? dirPath.left(dirPath.size() - 1) : dirPath;
    if (unusedDirs.contains(dp))
        return result;

    const QDir dir(dirPath);
    QFileInfoList list = dir.entryInfoList(nameFilters,
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (auto &fileInfo : list) {
        if (fileInfo.isDir()) {
            result.append(gatherFiles(fileInfo.absoluteFilePath(), baseDirPath.isEmpty() ?
                                          dirPath : baseDirPath));
        } else {
            static const QStringList unusedSuffixes = {"cmake", "la", "prl", "a", "pc"};
            if (unusedSuffixes.contains(fileInfo.suffix()))
                continue;

            QString remoteDir;
            if (baseDirPath.isEmpty()) {
                remoteDir = fullRemoteDirectory() + QLatin1Char('/') +
                        QFileInfo(dirPath).baseName();
            } else {
                QDir baseDir(baseDirPath);
                baseDir.cdUp();
                remoteDir = fullRemoteDirectory() + QLatin1Char('/') +
                        baseDir.relativeFilePath(dirPath);
            }
            result.append(DeployableFile(FilePath::fromString(fileInfo.absoluteFilePath()),
                                         remoteDir));
        }
    }

    return result;
}

QString QnxDeployQtLibrariesDialog::fullRemoteDirectory() const
{
    return m_ui->remoteDirectory->text();
}

void QnxDeployQtLibrariesDialog::startCheckDirProcess()
{
    QTC_CHECK(m_state == Inactive);
    m_state = CheckingRemoteDirectory;
    m_ui->deployLogWindow->appendPlainText(tr("Checking existence of \"%1\"")
                                           .arg(fullRemoteDirectory()));
    m_checkDirProcess.setCommand({m_device->filePath("test"),
                                  {"-d", fullRemoteDirectory()}});
    m_checkDirProcess.start();
}

void QnxDeployQtLibrariesDialog::startRemoveDirProcess()
{
    QTC_CHECK(m_state == CheckingRemoteDirectory);
    m_state = RemovingRemoteDirectory;
    m_ui->deployLogWindow->appendPlainText(tr("Removing \"%1\"").arg(fullRemoteDirectory()));
    m_removeDirProcess.setCommand({m_device->filePath("rm"),
                                  {"-rf", fullRemoteDirectory()}});
    m_removeDirProcess.start();
}

void QnxDeployQtLibrariesDialog::handleCheckDirDone()
{
    QTC_CHECK(m_state == CheckingRemoteDirectory);
    if (handleError(m_checkDirProcess))
        return;

    if (m_checkDirProcess.exitCode() == 0) { // Directory exists
        const int answer = QMessageBox::question(this, windowTitle(),
            tr("The remote directory \"%1\" already exists.\n"
               "Deploying to that directory will remove any files already present.\n\n"
               "Are you sure you want to continue?").arg(fullRemoteDirectory()),
               QMessageBox::Yes | QMessageBox::No);
        if (answer == QMessageBox::Yes)
            startRemoveDirProcess();
        else
            handleUploadFinished();
    } else {
        startUpload();
    }
}

void QnxDeployQtLibrariesDialog::handleRemoveDirDone()
{
    QTC_CHECK(m_state == RemovingRemoteDirectory);
    if (handleError(m_removeDirProcess))
        return;

    QTC_ASSERT(m_removeDirProcess.exitCode() == 0, return);
    startUpload();
}

// Returns true if the error appeared, false when finished with success
bool QnxDeployQtLibrariesDialog::handleError(const QtcProcess &process)
{
    if (process.result() == ProcessResult::FinishedWithSuccess)
        return false;

    m_ui->deployLogWindow->appendPlainText(tr("Connection failed: %1").arg(process.errorString()));
    handleUploadFinished();
    return true;
}


} // namespace Internal
} // namespace Qnx
