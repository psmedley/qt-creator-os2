/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Design Tooling
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

#include <QToolBar>
#include <QWidget>
#include <QLabel>

namespace QmlDesigner {

class CurveEditorModel;
class CurveEditorToolBar;
class GraphicsView;
class TreeView;

class CurveEditor : public QWidget
{
    Q_OBJECT

public:
    CurveEditor(CurveEditorModel *model, QWidget *parent = nullptr);

    bool dragging() const;

    void zoomX(double zoom);

    void zoomY(double zoom);

    void clearCanvas();

signals:
    void viewEnabledChanged(const bool);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateStatusLine();

    QLabel *m_infoText;

    QLabel *m_statusLine;

    CurveEditorToolBar *m_toolbar;

    TreeView *m_tree;

    GraphicsView *m_view;
};

} // End namespace QmlDesigner.
