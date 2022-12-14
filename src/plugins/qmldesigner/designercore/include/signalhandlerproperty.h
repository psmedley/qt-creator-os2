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

#include "qmldesignercorelib_global.h"
#include "abstractproperty.h"

namespace QmlDesigner {

class QMLDESIGNERCORE_EXPORT SignalHandlerProperty : public AbstractProperty
{
    friend ModelNode;
    friend Internal::ModelPrivate;
    friend AbstractProperty;

public:
    void setSource(const QString &source);
    QString source() const;

    SignalHandlerProperty();
    SignalHandlerProperty(const SignalHandlerProperty &property, AbstractView *view);

    static PropertyName prefixAdded(const PropertyName &propertyName);
    static PropertyName prefixRemoved(const PropertyName &propertyName);

protected:
    SignalHandlerProperty(const PropertyName &propertyName, const Internal::InternalNodePointer &internalNode, Model* model, AbstractView *view);
};

class QMLDESIGNERCORE_EXPORT SignalDeclarationProperty : public AbstractProperty
{
    friend ModelNode;
    friend Internal::ModelPrivate;
    friend AbstractProperty;

public:
    void setSignature(const QString &source);
    QString signature() const;

    SignalDeclarationProperty();
    SignalDeclarationProperty(const SignalDeclarationProperty &property, AbstractView *view);

protected:
    SignalDeclarationProperty(const PropertyName &propertyName, const Internal::InternalNodePointer &internalNode, Model* model, AbstractView *view);
};

} // namespace QmlDesigner
