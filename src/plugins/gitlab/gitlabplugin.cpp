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

#include "gitlabplugin.h"

#include "gitlabdialog.h"
#include "gitlaboptionspage.h"
#include "gitlabparameters.h"
#include "gitlabprojectsettings.h"
#include "queryrunner.h"
#include "resultparser.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/icore.h>
#include <git/gitplugin.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/session.h>
#include <utils/qtcassert.h>
#include <vcsbase/vcsoutputwindow.h>

#include <QAction>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>

namespace GitLab {
namespace Constants {
const char GITLAB_OPEN_VIEW[] = "GitLab.OpenView";
} // namespace Constants

class GitLabPluginPrivate : public QObject
{
public:
    GitLabParameters parameters;
    GitLabOptionsPage optionsPage{&parameters};
    QHash<ProjectExplorer::Project *, GitLabProjectSettings *> projectSettings;
    QPointer<GitLabDialog> dialog;

    QTimer notificationTimer;
    QString projectName;
    Utils::Id serverId;
    bool runningQuery = false;

    void setupNotificationTimer();
    void fetchEvents();
    void fetchUser();
    void createAndSendEventsRequest(const QDateTime timeStamp, int page = -1);
    void handleUser(const User &user);
    void handleEvents(const Events &events, const QDateTime &timeStamp);
};

static GitLabPluginPrivate *dd = nullptr;

GitLabPlugin::GitLabPlugin()
{
}

GitLabPlugin::~GitLabPlugin()
{
    if (!dd->projectSettings.isEmpty()) {
        qDeleteAll(dd->projectSettings);
        dd->projectSettings.clear();
    }
    delete dd;
    dd = nullptr;
}

bool GitLabPlugin::initialize(const QStringList & /*arguments*/, QString * /*errorString*/)
{
    dd = new GitLabPluginPrivate;
    dd->parameters.fromSettings(Core::ICore::settings());
    auto panelFactory = new ProjectExplorer::ProjectPanelFactory;
    panelFactory->setPriority(999);
    panelFactory->setDisplayName(tr("GitLab"));
    panelFactory->setCreateWidgetFunction([](ProjectExplorer::Project *project) {
        return new GitLabProjectSettingsWidget(project);
    });
    ProjectExplorer::ProjectPanelFactory::registerFactory(panelFactory);
    QAction *openViewAction = new QAction(tr("GitLab..."), this);
    auto gitlabCommand = Core::ActionManager::registerAction(openViewAction,
                                                             Constants::GITLAB_OPEN_VIEW);
    connect(openViewAction, &QAction::triggered, this, &GitLabPlugin::openView);
    Core::ActionContainer *ac = Core::ActionManager::actionContainer(Core::Constants::M_TOOLS);
    ac->addAction(gitlabCommand);
    connect(&dd->optionsPage, &GitLabOptionsPage::settingsChanged, this, [] {
        if (dd->dialog)
            dd->dialog->updateRemotes();
    });
    connect(ProjectExplorer::SessionManager::instance(),
            &ProjectExplorer::SessionManager::startupProjectChanged,
            this, &GitLabPlugin::onStartupProjectChanged);
    return true;
}

void GitLabPlugin::openView()
{
    if (dd->dialog.isNull()) {
        while (!dd->parameters.isValid()) {
            QMessageBox::warning(Core::ICore::dialogParent(), tr("Error"),
                                 tr("Invalid GitLab configuration. For a fully functional "
                                    "configuration, you need to set up host name or address and "
                                    "an access token. Providing the path to curl is mandatory."));
            if (!Core::ICore::showOptionsDialog("GitLab"))
                return;
        }
        GitLabDialog *gitlabD = new GitLabDialog(Core::ICore::dialogParent());
        gitlabD->setModal(true);
        Core::ICore::registerWindow(gitlabD, Core::Context("Git.GitLab"));
        dd->dialog = gitlabD;
    }
    const Qt::WindowStates state = dd->dialog->windowState();
    if (state & Qt::WindowMinimized)
        dd->dialog->setWindowState(state & ~Qt::WindowMinimized);
    dd->dialog->show();
    dd->dialog->raise();
}

void GitLabPlugin::onStartupProjectChanged()
{
    QTC_ASSERT(dd, return);
    disconnect(&dd->notificationTimer);
    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    if (!project) {
        dd->notificationTimer.stop();
        return;
    }

    const GitLabProjectSettings *projSettings = projectSettings(project);
    if (!projSettings->isLinked()) {
        dd->notificationTimer.stop();
        return;
    }

    dd->fetchEvents();
    dd->setupNotificationTimer();
}

void GitLabPluginPrivate::setupNotificationTimer()
{
    // make interval configurable?
    notificationTimer.setInterval(15 * 60 * 1000);
    QObject::connect(&notificationTimer, &QTimer::timeout, this, &GitLabPluginPrivate::fetchEvents);
    notificationTimer.start();
}

void GitLabPluginPrivate::fetchEvents()
{
    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    QTC_ASSERT(project, return);

    if (runningQuery)
        return;

    const GitLabProjectSettings *projSettings = GitLabPlugin::projectSettings(project);
    projectName = projSettings->currentProject();
    serverId = projSettings->currentServer();

    const QDateTime lastRequest = projSettings->lastRequest();
    if (!lastRequest.isValid()) { // we haven't queried events for this project yet
        fetchUser();
        return;
    }
    createAndSendEventsRequest(lastRequest);
}

void GitLabPluginPrivate::fetchUser()
{
    if (runningQuery)
        return;

    const Query query(Query::User);
    QueryRunner *runner = new QueryRunner(query, serverId, this);
    QObject::connect(runner, &QueryRunner::resultRetrieved, this, [this](const QByteArray &result) {
        handleUser(ResultParser::parseUser(result));
    });
    QObject::connect(runner, &QueryRunner::finished, [runner]() { runner->deleteLater(); });
    runningQuery = true;
    runner->start();
}

void GitLabPluginPrivate::createAndSendEventsRequest(const QDateTime timeStamp, int page)
{
    if (runningQuery)
        return;

    Query query(Query::Events, {projectName});
    QStringList additional = {"sort=asc"};

    QDateTime after = timeStamp.addDays(-1);
    additional.append(QLatin1String("after=%1").arg(after.toString("yyyy-MM-dd")));
    query.setAdditionalParameters(additional);

    if (page > 1)
        query.setPageParameter(page);

    QueryRunner *runner = new QueryRunner(query, serverId, this);
    QObject::connect(runner, &QueryRunner::resultRetrieved, this,
                     [this, timeStamp](const QByteArray &result) {
        handleEvents(ResultParser::parseEvents(result), timeStamp);
    });
    QObject::connect(runner, &QueryRunner::finished, [runner]() { runner->deleteLater(); });
    runningQuery = true;
    runner->start();
}

void GitLabPluginPrivate::handleUser(const User &user)
{
    runningQuery = false;

    QTC_ASSERT(user.error.message.isEmpty(), return);
    const QDateTime timeStamp = QDateTime::fromString(user.lastLogin, Qt::ISODateWithMs);
    createAndSendEventsRequest(timeStamp);
}

void GitLabPluginPrivate::handleEvents(const Events &events, const QDateTime &timeStamp)
{
    runningQuery = false;

    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    QTC_ASSERT(project, return);

    GitLabProjectSettings *projSettings = GitLabPlugin::projectSettings(project);
    QTC_ASSERT(projSettings->currentProject() == projectName, return);

    if (!projSettings->isLinked()) // link state has changed meanwhile - ignore the request
        return;

    if (!events.error.message.isEmpty()) {
        VcsBase::VcsOutputWindow::appendError("GitLab: Error while fetching events. "
                                              + events.error.message + '\n');
        return;
    }

    QDateTime lastTimeStamp;
    for (const Event &event : events.events) {
        const QDateTime eventTimeStamp = QDateTime::fromString(event.timeStamp, Qt::ISODateWithMs);
        if (!timeStamp.isValid() || timeStamp < eventTimeStamp) {
            VcsBase::VcsOutputWindow::appendMessage("GitLab: " + event.toMessage());
            if (!lastTimeStamp.isValid() || lastTimeStamp < eventTimeStamp)
                lastTimeStamp = eventTimeStamp;
        }
    }
    if (lastTimeStamp.isValid()) {
        if (auto outputWindow = VcsBase::VcsOutputWindow::instance())
            outputWindow->flash();
        projSettings->setLastRequest(lastTimeStamp);
    }

    if (events.pageInfo.currentPage < events.pageInfo.totalPages)
        createAndSendEventsRequest(timeStamp, events.pageInfo.currentPage + 1);
}

QList<GitLabServer> GitLabPlugin::allGitLabServers()
{
    QTC_ASSERT(dd, return {});
    return dd->parameters.gitLabServers;
}

GitLabServer GitLabPlugin::gitLabServerForId(const Utils::Id &id)
{
    QTC_ASSERT(dd, return {});
    return dd->parameters.serverForId(id);
}

GitLabParameters *GitLabPlugin::globalParameters()
{
    return &dd->parameters;
}

GitLabProjectSettings *GitLabPlugin::projectSettings(ProjectExplorer::Project *project)
{
    QTC_ASSERT(project, return nullptr);
    QTC_ASSERT(dd, return nullptr);
    auto &settings = dd->projectSettings[project];
    if (!settings)
        settings = new GitLabProjectSettings(project);
    return settings;
}

GitLabOptionsPage *GitLabPlugin::optionsPage()
{
    QTC_ASSERT(dd, return {});
    return &dd->optionsPage;
}

bool GitLabPlugin::handleCertificateIssue(const Utils::Id &serverId)
{
    QTC_ASSERT(dd, return false);

    GitLabServer server = dd->parameters.serverForId(serverId);
    if (QMessageBox::question(Core::ICore::dialogParent(),
                              QCoreApplication::translate(
                                  "GitLab::GitLabDialog", "Certificate Error"),
                              QCoreApplication::translate(
                                  "GitLab::GitLabDialog",
                                  "Server certificate for %1 cannot be authenticated.\n"
                                  "Do you want to disable SSL verification for this server?\n"
                                  "Note: This can expose you to man-in-the-middle attack.")
                              .arg(server.host))
            == QMessageBox::Yes) {
        int index = dd->parameters.gitLabServers.indexOf(server);
        server.validateCert = false;
        dd->parameters.gitLabServers.replace(index, server);
        emit dd->optionsPage.settingsChanged();
        return true;
    }
    return false;
}

void GitLabPlugin::linkedStateChanged(bool enabled)
{
    QTC_ASSERT(dd, return);

    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    if (project) {
        const GitLabProjectSettings *pSettings = projectSettings(project);
        dd->serverId = pSettings->currentServer();
        dd->projectName = pSettings->currentProject();
    } else {
        dd->serverId = Utils::Id();
        dd->projectName = QString();
    }

    if (enabled) {
        dd->fetchEvents();
        dd->setupNotificationTimer();
    } else {
        QObject::disconnect(&dd->notificationTimer, &QTimer::timeout,
                            dd, &GitLabPluginPrivate::fetchEvents);
        dd->notificationTimer.stop();
    }
}

} // namespace GitLab
