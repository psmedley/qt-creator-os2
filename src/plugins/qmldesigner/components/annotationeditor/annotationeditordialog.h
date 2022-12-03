/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "annotation.h"

#include <QDialog>


QT_BEGIN_NAMESPACE
class QAbstractButton;
class QDialogButtonBox;
QT_END_NAMESPACE

namespace QmlDesigner {

class DefaultAnnotationsModel;
class AnnotationEditorWidget;

class AnnotationEditorDialog : public QDialog
{
    Q_OBJECT
public:
    enum class ViewMode { TableView,
                          TabsView };

    explicit AnnotationEditorDialog(QWidget *parent,
                                    const QString &targetId = {},
                                    const QString &customId = {});
    ~AnnotationEditorDialog();

    const Annotation &annotation() const;
    void setAnnotation(const Annotation &annotation);

    const QString &customId() const;
    void setCustomId(const QString &customId);

    DefaultAnnotationsModel *defaultAnnotations() const;
    void loadDefaultAnnotations(const QString &filename);

private slots:
    void buttonClicked(QAbstractButton *button);

    void acceptedClicked();
    void appliedClicked();

signals:
    void acceptedDialog(); //use instead of QDialog::accepted
    void appliedDialog();

private:
    void updateAnnotation();

private:
    GlobalAnnotationStatus m_globalStatus = GlobalAnnotationStatus::NoStatus;
    Annotation m_annotation;
    QString m_customId;
    std::unique_ptr<DefaultAnnotationsModel> m_defaults;
    AnnotationEditorWidget *m_editorWidget;

    QDialogButtonBox *m_buttonBox;
};


} //namespace QmlDesigner
