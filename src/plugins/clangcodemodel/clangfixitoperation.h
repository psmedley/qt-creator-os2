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

#include "clangutils.h"

#include <texteditor/quickfix.h>

#include <utils/changeset.h>

#include <QVector>
#include <QSharedPointer>

namespace TextEditor {
class RefactoringChanges;
class RefactoringFile;
}

namespace ClangCodeModel {
namespace Internal {

class ClangFixItOperation : public TextEditor::QuickFixOperation
{
public:
    ClangFixItOperation(const QString &fixItText, const QList<ClangFixIt> &fixIts);

    int priority() const override;
    QString description() const override;
    void perform() override;

    QString firstRefactoringFileContent_forTestOnly() const;

private:
    void applyFixitsToFile(TextEditor::RefactoringFile &refactoringFile,
                           const QList<ClangFixIt> fixIts);
    Utils::ChangeSet toChangeSet(TextEditor::RefactoringFile &refactoringFile,
            const QList<ClangFixIt> fixIts) const;

private:
    QString fixItText;
    QVector<QSharedPointer<TextEditor::RefactoringFile>> refactoringFiles;
    QList<ClangFixIt> fixIts;
};

} // namespace Internal
} // namespace ClangCodeModel
