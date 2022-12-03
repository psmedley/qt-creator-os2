/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "edit3dactions.h"
#include "edit3dview.h"
#include "edit3dwidget.h"

#include <viewmanager.h>
#include <nodeinstanceview.h>
#include <qmldesignerplugin.h>
#include <nodemetainfo.h>

#include <utils/algorithm.h>

#include <QDebug>

namespace QmlDesigner {

Edit3DActionTemplate::Edit3DActionTemplate(const QString &description,
                                           SelectionContextOperation action,
                                           View3DActionCommand::Type type)
    : DefaultAction(description),
      m_action(action),
      m_type(type)
{ }

void Edit3DActionTemplate::actionTriggered(bool b)
{
    if (m_type != View3DActionCommand::Empty && m_type != View3DActionCommand::SelectBackgroundColor
        && m_type != View3DActionCommand::SelectGridColor) {
        auto view = QmlDesignerPlugin::instance()->viewManager().nodeInstanceView();
        View3DActionCommand cmd(m_type, b);
        view->view3DAction(cmd);
    }

    if (m_action)
        m_action(m_selectionContext);
}

Edit3DAction::Edit3DAction(const QByteArray &menuId, View3DActionCommand::Type type,
                           const QString &description, const QKeySequence &key, bool checkable,
                           bool checked, const QIcon &iconOff, const QIcon &iconOn,
                           SelectionContextOperation selectionAction, const QString &toolTip)
    : AbstractAction(new Edit3DActionTemplate(description, selectionAction, type))
    , m_menuId(menuId)
{
    action()->setShortcut(key);
    action()->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    action()->setCheckable(checkable);
    action()->setChecked(checked);

    // Description will be used as tooltip by default if no explicit tooltip is provided
    if (!toolTip.isEmpty())
        action()->setToolTip(toolTip);

    if (checkable) {
        QIcon onOffIcon;
        const auto onAvail = iconOn.availableSizes(); // Assume both icons have same sizes available
        for (const auto &size : onAvail) {
            onOffIcon.addPixmap(iconOn.pixmap(size), QIcon::Normal, QIcon::On);
            onOffIcon.addPixmap(iconOff.pixmap(size), QIcon::Normal, QIcon::Off);
        }
        action()->setIcon(onOffIcon);
    } else {
        action()->setIcon(iconOff);
    }
}

QByteArray Edit3DAction::category() const
{
    return QByteArray();
}

bool Edit3DAction::isVisible(const SelectionContext &selectionContext) const
{
    Q_UNUSED(selectionContext)
    return true;
}

bool Edit3DAction::isEnabled(const SelectionContext &selectionContext) const
{
    return isVisible(selectionContext);
}

Edit3DCameraAction::Edit3DCameraAction(const QByteArray &menuId, View3DActionCommand::Type type,
                                       const QString &description, const QKeySequence &key,
                                       bool checkable, bool checked, const QIcon &iconOff,
                                       const QIcon &iconOn,
                                       SelectionContextOperation selectionAction)
    : Edit3DAction(menuId, type, description, key, checkable, checked, iconOff, iconOn, selectionAction)
{
}

bool Edit3DCameraAction::isEnabled(const SelectionContext &selectionContext) const
{
    return Utils::anyOf(selectionContext.selectedModelNodes(), [](const ModelNode &node) {
        return node.isValid() && node.metaInfo().isSubclassOf("QQuick3D.Camera");
    });
}

}

