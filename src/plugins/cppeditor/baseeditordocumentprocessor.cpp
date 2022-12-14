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

#include "baseeditordocumentprocessor.h"

#include "cppcodemodelsettings.h"
#include "cppmodelmanager.h"
#include "cpptoolsreuse.h"
#include "editordocumenthandle.h"

#include <projectexplorer/session.h>
#include <texteditor/quickfix.h>

namespace CppEditor {

/*!
    \class CppEditor::BaseEditorDocumentProcessor

    \brief The BaseEditorDocumentProcessor class controls and executes all
           document relevant actions (reparsing, semantic highlighting, additional
           semantic calculations) after a text document has changed.
*/

BaseEditorDocumentProcessor::BaseEditorDocumentProcessor(QTextDocument *textDocument,
                                                         const QString &filePath)
    : m_filePath(filePath),
      m_textDocument(textDocument)
{
}

BaseEditorDocumentProcessor::~BaseEditorDocumentProcessor() = default;

void BaseEditorDocumentProcessor::run(bool projectsUpdated)
{
    const Utils::Language languagePreference = codeModelSettings()->interpretAmbigiousHeadersAsCHeaders()
            ? Utils::Language::C
            : Utils::Language::Cxx;

    runImpl({CppModelManager::instance()->workingCopy(),
             ProjectExplorer::SessionManager::startupProject(),
             languagePreference,
             projectsUpdated});
}

TextEditor::QuickFixOperations
BaseEditorDocumentProcessor::extraRefactoringOperations(const TextEditor::AssistInterface &)
{
    return TextEditor::QuickFixOperations();
}

void BaseEditorDocumentProcessor::invalidateDiagnostics()
{
}

void BaseEditorDocumentProcessor::setParserConfig(
        const BaseEditorDocumentParser::Configuration &config)
{
    parser()->setConfiguration(config);
}

void BaseEditorDocumentProcessor::runParser(QFutureInterface<void> &future,
                                            BaseEditorDocumentParser::Ptr parser,
                                            BaseEditorDocumentParser::UpdateParams updateParams)
{
    future.setProgressRange(0, 1);
    if (future.isCanceled()) {
        future.setProgressValue(1);
        return;
    }

    parser->update(future, updateParams);
    CppModelManager::instance()->finishedRefreshingSourceFiles({parser->filePath()});

    future.setProgressValue(1);
}

} // namespace CppEditor
