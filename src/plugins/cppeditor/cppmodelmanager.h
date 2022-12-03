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

#pragma once

#include "cppeditor_global.h"

#include "cursorineditor.h"
#include "projectinfo.h"
#include "projectpart.h"

#include <cplusplus/cppmodelmanagerbase.h>
#include <coreplugin/find/ifindfilter.h>
#include <coreplugin/locator/ilocatorfilter.h>
#include <projectexplorer/headerpath.h>
#include <utils/link.h>

#include <QFuture>
#include <QObject>
#include <QStringList>

namespace Core {
class IDocument;
class IEditor;
}
namespace CPlusPlus {
class AST;
class CallAST;
class LookupContext;
} // namespace CPlusPlus
namespace ProjectExplorer { class Project; }
namespace TextEditor {
class BaseHoverHandler;
class TextDocument;
} // namespace TextEditor

namespace CppEditor {

class AbstractEditorSupport;
class AbstractOverviewModel;
class BaseEditorDocumentProcessor;
class CppCompletionAssistProvider;
class CppEditorDocumentHandle;
class CppIndexingSupport;
class CppLocatorData;
class FollowSymbolUnderCursor;
class ModelManagerSupportProvider;
class ModelManagerSupport;
class SymbolFinder;
class WorkingCopy;

namespace Internal {
class CppSourceProcessor;
class CppEditorPluginPrivate;
class CppModelManagerPrivate;
}

namespace Tests { class ModelManagerTestHelper; }

enum class SignalSlotType {
    OldStyleSignal,
    NewStyleSignal,
    None
};

class CPPEDITOR_EXPORT CppModelManager final : public CPlusPlus::CppModelManagerBase
{
    Q_OBJECT

private:
    friend class Internal::CppEditorPluginPrivate;
    CppModelManager();
    ~CppModelManager() override;

public:
    using Document = CPlusPlus::Document;

    static CppModelManager *instance();

    void registerJsExtension();

     // Documented in source file.
     enum ProgressNotificationMode {
        ForcedProgressNotification,
        ReservedProgressNotification
    };

    QFuture<void> updateSourceFiles(const QSet<QString> &sourceFiles,
                                    ProgressNotificationMode mode = ReservedProgressNotification);
    void updateCppEditorDocuments(bool projectsUpdated = false) const;
    WorkingCopy workingCopy() const;
    QByteArray codeModelConfiguration() const;
    CppLocatorData *locatorData() const;

    bool setExtraDiagnostics(const QString &fileName,
                             const QString &kind,
                             const QList<Document::DiagnosticMessage> &diagnostics) override;

    const QList<Document::DiagnosticMessage> diagnosticMessages();

    QList<ProjectInfo::ConstPtr> projectInfos() const;
    ProjectInfo::ConstPtr projectInfo(ProjectExplorer::Project *project) const;
    QFuture<void> updateProjectInfo(const ProjectInfo::ConstPtr &newProjectInfo,
                                    const QSet<QString> &additionalFiles = {});

    /// \return The project part with the given project file
    ProjectPart::ConstPtr projectPartForId(const QString &projectPartId) const;
    /// \return All project parts that mention the given file name as one of the sources/headers.
    QList<ProjectPart::ConstPtr> projectPart(const Utils::FilePath &fileName) const;
    QList<ProjectPart::ConstPtr> projectPart(const QString &fileName) const
    { return projectPart(Utils::FilePath::fromString(fileName)); }
    /// This is a fall-back function: find all files that includes the file directly or indirectly,
    /// and return its \c ProjectPart list for use with this file.
    QList<ProjectPart::ConstPtr> projectPartFromDependencies(const Utils::FilePath &fileName) const;
    /// \return A synthetic \c ProjectPart which consists of all defines/includes/frameworks from
    ///         all loaded projects.
    ProjectPart::ConstPtr fallbackProjectPart();

    CPlusPlus::Snapshot snapshot() const override;
    Document::Ptr document(const QString &fileName) const;
    bool replaceDocument(Document::Ptr newDoc);

    void emitDocumentUpdated(Document::Ptr doc);
    void emitAbstractEditorSupportContentsUpdated(const QString &filePath,
                                                  const QString &sourcePath,
                                                  const QByteArray &contents);
    void emitAbstractEditorSupportRemoved(const QString &filePath);

    static bool isCppEditor(Core::IEditor *editor);
    static bool usesClangd(const TextEditor::TextDocument *document);
    bool isClangCodeModelActive() const;

    QSet<AbstractEditorSupport*> abstractEditorSupports() const;
    void addExtraEditorSupport(AbstractEditorSupport *editorSupport);
    void removeExtraEditorSupport(AbstractEditorSupport *editorSupport);

    const QList<CppEditorDocumentHandle *> cppEditorDocuments() const;
    CppEditorDocumentHandle *cppEditorDocument(const QString &filePath) const;
    static BaseEditorDocumentProcessor *cppEditorDocumentProcessor(const QString &filePath);
    void registerCppEditorDocument(CppEditorDocumentHandle *cppEditorDocument);
    void unregisterCppEditorDocument(const QString &filePath);

    QList<int> references(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context);

    SignalSlotType getSignalSlotType(const QString &filePath,
                                     const QByteArray &content,
                                     int position) const;

    void renameUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context,
                      const QString &replacement = QString());
    void findUsages(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context);

    void findMacroUsages(const CPlusPlus::Macro &macro);
    void renameMacroUsages(const CPlusPlus::Macro &macro, const QString &replacement);

    void finishedRefreshingSourceFiles(const QSet<QString> &files);

    void activateClangCodeModel(ModelManagerSupportProvider *modelManagerSupportProvider);
    CppCompletionAssistProvider *completionAssistProvider() const;
    BaseEditorDocumentProcessor *createEditorDocumentProcessor(
                    TextEditor::TextDocument *baseTextDocument) const;
    TextEditor::BaseHoverHandler *createHoverHandler() const;
    static FollowSymbolUnderCursor &builtinFollowSymbol();

    enum class Backend { Builtin, Best };
    static void followSymbol(const CursorInEditor &data,
                             const Utils::LinkHandler &processLinkCallback,
                             bool resolveTarget, bool inNextSplit, Backend backend = Backend::Best);
    static void switchDeclDef(const CursorInEditor &data,
                              const Utils::LinkHandler &processLinkCallback,
                              Backend backend = Backend::Best);
    static void startLocalRenaming(const CursorInEditor &data, const ProjectPart *projectPart,
                                   RenameCallback &&renameSymbolsCallback,
                                   Backend backend = Backend::Best);
    static void globalRename(const CursorInEditor &data, const QString &replacement,
                             Backend backend = Backend::Best);
    static void findUsages(const CursorInEditor &data, Backend backend = Backend::Best);
    static void switchHeaderSource(bool inNextSplit, Backend backend = Backend::Best);

    static Core::ILocatorFilter *createAuxiliaryCurrentDocumentFilter();

    std::unique_ptr<AbstractOverviewModel> createOverviewModel() const;

    CppIndexingSupport *indexingSupport();

    QStringList projectFiles();

    ProjectExplorer::HeaderPaths headerPaths();

    // Use this *only* for auto tests
    void setHeaderPaths(const ProjectExplorer::HeaderPaths &headerPaths);

    ProjectExplorer::Macros definedMacros();

    void enableGarbageCollector(bool enable);

    SymbolFinder *symbolFinder();

    QThreadPool *sharedThreadPool();

    static QSet<QString> timeStampModifiedFiles(const QList<Document::Ptr> &documentsToCheck);

    static Internal::CppSourceProcessor *createSourceProcessor();
    static QString configurationFileName();
    static QString editorConfigurationFileName();

    void setLocatorFilter(std::unique_ptr<Core::ILocatorFilter> &&filter);
    void setClassesFilter(std::unique_ptr<Core::ILocatorFilter> &&filter);
    void setIncludesFilter(std::unique_ptr<Core::ILocatorFilter> &&filter);
    void setFunctionsFilter(std::unique_ptr<Core::ILocatorFilter> &&filter);
    void setSymbolsFindFilter(std::unique_ptr<Core::IFindFilter> &&filter);
    void setCurrentDocumentFilter(std::unique_ptr<Core::ILocatorFilter> &&filter);

    Core::ILocatorFilter *locatorFilter() const;
    Core::ILocatorFilter *classesFilter() const;
    Core::ILocatorFilter *includesFilter() const;
    Core::ILocatorFilter *functionsFilter() const;
    Core::IFindFilter *symbolsFindFilter() const;
    Core::ILocatorFilter *currentDocumentFilter() const;

    /*
     * try to find build system target that depends on the given file - if the file is no header
     * try to find the corresponding header and use this instead to find the respective target
     */
    QSet<QString> dependingInternalTargets(const Utils::FilePath &file) const;

    QSet<QString> internalTargets(const Utils::FilePath &filePath) const;

    void renameIncludes(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath);

    // for VcsBaseSubmitEditor
    Q_INVOKABLE QSet<QString> symbolsInFiles(const QSet<Utils::FilePath> &files) const;

signals:
    /// Project data might be locked while this is emitted.
    void aboutToRemoveFiles(const QStringList &files);

    void documentUpdated(CPlusPlus::Document::Ptr doc);
    void sourceFilesRefreshed(const QSet<QString> &files);

    void projectPartsUpdated(ProjectExplorer::Project *project);
    void projectPartsRemoved(const QStringList &projectPartIds);

    void globalSnapshotChanged();

    void gcFinished(); // Needed for tests.

    void abstractEditorSupportContentsUpdated(const QString &filePath,
                                              const QString &sourcePath,
                                              const QByteArray &contents);
    void abstractEditorSupportRemoved(const QString &filePath);
    void fallbackProjectPartUpdated();

    void diagnosticsChanged(const QString &fileName, const QString &kind);

public slots:
    void updateModifiedSourceFiles();
    void GC();

private:
    // This should be executed in the GUI thread.
    friend class Tests::ModelManagerTestHelper;
    void onAboutToLoadSession();
    void onProjectAdded(ProjectExplorer::Project *project);
    void onAboutToRemoveProject(ProjectExplorer::Project *project);
    void onActiveProjectChanged(ProjectExplorer::Project *project);
    void onSourceFilesRefreshed() const;
    void onCurrentEditorChanged(Core::IEditor *editor);
    void onCoreAboutToClose();
    void setupFallbackProjectPart();

    void initializeBuiltinModelManagerSupport();
    void delayedGC();
    void recalculateProjectPartMappings();

    void replaceSnapshot(const CPlusPlus::Snapshot &newSnapshot);
    void removeFilesFromSnapshot(const QSet<QString> &removedFiles);
    void removeProjectInfoFilesAndIncludesFromSnapshot(const ProjectInfo &projectInfo);

    WorkingCopy buildWorkingCopyList();

    ModelManagerSupport *modelManagerSupport(Backend backend) const;

    void ensureUpdated();
    QStringList internalProjectFiles() const;
    ProjectExplorer::HeaderPaths internalHeaderPaths() const;
    ProjectExplorer::Macros internalDefinedMacros() const;

    void dumpModelManagerConfiguration(const QString &logFileId);
    void initCppTools();

private:
    Internal::CppModelManagerPrivate *d;
};

} // namespace CppEditor
