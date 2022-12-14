/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include <qmldesignercorelib_global.h>
#include <modelnode.h>
#include "qmlobjectnode.h"
#include "qmlstate.h"
#include "qmlvisualnode.h"

#include <QStringList>
#include <QRectF>
#include <QTransform>

namespace QmlDesigner {

class QmlModelStateGroup;
class QmlAnchors;
class ItemLibraryEntry;

class QMLDESIGNERCORE_EXPORT Qml3DNode : public QmlVisualNode
{
    friend class QmlAnchors;
public:
    Qml3DNode() : QmlVisualNode() {}
    Qml3DNode(const ModelNode &modelNode)  : QmlVisualNode(modelNode) {}
    bool isValid() const override;
    static bool isValidQml3DNode(const ModelNode &modelNode);
    static bool isValidVisualRoot(const ModelNode &modelNode);

    // From QmlObjectNode
    void setVariantProperty(const PropertyName &name, const QVariant &value) override;
    void setBindingProperty(const PropertyName &name, const QString &expression) override;
    bool isBlocked(const PropertyName &propName) const override;

    friend auto qHash(const Qml3DNode &node) { return qHash(node.modelNode()); }

private:
    void handleEulerRotationSet();
};

QMLDESIGNERCORE_EXPORT QList<ModelNode> toModelNodeList(const QList<Qml3DNode> &fxItemNodeList);
QMLDESIGNERCORE_EXPORT QList<Qml3DNode> toQml3DNodeList(const QList<ModelNode> &modelNodeList);

} //QmlDesigner
