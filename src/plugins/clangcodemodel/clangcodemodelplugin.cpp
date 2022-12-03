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

#include "clangcodemodelplugin.h"

#include "clangconstants.h"
#include "clangutils.h"

#ifdef WITH_TESTS
#  include "test/activationsequenceprocessortest.h"
#  include "test/clangbatchfileprocessor.h"
#  include "test/clangdtests.h"
#  include "test/clangfixittest.h"
#endif

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/progressmanager.h>

#include <cppeditor/clangdiagnosticconfig.h>
#include <cppeditor/cppmodelmanager.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>

#include <texteditor/textmark.h>

#include <utils/temporarydirectory.h>
#include <utils/runextensions.h>

using namespace Utils;

namespace ClangCodeModel {
namespace Internal {

void ClangCodeModelPlugin::generateCompilationDB()
{
    using namespace CppEditor;

    ProjectExplorer::Target *target = ProjectExplorer::SessionManager::startupTarget();
    if (!target)
        return;

    const auto projectInfo = CppModelManager::instance()->projectInfo(target->project());
    if (!projectInfo)
        return;
    FilePath baseDir = projectInfo->buildRoot();
    if (baseDir == target->project()->projectDirectory())
        baseDir = TemporaryDirectory::masterDirectoryFilePath();

    QFuture<GenerateCompilationDbResult> task
            = Utils::runAsync(&Internal::generateCompilationDB, projectInfo,
                              baseDir, CompilationDbPurpose::Project,
                              warningsConfigForProject(target->project()),
                              globalClangOptions(),
                              FilePath());
    Core::ProgressManager::addTask(task, tr("Generating Compilation DB"), "generate compilation db");
    m_generatorWatcher.setFuture(task);
}

static bool isDBGenerationEnabled(ProjectExplorer::Project *project)
{
    using namespace CppEditor;
    if (!project)
        return false;
    const ProjectInfo::ConstPtr projectInfo = CppModelManager::instance()->projectInfo(project);
    return projectInfo && !projectInfo->projectParts().isEmpty();
}

ClangCodeModelPlugin::~ClangCodeModelPlugin()
{
    m_generatorWatcher.waitForFinished();
}

bool ClangCodeModelPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    ProjectExplorer::TaskHub::addCategory(Constants::TASK_CATEGORY_DIAGNOSTICS,
                                          tr("Clang Code Model"));

    connect(ProjectExplorer::ProjectExplorerPlugin::instance(),
            &ProjectExplorer::ProjectExplorerPlugin::finishedInitialization,
            this,
            &ClangCodeModelPlugin::maybeHandleBatchFileAndExit);

    CppEditor::CppModelManager::instance()->activateClangCodeModel(&m_modelManagerSupportProvider);

    createCompilationDBButton();

    return true;
}

void ClangCodeModelPlugin::createCompilationDBButton()
{
    Core::ActionContainer *mbuild =
            Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_BUILDPROJECT);
    // generate compile_commands.json
    m_generateCompilationDBAction = new ParameterAction(
                tr("Generate Compilation Database"),
                tr("Generate Compilation Database for \"%1\""),
                ParameterAction::AlwaysEnabled, this);

    ProjectExplorer::Project *startupProject = ProjectExplorer::SessionManager::startupProject();
    m_generateCompilationDBAction->setEnabled(isDBGenerationEnabled(startupProject));
    if (startupProject)
        m_generateCompilationDBAction->setParameter(startupProject->displayName());

    Core::Command *command = Core::ActionManager::registerAction(m_generateCompilationDBAction,
                                                                 Constants::GENERATE_COMPILATION_DB);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDescription(m_generateCompilationDBAction->text());
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);

    connect(&m_generatorWatcher, &QFutureWatcher<GenerateCompilationDbResult>::finished,
            this, [this] () {
        const GenerateCompilationDbResult result = m_generatorWatcher.result();
        QString message;
        if (result.error.isEmpty()) {
            message = tr("Clang compilation database generated at \"%1\".")
                    .arg(QDir::toNativeSeparators(result.filePath));
        } else {
            message = tr("Generating Clang compilation database failed: %1").arg(result.error);
        }
        Core::MessageManager::writeFlashing(message);
        m_generateCompilationDBAction->setEnabled(
                    isDBGenerationEnabled(ProjectExplorer::SessionManager::startupProject()));
    });
    connect(m_generateCompilationDBAction, &QAction::triggered, this, [this] {
        if (!m_generateCompilationDBAction->isEnabled())
            return;

        m_generateCompilationDBAction->setEnabled(false);
        generateCompilationDB();
    });
    connect(CppEditor::CppModelManager::instance(), &CppEditor::CppModelManager::projectPartsUpdated,
            this, [this](ProjectExplorer::Project *project) {
        if (project != ProjectExplorer::SessionManager::startupProject())
            return;
        m_generateCompilationDBAction->setParameter(project->displayName());
        if (!m_generatorWatcher.isRunning())
            m_generateCompilationDBAction->setEnabled(isDBGenerationEnabled(project));
    });
    connect(ProjectExplorer::SessionManager::instance(),
            &ProjectExplorer::SessionManager::startupProjectChanged,
            this,
            [this](ProjectExplorer::Project *project) {
        m_generateCompilationDBAction->setParameter(project ? project->displayName() : "");
        if (!m_generatorWatcher.isRunning())
            m_generateCompilationDBAction->setEnabled(isDBGenerationEnabled(project));
    });
    connect(ProjectExplorer::SessionManager::instance(),
            &ProjectExplorer::SessionManager::projectDisplayNameChanged,
            this,
            [this](ProjectExplorer::Project *project) {
        if (project != ProjectExplorer::SessionManager::startupProject())
            return;
        m_generateCompilationDBAction->setParameter(project->displayName());
    });
}

// For e.g. creation of profile-guided optimization builds.
void ClangCodeModelPlugin::maybeHandleBatchFileAndExit() const
{
#ifdef WITH_TESTS
    const QString batchFilePath = QString::fromLocal8Bit(qgetenv("QTC_CLANG_BATCH"));
    if (!batchFilePath.isEmpty() && QTC_GUARD(QFileInfo::exists(batchFilePath))) {
        const bool runSucceeded = runClangBatchFile(batchFilePath);
        QCoreApplication::exit(!runSucceeded);
    }
#endif
}

#ifdef WITH_TESTS
QVector<QObject *> ClangCodeModelPlugin::createTestObjects() const
{
    return {
        new Tests::ActivationSequenceProcessorTest,
        new Tests::ClangdTestCompletion,
        new Tests::ClangdTestExternalChanges,
        new Tests::ClangdTestFindReferences,
        new Tests::ClangdTestFollowSymbol,
        new Tests::ClangdTestHighlighting,
        new Tests::ClangdTestLocalReferences,
        new Tests::ClangdTestTooltips,
        new Tests::ClangFixItTest,
    };
}
#endif

} // namespace Internal
} // namespace Clang
