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

#include <abstractview.h>
#include <QStandardItemModel>

namespace QmlDesigner {

class NodeListModel : public QStandardItemModel
{
    Q_OBJECT

public:
    enum Columns : unsigned int {
        idColumn = 0,
        typeColumn = 1,
        fromColumn = 2,
        toColumn = 3
    };

    enum Roles : unsigned int {
        internalIdRole = Qt::UserRole + 1,
        eventIdsRole = Qt::UserRole + 2
    };

    NodeListModel(QObject *parent = nullptr);
};


class NodeListView : public AbstractView
{
    Q_OBJECT

public:
    explicit NodeListView(AbstractView *parent);
    ~NodeListView() override;

    int currentNode() const;
    QStandardItemModel *itemModel() const;

    void reset();
    void selectNode(int internalId);
    bool addEventId(int nodeId, const QString &event);
    bool removeEventIds(int nodeId, const QStringList &events);
    QString setNodeId(int internalId, const QString &id);

private:
    ModelNode compatibleModelNode(int nodeId);
    bool setEventIds(const ModelNode &node, const QStringList &events);

    QStandardItemModel *m_itemModel;
};

} // namespace QmlDesigner.
