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

#include "cppmodelmanager.h"
#include "cppworkingcopy.h"

#include <cplusplus/CppDocument.h>

#include <texteditor/refactoringchanges.h>

namespace CppEditor {

class CppRefactoringChanges;
class CppRefactoringFile;
class CppRefactoringChangesData;
using CppRefactoringFilePtr = QSharedPointer<CppRefactoringFile>;
using CppRefactoringFileConstPtr = QSharedPointer<const CppRefactoringFile>;

class CPPEDITOR_EXPORT CppRefactoringFile: public TextEditor::RefactoringFile
{
public:
    CPlusPlus::Document::Ptr cppDocument() const;
    void setCppDocument(CPlusPlus::Document::Ptr document);

    CPlusPlus::Scope *scopeAt(unsigned index) const;

    bool isCursorOn(unsigned tokenIndex) const;
    bool isCursorOn(const CPlusPlus::AST *ast) const;

    Range range(int start, int end) const;
    Range range(unsigned tokenIndex) const;
    Range range(const CPlusPlus::AST *ast) const;

    const CPlusPlus::Token &tokenAt(unsigned index) const;

    int startOf(unsigned index) const;
    int startOf(const CPlusPlus::AST *ast) const;
    int endOf(unsigned index) const;
    int endOf(const CPlusPlus::AST *ast) const;

    void startAndEndOf(unsigned index, int *start, int *end) const;

    using TextEditor::RefactoringFile::textOf;
    QString textOf(const CPlusPlus::AST *ast) const;

protected:
    CppRefactoringFile(const Utils::FilePath &filePath, const QSharedPointer<TextEditor::RefactoringChangesData> &data);
    CppRefactoringFile(QTextDocument *document, const Utils::FilePath &filePath);
    explicit CppRefactoringFile(TextEditor::TextEditorWidget *editor);

    CppRefactoringChangesData *data() const;
    void fileChanged() override;

    mutable CPlusPlus::Document::Ptr m_cppDocument;

    friend class CppRefactoringChanges; // for access to constructor
};

class CPPEDITOR_EXPORT CppRefactoringChangesData : public TextEditor::RefactoringChangesData
{
public:
    explicit CppRefactoringChangesData(const CPlusPlus::Snapshot &snapshot);

    void indentSelection(const QTextCursor &selection,
                         const Utils::FilePath &filePath,
                         const TextEditor::TextDocument *textDocument) const override;

    void reindentSelection(const QTextCursor &selection,
                           const Utils::FilePath &filePath,
                           const TextEditor::TextDocument *textDocument) const override;

    void fileChanged(const Utils::FilePath &filePath) override;

    CPlusPlus::Snapshot m_snapshot;
    CppModelManager *m_modelManager;
    WorkingCopy m_workingCopy;
};

class CPPEDITOR_EXPORT CppRefactoringChanges: public TextEditor::RefactoringChanges
{
public:
    explicit CppRefactoringChanges(const CPlusPlus::Snapshot &snapshot);

    static CppRefactoringFilePtr file(TextEditor::TextEditorWidget *editor,
                                      const CPlusPlus::Document::Ptr &document);
    CppRefactoringFilePtr file(const Utils::FilePath &filePath) const;
    // safe to use from non-gui threads
    CppRefactoringFileConstPtr fileNoEditor(const Utils::FilePath &filePath) const;

    const CPlusPlus::Snapshot &snapshot() const;

private:
    CppRefactoringChangesData *data() const;
};

} // namespace CppEditor
