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

#include "showoutputtaskhandler.h"

#include "task.h"

#include <coreplugin/ioutputpane.h>
#include <coreplugin/outputwindow.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QAction>

namespace ProjectExplorer {
namespace Internal {

ShowOutputTaskHandler::ShowOutputTaskHandler(Core::IOutputPane *window, const QString &text,
                                             const QString &tooltip, const QString &shortcut)
    : m_window(window), m_text(text), m_tooltip(tooltip), m_shortcut(shortcut)
{
    QTC_CHECK(m_window);
    QTC_CHECK(!m_text.isEmpty());
}

bool ShowOutputTaskHandler::canHandle(const Task &task) const
{
    return Utils::anyOf(m_window->outputWindows(), [task](const Core::OutputWindow *ow) {
        return ow->knowsPositionOf(task.taskId);
    });
}

void ShowOutputTaskHandler::handle(const Task &task)
{
    Q_ASSERT(canHandle(task));
    // popup first as this does move the visible area!
    m_window->popup(Core::IOutputPane::Flags(Core::IOutputPane::ModeSwitch | Core::IOutputPane::WithFocus));
    for (Core::OutputWindow * const ow : m_window->outputWindows()) {
        if (ow->knowsPositionOf(task.taskId)) {
            m_window->ensureWindowVisible(ow);
            ow->showPositionOf(task.taskId);
            break;
        }
    }
}

QAction *ShowOutputTaskHandler::createAction(QObject *parent) const
{
    QAction * const outputAction = new QAction(m_text, parent);
    if (!m_tooltip.isEmpty())
        outputAction->setToolTip(m_tooltip);
    if (!m_shortcut.isEmpty())
        outputAction->setShortcut(QKeySequence(m_shortcut));
    outputAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    return outputAction;
}

} // namespace Internal
} // namespace ProjectExplorer
