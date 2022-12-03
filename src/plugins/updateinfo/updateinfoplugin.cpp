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

#include "updateinfoplugin.h"

#include "settingspage.h"
#include "updateinfotools.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/futureprogress.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <coreplugin/settingsdatabase.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>
#include <utils/infobar.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>

#include <QDate>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMetaEnum>
#include <QPointer>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVersionNumber>

#include <memory>

Q_LOGGING_CATEGORY(updateLog, "qtc.updateinfo", QtWarningMsg)

const char UpdaterGroup[] = "Updater";
const char MaintenanceToolKey[] = "MaintenanceTool";
const char AutomaticCheckKey[] = "AutomaticCheck";
const char CheckForNewQtVersionsKey[] = "CheckForNewQtVersions";
const char CheckIntervalKey[] = "CheckUpdateInterval";
const char LastCheckDateKey[] = "LastCheckDate";
const char LastMaxQtVersionKey[] = "LastMaxQtVersion";
const quint32 OneMinute = 60000;
const quint32 OneHour = 3600000;
const char InstallUpdates[] = "UpdateInfo.InstallUpdates";
const char InstallQtUpdates[] = "UpdateInfo.InstallQtUpdates";

using namespace Core;
using namespace Utils;

namespace UpdateInfo {
namespace Internal {

class UpdateInfoPluginPrivate
{
public:
    QString m_maintenanceTool;
    std::unique_ptr<QtcProcess> m_maintenanceToolProcess;
    QPointer<FutureProgress> m_progress;
    QString m_updateOutput;
    QString m_packagesOutput;
    QTimer *m_checkUpdatesTimer = nullptr;
    struct Settings
    {
        bool automaticCheck = true;
        UpdateInfoPlugin::CheckUpdateInterval checkInterval = UpdateInfoPlugin::WeeklyCheck;
        bool checkForQtVersions = true;
    };
    Settings m_settings;
    QDate m_lastCheckDate;
    QVersionNumber m_lastMaxQtVersion;
};

UpdateInfoPlugin::UpdateInfoPlugin()
    : d(new UpdateInfoPluginPrivate)
{
    d->m_checkUpdatesTimer = new QTimer(this);
    d->m_checkUpdatesTimer->setTimerType(Qt::VeryCoarseTimer);
    d->m_checkUpdatesTimer->setInterval(OneHour);
    connect(d->m_checkUpdatesTimer, &QTimer::timeout,
            this, &UpdateInfoPlugin::doAutoCheckForUpdates);
}

UpdateInfoPlugin::~UpdateInfoPlugin()
{
    stopCheckForUpdates();
    if (!d->m_maintenanceTool.isEmpty())
        saveSettings();

    delete d;
}

void UpdateInfoPlugin::startAutoCheckForUpdates()
{
    doAutoCheckForUpdates();

    d->m_checkUpdatesTimer->start();
}

void UpdateInfoPlugin::stopAutoCheckForUpdates()
{
    d->m_checkUpdatesTimer->stop();
}

void UpdateInfoPlugin::doAutoCheckForUpdates()
{
    if (d->m_maintenanceToolProcess)
        return; // update task is still running (might have been run manually just before)

    if (nextCheckDate().isValid() && nextCheckDate() > QDate::currentDate())
        return; // not a time for check yet

    startCheckForUpdates();
}

void UpdateInfoPlugin::startCheckForUpdates()
{
    stopCheckForUpdates();

    QFutureInterface<void> futureIf;
    FutureProgress *futureProgress
        = ProgressManager::addTimedTask(futureIf,
                                        tr("Checking for Updates"),
                                        Id("UpdateInfo.CheckingForUpdates"),
                                        60);
    futureProgress->setKeepOnFinish(FutureProgress::KeepOnFinishTillUserInteraction);
    futureProgress->setSubtitleVisibleInStatusBar(true);
    connect(futureProgress, &FutureProgress::canceled, this, [this, futureIf]() mutable {
        futureIf.reportCanceled();
        futureIf.reportFinished();
        stopCheckForUpdates();
    });

    d->m_maintenanceToolProcess.reset(new QtcProcess);
    d->m_maintenanceToolProcess->setCommand({Utils::FilePath::fromString(d->m_maintenanceTool),
                                             {"ch", "-g", "*=false,ifw.package.*=true"}});
    d->m_maintenanceToolProcess->setTimeoutS(3 * 60); // 3 minutes
    // TODO handle error
    connect(
        d->m_maintenanceToolProcess.get(),
        &QtcProcess::done,
        this,
        [this, futureIf]() mutable {
            if (d->m_maintenanceToolProcess->result() == ProcessResult::FinishedWithSuccess) {
                d->m_updateOutput = d->m_maintenanceToolProcess->cleanedStdOut();
                if (d->m_settings.checkForQtVersions) {
                    d->m_maintenanceToolProcess.reset(new QtcProcess);
                    d->m_maintenanceToolProcess->setCommand(
                        {Utils::FilePath::fromString(d->m_maintenanceTool),
                         {"se", "qt[.]qt[0-9][.][0-9]+$", "-g", "*=false,ifw.package.*=true"}});
                    d->m_maintenanceToolProcess->setTimeoutS(3 * 60); // 3 minutes
                    connect(
                        d->m_maintenanceToolProcess.get(),
                        &QtcProcess::done,
                        this,
                        [this, futureIf]() mutable {
                            if (d->m_maintenanceToolProcess->result()
                                == ProcessResult::FinishedWithSuccess) {
                                d->m_packagesOutput = d->m_maintenanceToolProcess->cleanedStdOut();
                                d->m_maintenanceToolProcess.reset();
                                futureIf.reportFinished();
                                checkForUpdatesFinished();
                            } else {
                                futureIf.reportCanceled(); // is used to indicate error
                                futureIf.reportFinished();
                            }
                        },
                        Qt::QueuedConnection);
                    d->m_maintenanceToolProcess->start();
                } else {
                    d->m_maintenanceToolProcess.reset();
                    futureIf.reportFinished();
                    checkForUpdatesFinished();
                }
            } else {
                futureIf.reportCanceled(); // is used to indicate error
                futureIf.reportFinished();
            }
        },
        Qt::QueuedConnection);

    d->m_maintenanceToolProcess->start();
    futureIf.reportStarted();

    emit checkForUpdatesRunningChanged(true);
}

void UpdateInfoPlugin::stopCheckForUpdates()
{
    if (!d->m_maintenanceToolProcess)
        return;

    d->m_maintenanceToolProcess->disconnect();
    d->m_maintenanceToolProcess.reset();
    d->m_updateOutput.clear();
    d->m_packagesOutput.clear();
    emit checkForUpdatesRunningChanged(false);
}

static void showUpdateInfo(const QList<Update> &updates, const std::function<void()> &startUpdater)
{
    Utils::InfoBarEntry info(InstallUpdates,
                             UpdateInfoPlugin::tr("New updates are available. Start the update?"));
    info.addCustomButton(UpdateInfoPlugin::tr("Start Update"), [startUpdater] {
        Core::ICore::infoBar()->removeInfo(InstallUpdates);
        startUpdater();
    });
    info.setDetailsWidgetCreator([updates]() -> QWidget * {
        const QString updateText = Utils::transform(updates, [](const Update &u) {
                                       return u.version.isEmpty()
                                                  ? u.name
                                                  : UpdateInfoPlugin::tr("%1 (%2)",
                                                                         "Package name and version")
                                                        .arg(u.name, u.version);
                                   }).join("</li><li>");
        auto label = new QLabel;
        label->setText("<qt><p>" + UpdateInfoPlugin::tr("Available updates:") + "<ul><li>"
                       + updateText + "</li></ul></p></qt>");
        label->setContentsMargins(0, 0, 0, 8);
        return label;
    });
    Core::ICore::infoBar()->removeInfo(InstallUpdates); // remove any existing notifications
    Core::ICore::infoBar()->unsuppressInfo(InstallUpdates);
    Core::ICore::infoBar()->addInfo(info);
}

static void showQtUpdateInfo(const QtPackage &package,
                             const std::function<void()> &startPackageManager)
{
    Utils::InfoBarEntry info(InstallQtUpdates,
                             UpdateInfoPlugin::tr(
                                 "%1 is available. Check the <a %2>Qt blog</a> for details.")
                                 .arg(package.displayName,
                                      QString("href=\"https://www.qt.io/blog/tag/releases\"")));
    info.addCustomButton(UpdateInfoPlugin::tr("Start Package Manager"), [startPackageManager] {
        Core::ICore::infoBar()->removeInfo(InstallQtUpdates);
        startPackageManager();
    });
    info.addCustomButton(UpdateInfoPlugin::tr("Open Settings"), [] {
        Core::ICore::infoBar()->removeInfo(InstallQtUpdates);
        Core::ICore::showOptionsDialog(FILTER_OPTIONS_PAGE_ID);
    });
    Core::ICore::infoBar()->removeInfo(InstallQtUpdates); // remove any existing notifications
    Core::ICore::infoBar()->unsuppressInfo(InstallQtUpdates);
    Core::ICore::infoBar()->addInfo(info);
}

void UpdateInfoPlugin::checkForUpdatesFinished()
{
    setLastCheckDate(QDate::currentDate());

    qCDebug(updateLog) << "--- MaintenanceTool output (updates):";
    qCDebug(updateLog) << qPrintable(d->m_updateOutput);
    qCDebug(updateLog) << "--- MaintenanceTool output (packages):";
    qCDebug(updateLog) << qPrintable(d->m_packagesOutput);

    stopCheckForUpdates();

    const QList<Update> updates = availableUpdates(d->m_updateOutput);
    const QList<QtPackage> qtPackages = availableQtPackages(d->m_packagesOutput);
    if (updateLog().isDebugEnabled()) {
        qCDebug(updateLog) << "--- Available updates:";
        for (const Update &u : updates)
            qCDebug(updateLog) << u.name << u.version;
        qCDebug(updateLog) << "--- Available Qt packages:";
        for (const QtPackage &p : qtPackages) {
            qCDebug(updateLog) << p.displayName << p.version << "installed:" << p.installed
                               << "prerelease:" << p.isPrerelease;
        }
    }
    Utils::optional<QtPackage> qtToNag = qtToNagAbout(qtPackages, &d->m_lastMaxQtVersion);

    if (!updates.isEmpty() || qtToNag) {
        // progress details are shown until user interaction for the "no updates" case,
        // so we can show the "No updates found" text, but if we have updates we don't
        // want to keep it around
        if (d->m_progress)
            d->m_progress->setKeepOnFinish(FutureProgress::HideOnFinish);
        emit newUpdatesAvailable(true);
        if (!updates.isEmpty())
            showUpdateInfo(updates, [this] { startUpdater(); });
        if (qtToNag)
            showQtUpdateInfo(*qtToNag, [this] { startPackageManager(); });
    } else {
        emit newUpdatesAvailable(false);
        if (d->m_progress)
            d->m_progress->setSubtitle(tr("No updates found."));
    }
}

bool UpdateInfoPlugin::isCheckForUpdatesRunning() const
{
    return d->m_maintenanceToolProcess.get() != nullptr;
}

void UpdateInfoPlugin::extensionsInitialized()
{
    if (isAutomaticCheck())
        QTimer::singleShot(OneMinute, this, &UpdateInfoPlugin::startAutoCheckForUpdates);
}

bool UpdateInfoPlugin::initialize(const QStringList & /* arguments */, QString *errorMessage)
{
    loadSettings();

    if (d->m_maintenanceTool.isEmpty()) {
        *errorMessage = tr("Could not determine location of maintenance tool. Please check "
            "your installation if you did not enable this plugin manually.");
        return false;
    }

    if (!QFileInfo(d->m_maintenanceTool).isExecutable()) {
        *errorMessage = tr("The maintenance tool at \"%1\" is not an executable. Check your installation.")
            .arg(d->m_maintenanceTool);
        d->m_maintenanceTool.clear();
        return false;
    }

    connect(ICore::instance(), &ICore::saveSettingsRequested,
            this, &UpdateInfoPlugin::saveSettings);

    (void) new SettingsPage(this);

    QAction *checkForUpdatesAction = new QAction(tr("Check for Updates"), this);
    checkForUpdatesAction->setMenuRole(QAction::ApplicationSpecificRole);
    Core::Command *checkForUpdatesCommand = Core::ActionManager::registerAction(checkForUpdatesAction, "Updates.CheckForUpdates");
    connect(checkForUpdatesAction, &QAction::triggered, this, &UpdateInfoPlugin::startCheckForUpdates);
    ActionContainer *const helpContainer = ActionManager::actionContainer(Core::Constants::M_HELP);
    helpContainer->addAction(checkForUpdatesCommand, Constants::G_HELP_UPDATES);

    return true;
}

void UpdateInfoPlugin::loadSettings() const
{
    UpdateInfoPluginPrivate::Settings def;
    QSettings *settings = ICore::settings();
    const QString updaterKey = QLatin1String(UpdaterGroup) + '/';
    d->m_maintenanceTool = settings->value(updaterKey + MaintenanceToolKey).toString();
    d->m_lastCheckDate = settings->value(updaterKey + LastCheckDateKey, QDate()).toDate();
    d->m_settings.automaticCheck
        = settings->value(updaterKey + AutomaticCheckKey, def.automaticCheck).toBool();
    const QMetaObject *mo = metaObject();
    const QMetaEnum me = mo->enumerator(mo->indexOfEnumerator(CheckIntervalKey));
    if (QTC_GUARD(me.isValid())) {
        const QString checkInterval = settings
                                          ->value(updaterKey + CheckIntervalKey,
                                                  me.valueToKey(def.checkInterval))
                                          .toString();
        bool ok = false;
        const int newValue = me.keyToValue(checkInterval.toUtf8(), &ok);
        if (ok)
            d->m_settings.checkInterval = static_cast<CheckUpdateInterval>(newValue);
    }
    const QString lastMaxQtVersionString = settings->value(updaterKey + LastMaxQtVersionKey)
                                               .toString();
    d->m_lastMaxQtVersion = QVersionNumber::fromString(lastMaxQtVersionString);
    d->m_settings.checkForQtVersions
        = settings->value(updaterKey + CheckForNewQtVersionsKey, def.checkForQtVersions).toBool();
}

void UpdateInfoPlugin::saveSettings()
{
    UpdateInfoPluginPrivate::Settings def;
    Utils::QtcSettings *settings = ICore::settings();
    settings->beginGroup(UpdaterGroup);
    settings->setValueWithDefault(LastCheckDateKey, d->m_lastCheckDate, QDate());
    settings->setValueWithDefault(AutomaticCheckKey,
                                  d->m_settings.automaticCheck,
                                  def.automaticCheck);
    // Note: don't save MaintenanceToolKey on purpose! This setting may be set only by installer.
    // If creator is run not from installed SDK, the setting can be manually created here:
    // [CREATOR_INSTALLATION_LOCATION]/share/qtcreator/QtProject/QtCreator.ini or
    // [CREATOR_INSTALLATION_LOCATION]/Qt Creator.app/Contents/Resources/QtProject/QtCreator.ini on OS X
    const QMetaObject *mo = metaObject();
    const QMetaEnum me = mo->enumerator(mo->indexOfEnumerator(CheckIntervalKey));
    settings->setValueWithDefault(CheckIntervalKey,
                                  QString::fromUtf8(me.valueToKey(d->m_settings.checkInterval)),
                                  QString::fromUtf8(me.valueToKey(def.checkInterval)));
    settings->setValueWithDefault(LastMaxQtVersionKey, d->m_lastMaxQtVersion.toString());
    settings->setValueWithDefault(CheckForNewQtVersionsKey,
                                  d->m_settings.checkForQtVersions,
                                  def.checkForQtVersions);
    settings->endGroup();
}

bool UpdateInfoPlugin::isAutomaticCheck() const
{
    return d->m_settings.automaticCheck;
}

void UpdateInfoPlugin::setAutomaticCheck(bool on)
{
    if (d->m_settings.automaticCheck == on)
        return;

    d->m_settings.automaticCheck = on;
    if (on)
        startAutoCheckForUpdates();
    else
        stopAutoCheckForUpdates();
}

UpdateInfoPlugin::CheckUpdateInterval UpdateInfoPlugin::checkUpdateInterval() const
{
    return d->m_settings.checkInterval;
}

void UpdateInfoPlugin::setCheckUpdateInterval(UpdateInfoPlugin::CheckUpdateInterval interval)
{
    if (d->m_settings.checkInterval == interval)
        return;

    d->m_settings.checkInterval = interval;
}

bool UpdateInfoPlugin::isCheckingForQtVersions() const
{
    return d->m_settings.checkForQtVersions;
}

void UpdateInfoPlugin::setCheckingForQtVersions(bool on)
{
    d->m_settings.checkForQtVersions = on;
}

QDate UpdateInfoPlugin::lastCheckDate() const
{
    return d->m_lastCheckDate;
}

void UpdateInfoPlugin::setLastCheckDate(const QDate &date)
{
    if (d->m_lastCheckDate == date)
        return;

    d->m_lastCheckDate = date;
    emit lastCheckDateChanged(date);
}

QDate UpdateInfoPlugin::nextCheckDate() const
{
    return nextCheckDate(d->m_settings.checkInterval);
}

QDate UpdateInfoPlugin::nextCheckDate(CheckUpdateInterval interval) const
{
    if (!d->m_lastCheckDate.isValid())
        return QDate();

    if (interval == DailyCheck)
        return d->m_lastCheckDate.addDays(1);
    if (interval == WeeklyCheck)
        return d->m_lastCheckDate.addDays(7);
    return d->m_lastCheckDate.addMonths(1);
}

void UpdateInfoPlugin::startUpdater()
{
    Utils::QtcProcess::startDetached(
        {Utils::FilePath::fromString(d->m_maintenanceTool), {"--updater"}});
}

void UpdateInfoPlugin::startPackageManager()
{
    Utils::QtcProcess::startDetached(
        {Utils::FilePath::fromString(d->m_maintenanceTool), {"--start-package-manager"}});
}

} //namespace Internal
} //namespace UpdateInfo
