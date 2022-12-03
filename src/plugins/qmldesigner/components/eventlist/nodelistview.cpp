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
#include "nodelistview.h"
#include "eventlist.h"

#include "bindingproperty.h"
#include "nodemetainfo.h"
#include "utils/qtcassert.h"
#include "variantproperty.h"

#include <QStandardItemModel>

#include <theme.h>
#include <utils/stylehelper.h>

namespace QmlDesigner {

QStringList eventIdsFromVariant(const QVariant &val)
{
    QStringList eventIds = val.toString().split(",", Qt::SkipEmptyParts);
    for (QString &str : eventIds)
        str = str.trimmed();
    return eventIds;
}

QVariant eventIdsToVariant(const QStringList &eventIds)
{
    if (eventIds.empty())
        return QVariant();

    return QVariant(eventIds.join(", "));
}

NodeListModel::NodeListModel(QObject *parent)
    : QStandardItemModel(0, 4, parent)
{
    setHeaderData(idColumn, Qt::Horizontal, tr("ID"));
    setHeaderData(typeColumn, Qt::Horizontal, tr("Type"));
    setHeaderData(fromColumn, Qt::Horizontal, tr("From"));
    setHeaderData(toColumn, Qt::Horizontal, tr("To"));
    setSortRole(internalIdRole);
}


NodeListView::NodeListView(AbstractView *parent)
    : AbstractView(parent)
    , m_itemModel(new NodeListModel(this))
{
    parent->model()->attachView(this);
    reset();
}

NodeListView::~NodeListView() {}

QStandardItemModel *NodeListView::itemModel() const
{
    return m_itemModel;
}

int NodeListView::currentNode() const
{
    ModelNode node = firstSelectedModelNode();
    if (node.isValid())
        return node.internalId();

    return -1;
}

void NodeListView::selectNode(int internalId)
{
    ModelNode node = modelNodeForInternalId(internalId);
    if (node.isValid()) {
        clearSelectedModelNodes();
        selectModelNode(node);
    }
}


QModelIndex indexByInternalId(QAbstractItemModel *model, int id)
{
    constexpr int count = 1;
    auto ids = model->match(model->index(0, NodeListModel::idColumn),
                            NodeListModel::internalIdRole,
                            id,
                            count,
                            Qt::MatchExactly);
    if (ids.size() == count)
        return ids.first();

    return QModelIndex();
}

bool NodeListView::addEventId(int nodeId, const QString &event)
{
    if (auto node = compatibleModelNode(nodeId); node.isValid()) {
        QStringList events;
        if (node.hasProperty("eventIds"))
            events = eventIdsFromVariant(node.variantProperty("eventIds").value());
        events.push_back(event);

        return setEventIds(node, events);
    }
    return false;
}

bool NodeListView::removeEventIds(int nodeId, const QStringList &eventIds)
{
    if (auto node = compatibleModelNode(nodeId); node.isValid()) {
        QStringList events = eventIdsFromVariant(node.variantProperty("eventIds").value());
        for (auto &&remove : eventIds)
            Q_UNUSED(events.removeOne(remove));

        return setEventIds(node, events);
    }
    return false;
}

QString NodeListView::setNodeId(int internalId, const QString &id)
{
    ModelNode node = modelNodeForInternalId(internalId);
    if (node.isValid()) {
        QString newId = model()->generateNewId(id);
        node.setIdWithRefactoring(newId);
        return newId;
    }

    return QString();
}

bool supported(const ModelNode &node)
{
    if (!node.isValid())
        return false;

    const NodeMetaInfo metaInfo = node.metaInfo();
    if (!metaInfo.isValid())
        return false;

    return metaInfo.hasProperty("eventIds");
}

static QIcon flowTypeIconFromFont(const TypeName &type)
{
    QString unicode = Theme::getIconUnicode(Theme::Icon::edit);
    const QString fontName = "qtds_propertyIconFont.ttf";
    if (type == "FlowView.FlowTransition")
        unicode = Theme::getIconUnicode(Theme::Icon::flowTransition);
    else if (type == "FlowView.FlowActionArea")
        unicode = Theme::getIconUnicode(Theme::Icon::flowAction);
    else if (type == "FlowView.FlowWildcard")
        unicode = Theme::getIconUnicode(Theme::Icon::wildcard);

    return Utils::StyleHelper::getIconFromIconFont(fontName, unicode, 28, 28);
}

void NodeListView::reset()
{
    auto setData = [this](int row, int column, const QVariant &data, int role = Qt::EditRole) {
        QModelIndex idx = m_itemModel->index(row, column, QModelIndex());
        m_itemModel->setData(idx, data, role);
    };

    m_itemModel->removeRows(0, m_itemModel->rowCount(QModelIndex()), QModelIndex());

    for (auto &&node : allModelNodes()) {
        if (supported(node)) {
            int row = m_itemModel->rowCount();
            if (m_itemModel->insertRows(row, 1, QModelIndex())) {
                int iid = node.internalId();
                auto eventIds = eventIdsFromVariant(node.variantProperty("eventIds").value());

                setData(row, NodeListModel::idColumn, node.id());
                setData(row,
                        NodeListModel::idColumn,
                        flowTypeIconFromFont(node.type()),
                        Qt::DecorationRole);
                setData(row, NodeListModel::idColumn, iid, NodeListModel::internalIdRole);
                setData(row, NodeListModel::idColumn, eventIds, NodeListModel::eventIdsRole);

                setData(row, NodeListModel::typeColumn, node.type());
                setData(row, NodeListModel::fromColumn, node.bindingProperty("from").expression());
                setData(row, NodeListModel::toColumn, node.bindingProperty("to").expression());
            }
        }
    }
    m_itemModel->sort(0);
}

ModelNode NodeListView::compatibleModelNode(int nodeId)
{
    if (auto node = modelNodeForInternalId(nodeId); node.isValid()) {
        QTC_ASSERT(node.metaInfo().isValid(), return ModelNode());
        QTC_ASSERT(node.metaInfo().hasProperty("eventIds"), return ModelNode());
        return node;
    }
    return ModelNode();
}

bool setEventIdsInModelNode(AbstractView *view, const ModelNode &node, const QStringList &events)
{
    if (events.empty()) {
        if (node.hasProperty("eventIds")) {
            return view->executeInTransaction("NodeListView::setEventIds",
                                              [=]() { node.removeProperty("eventIds"); });
        }
    } else {
        QStringList copy(events);
        Q_UNUSED(copy.removeDuplicates());
        QString value = events.join(", ");
        return view->executeInTransaction("NodeListView::setEventIds", [=]() {
            node.variantProperty("eventIds").setValue(value);
        });
    }
    return false;
}

bool NodeListView::setEventIds(const ModelNode &node, const QStringList &events)
{
    bool result = setEventIdsInModelNode(this, node, events);
    auto modelIndex = indexByInternalId(m_itemModel, node.internalId());
    if (modelIndex.isValid() && result) {
        m_itemModel->setData(modelIndex, events, NodeListModel::eventIdsRole);
        return true;
    }
    return false;
}

} // namespace QmlDesigner.
