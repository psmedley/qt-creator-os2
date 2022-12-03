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

#include "qmlobjectnode.h"
#include "qmlitemnode.h"
#include "qmlstate.h"
#include "qmltimelinekeyframegroup.h"
#include "qmlvisualnode.h"
#include "qml3dnode.h"
#include "variantproperty.h"
#include "nodeproperty.h"
#include <invalidmodelnodeexception.h>
#include "abstractview.h"
#include "nodeinstance.h"
#include "nodemetainfo.h"
#include "bindingproperty.h"
#include "nodelistproperty.h"
#include "nodeinstanceview.h"

#include <qmltimeline.h>

#ifndef QMLDESIGNER_TEST
#include <qmldesignerplugin.h>
#endif

#include <utils/qtcassert.h>

#include <QRegularExpression>

namespace QmlDesigner {

void QmlObjectNode::setVariantProperty(const PropertyName &name, const QVariant &value)
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (timelineIsActive() && currentTimeline().isRecording()) {
        modelNode().validId();

        QmlTimelineKeyframeGroup timelineFrames(currentTimeline().keyframeGroup(modelNode(), name));

        Q_ASSERT(timelineFrames.isValid());

        qreal frame = currentTimeline().modelNode().auxiliaryData("currentFrame@NodeInstance").toReal();
        timelineFrames.setValue(value, frame);

        return;
    } else if (modelNode().hasId() && timelineIsActive() && currentTimeline().hasKeyframeGroup(modelNode(), name)) {
        QmlTimelineKeyframeGroup timelineFrames(currentTimeline().keyframeGroup(modelNode(), name));

        Q_ASSERT(timelineFrames.isValid());

        if (timelineFrames.isRecording()) {
            qreal frame = currentTimeline().modelNode().auxiliaryData("currentFrame@NodeInstance").toReal();
            timelineFrames.setValue(value, frame);

            return;
        }
    }

    if (isInBaseState()) {
        modelNode().variantProperty(name).setValue(value); //basestate
    } else {
        modelNode().validId();

        QmlPropertyChanges changeSet(currentState().propertyChanges(modelNode()));
        Q_ASSERT(changeSet.isValid());
        changeSet.modelNode().variantProperty(name).setValue(value);
    }
}

void QmlObjectNode::setBindingProperty(const PropertyName &name, const QString &expression)
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (isInBaseState()) {
        modelNode().bindingProperty(name).setExpression(expression); //basestate
    } else {
        modelNode().validId();

        QmlPropertyChanges changeSet(currentState().propertyChanges(modelNode()));
        Q_ASSERT(changeSet.isValid());
        changeSet.modelNode().bindingProperty(name).setExpression(expression);
    }
}

QmlModelState QmlObjectNode::currentState() const
{
    if (isValid())
        return QmlModelState(view()->currentStateNode());
    else
        return QmlModelState();
}

QmlTimeline QmlObjectNode::currentTimeline() const
{
    if (isValid())
        return view()->currentTimeline();
    else
        return QmlTimeline();
}

bool QmlObjectNode::isRootModelNode() const
{
    return modelNode().isRootNode();
}


/*!
    Returns the value of the property specified by \name that is based on an
    actual instance. The return value is not the value in the model, but the
    value of a real instantiated instance of this object.
*/
QVariant  QmlObjectNode::instanceValue(const PropertyName &name) const
{
    return nodeInstance().property(name);
}


bool QmlObjectNode::hasProperty(const PropertyName &name) const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (currentState().hasPropertyChanges(modelNode())) {
        QmlPropertyChanges propertyChanges = currentState().propertyChanges(modelNode());
        if (propertyChanges.modelNode().hasProperty(name))
            return true;
    }

    return modelNode().hasProperty(name);
}

bool QmlObjectNode::hasBindingProperty(const PropertyName &name) const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (currentState().hasPropertyChanges(modelNode())) {
        QmlPropertyChanges propertyChanges = currentState().propertyChanges(modelNode());
        if (propertyChanges.modelNode().hasBindingProperty(name))
            return true;
    }

    return modelNode().hasBindingProperty(name);
}

NodeAbstractProperty QmlObjectNode::nodeAbstractProperty(const PropertyName &name) const
{
    return modelNode().nodeAbstractProperty(name);
}

NodeAbstractProperty QmlObjectNode::defaultNodeAbstractProperty() const
{
    return modelNode().defaultNodeAbstractProperty();
}

NodeProperty QmlObjectNode::nodeProperty(const PropertyName &name) const
{
    return modelNode().nodeProperty(name);
}

NodeListProperty QmlObjectNode::nodeListProperty(const PropertyName &name) const
{
    return modelNode().nodeListProperty(name);
}

bool QmlObjectNode::instanceHasValue(const PropertyName &name) const
{
    return nodeInstance().hasProperty(name);
}

bool QmlObjectNode::propertyAffectedByCurrentState(const PropertyName &name) const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (currentState().isBaseState())
        return modelNode().hasProperty(name);

    if (timelineIsActive() && currentTimeline().hasTimeline(modelNode(), name))
        return true;

    if (!currentState().hasPropertyChanges(modelNode()))
        return false;

    return currentState().propertyChanges(modelNode()).modelNode().hasProperty(name);
}

QVariant QmlObjectNode::modelValue(const PropertyName &name) const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (timelineIsActive() && currentTimeline().hasTimeline(modelNode(), name)) {
        QmlTimelineKeyframeGroup timelineFrames(currentTimeline().keyframeGroup(modelNode(), name));

        Q_ASSERT(timelineFrames.isValid());

        qreal frame = currentTimeline().modelNode().auxiliaryData("currentFrame@NodeInstance").toReal();

        QVariant value = timelineFrames.value(frame);

        if (!value.isValid()) //interpolation is not done in the model
            value = instanceValue(name);

        return value;
    }

    if (currentState().isBaseState())
        return modelNode().variantProperty(name).value();

    if (!currentState().hasPropertyChanges(modelNode()))
        return modelNode().variantProperty(name).value();

    QmlPropertyChanges propertyChanges(currentState().propertyChanges(modelNode()));

    if (!propertyChanges.modelNode().hasProperty(name))
        return modelNode().variantProperty(name).value();

    return propertyChanges.modelNode().variantProperty(name).value();
}

bool QmlObjectNode::isTranslatableText(const PropertyName &name) const
{
    if (modelNode().metaInfo().isValid() && modelNode().metaInfo().hasProperty(name))
        if (modelNode().metaInfo().propertyTypeName(name) == "QString" || modelNode().metaInfo().propertyTypeName(name) == "string") {
            if (modelNode().hasBindingProperty(name)) {
                static QRegularExpression regularExpressionPattern(
                            QLatin1String("^qsTr(|Id|anslate)\\(\".*\"\\)$"));
                return modelNode().bindingProperty(name).expression().contains(regularExpressionPattern);
            }

            return false;
        }

    return false;
}

QString QmlObjectNode::stripedTranslatableText(const PropertyName &name) const
{
    if (modelNode().hasBindingProperty(name)) {
        static QRegularExpression regularExpressionPattern(
                    QLatin1String("^qsTr(|Id|anslate)\\(\"(.*)\"\\)$"));
        const QRegularExpressionMatch match = regularExpressionPattern.match(
                    modelNode().bindingProperty(name).expression());
        if (match.hasMatch())
            return match.captured(2);
        return instanceValue(name).toString();
    }
    return instanceValue(name).toString();
}

QString QmlObjectNode::expression(const PropertyName &name) const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (currentState().isBaseState())
        return modelNode().bindingProperty(name).expression();

    if (!currentState().hasPropertyChanges(modelNode()))
        return modelNode().bindingProperty(name).expression();

    QmlPropertyChanges propertyChanges(currentState().propertyChanges(modelNode()));

    if (!propertyChanges.modelNode().hasProperty(name))
        return modelNode().bindingProperty(name).expression();

    return propertyChanges.modelNode().bindingProperty(name).expression();
}

/*!
    Returns \c true if the ObjectNode is in the BaseState.
*/
bool QmlObjectNode::isInBaseState() const
{
    return currentState().isBaseState();
}

bool QmlObjectNode::timelineIsActive() const
{
    return currentTimeline().isValid();
}

bool QmlObjectNode::instanceCanReparent() const
{
    return isInBaseState();
}

QmlPropertyChanges QmlObjectNode::propertyChangeForCurrentState() const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

     if (currentState().isBaseState())
         return QmlPropertyChanges();

     if (!currentState().hasPropertyChanges(modelNode()))
         return QmlPropertyChanges();

     return currentState().propertyChanges(modelNode());
}

static void removeStateOperationsForChildren(const QmlObjectNode &node)
{
    if (node.isValid()) {
        for (QmlModelStateOperation stateOperation : node.allAffectingStatesOperations()) {
            stateOperation.modelNode().destroy(); //remove of belonging StatesOperations
        }

        for (const QmlObjectNode childNode : node.modelNode().directSubModelNodes()) {
            removeStateOperationsForChildren(childNode);
        }
    }
}

static void removeAnimationsFromAnimation(const ModelNode &animation)
{
    QTC_ASSERT(animation.isValid(), return);

    const QList<ModelNode> propertyAnimations = animation.subModelNodesOfType(
        "QtQuick.PropertyAnimation");

    for (const ModelNode &child : propertyAnimations) {
        if (!child.hasBindingProperty("target")) {
            ModelNode nonConst = animation;
            nonConst.destroy();
            return;
        }
    }
}

static void removeAnimationsFromTransition(const ModelNode &transition, const QmlObjectNode &node)
{
    QTC_ASSERT(node.isValid(), return);
    QTC_ASSERT(transition.isValid(), return);

    const auto children = transition.directSubModelNodes();
    for (const ModelNode &parallel : children)
        removeAnimationsFromAnimation(parallel);
}

static void removeDanglingAnimationsFromTransitions(const QmlObjectNode &node)
{
    QTC_ASSERT(node.isValid(), return);

    auto root = node.view()->rootModelNode();

    if (root.isValid() && root.hasProperty("transitions")) {
        NodeAbstractProperty transitions = root.nodeAbstractProperty("transitions");
        if (transitions.isValid()) {
            const auto transitionNodes = transitions.directSubNodes();
            for (const auto &transition : transitionNodes)
                removeAnimationsFromTransition(transition, node);
        }
    }
}

static void removeAliasExports(const QmlObjectNode &node)
{

    PropertyName propertyName = node.id().toUtf8();

    ModelNode rootNode = node.view()->rootModelNode();
    bool hasAliasExport = !propertyName.isEmpty()
            && rootNode.isValid()
            && rootNode.hasBindingProperty(propertyName)
            && rootNode.bindingProperty(propertyName).isAliasExport();

    if (hasAliasExport)
        rootNode.removeProperty(propertyName);


    const QList<ModelNode> nodes = node.modelNode().directSubModelNodes();
    for (const ModelNode &childNode : nodes) {
        removeAliasExports(childNode);
    }

}

static void removeLayerEnabled(const ModelNode &node)
{
    QTC_ASSERT(node.isValid(), return );
    if (node.parentProperty().isValid() && node.parentProperty().name() == "layer.effect") {
        ModelNode parent = node.parentProperty().parentModelNode();
        if (parent.isValid() && parent.hasProperty("layer.enabled"))
            parent.removeProperty("layer.enabled");
    }
}

static void deleteAllReferencesToNodeAndChildren(const ModelNode &node)
{
    BindingProperty::deleteAllReferencesTo(node);
    const auto subNodes = node.allSubModelNodes();
    for (const ModelNode &child : subNodes)
        BindingProperty::deleteAllReferencesTo(child);
}

/*!
    Deletes this object's node and its dependencies from the model.
    Everything that belongs to this Object, the ModelNode, and ChangeOperations
    is deleted from the model.
*/
void QmlObjectNode::destroy()
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    removeLayerEnabled(modelNode());
    removeAliasExports(modelNode());

    for (QmlModelStateOperation stateOperation : allAffectingStatesOperations()) {
        stateOperation.modelNode().destroy(); //remove of belonging StatesOperations
    }

    QVector<ModelNode> timelineNodes;
    const auto allNodes = view()->allModelNodes();
    for (const auto &timelineNode : allNodes) {
        if (QmlTimeline::isValidQmlTimeline(timelineNode))
            timelineNodes.append(timelineNode);
    }

    const auto subNodes = modelNode().allSubModelNodesAndThisNode();
    for (auto &timelineNode : qAsConst(timelineNodes)) {
        QmlTimeline timeline(timelineNode);
        for (const auto &subNode : subNodes)
            timeline.destroyKeyframesForTarget(subNode);
    }

    bool wasFlowEditorTarget = false;
    if (QmlFlowTargetNode::isFlowEditorTarget(modelNode())) {
        QmlFlowTargetNode(modelNode()).destroyTargets();
        wasFlowEditorTarget = true;
    }

    removeStateOperationsForChildren(modelNode());
    deleteAllReferencesToNodeAndChildren(modelNode());

    removeDanglingAnimationsFromTransitions(modelNode());

    QmlFlowViewNode root(view()->rootModelNode());

    modelNode().destroy();

    if (wasFlowEditorTarget && root.isValid())
        root.removeDanglingTransitions();
}

void QmlObjectNode::ensureAliasExport()
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (!isAliasExported()) {
        modelNode().validId();
        ModelNode rootModelNode = view()->rootModelNode();
        rootModelNode.bindingProperty(modelNode().id().toUtf8()).
            setDynamicTypeNameAndExpression("alias", modelNode().id());
    }
}

bool QmlObjectNode::isAliasExported() const
{

    if (modelNode().isValid() && !modelNode().id().isEmpty()) {
         PropertyName modelNodeId = modelNode().id().toUtf8();
         ModelNode rootModelNode = view()->rootModelNode();
         Q_ASSERT(rootModelNode.isValid());
         if (rootModelNode.hasBindingProperty(modelNodeId)
                 && rootModelNode.bindingProperty(modelNodeId).isDynamic()
                 && rootModelNode.bindingProperty(modelNodeId).expression() == modelNode().id())
             return true;
    }

    return false;
}

/*!
    Returns a list of states the affect this object.
*/

QList<QmlModelState> QmlObjectNode::allAffectingStates() const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    QList<QmlModelState> returnList;

    const QList<QmlModelState> states = allDefinedStates();
    for (const QmlModelState &state : states) {
        if (state.affectsModelNode(modelNode()))
            returnList.append(state);
    }
    return returnList;
}

/*!
    Returns a list of all state operations that affect this object.
*/

QList<QmlModelStateOperation> QmlObjectNode::allAffectingStatesOperations() const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    QList<QmlModelStateOperation> returnList;
    const QList<QmlModelState> states = allDefinedStates();
    for (const QmlModelState &state : states) {
        if (state.affectsModelNode(modelNode()))
            returnList.append(state.stateOperations(modelNode()));
    }

    return returnList;
}

static QList<QmlVisualNode> allQmlVisualNodesRecursive(const QmlItemNode &qmlItemNode)
{
    QList<QmlVisualNode> qmlVisualNodeList;

    if (qmlItemNode.isValid()) {
        qmlVisualNodeList.append(qmlItemNode);

        const QList<ModelNode> nodes = qmlItemNode.modelNode().directSubModelNodes();
        for (const ModelNode &modelNode : nodes) {
            if (QmlVisualNode::isValidQmlVisualNode(modelNode))
                qmlVisualNodeList.append(allQmlVisualNodesRecursive(modelNode));
        }
    }

    return qmlVisualNodeList;
}

QList<QmlModelState> QmlObjectNode::allDefinedStates() const
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    QList<QmlModelState> returnList;

    QList<QmlVisualNode> allVisualNodes;

    if (QmlVisualNode::isValidQmlVisualNode(view()->rootModelNode()))
        allVisualNodes.append(allQmlVisualNodesRecursive(view()->rootModelNode()));

    for (const QmlVisualNode &node : qAsConst(allVisualNodes))
        returnList.append(node.states().allStates());

    return returnList;
}

QList<QmlModelStateOperation> QmlObjectNode::allInvalidStateOperations() const
{
    QList<QmlModelStateOperation> result;

    const auto allStates =  allDefinedStates();
    for (const auto &state : allStates)
        result.append(state.allInvalidStateOperations());
    return result;
}


/*!
    Removes a variant property of the object specified by \a name from the
    model.
*/

void  QmlObjectNode::removeProperty(const PropertyName &name)
{
    if (!isValid())
        throw new InvalidModelNodeException(__LINE__, __FUNCTION__, __FILE__);

    if (isInBaseState()) {
        modelNode().removeProperty(name); //basestate
    } else {
        QmlPropertyChanges changeSet(currentState().propertyChanges(modelNode()));
        Q_ASSERT(changeSet.isValid());
        changeSet.removeProperty(name);
    }
}

QList<ModelNode> toModelNodeList(const QList<QmlObjectNode> &qmlObjectNodeList)
{
    QList<ModelNode> modelNodeList;

    for (const QmlObjectNode &qmlObjectNode : qmlObjectNodeList)
        modelNodeList.append(qmlObjectNode.modelNode());

    return modelNodeList;
}

QList<QmlObjectNode> toQmlObjectNodeList(const QList<ModelNode> &modelNodeList)
{
    QList<QmlObjectNode> qmlObjectNodeList;

    for (const ModelNode &modelNode : modelNodeList) {
        if (QmlObjectNode::isValidQmlObjectNode(modelNode))
             qmlObjectNodeList.append(modelNode);
    }

    return qmlObjectNodeList;
}

bool QmlObjectNode::isAncestorOf(const QmlObjectNode &objectNode) const
{
    return modelNode().isAncestorOf(objectNode.modelNode());
}

QVariant QmlObjectNode::instanceValue(const ModelNode &modelNode, const PropertyName &name)
{
    Q_ASSERT(modelNode.view()->nodeInstanceView()->hasInstanceForModelNode(modelNode));
    return modelNode.view()->nodeInstanceView()->instanceForModelNode(modelNode).property(name);
}

QString QmlObjectNode::generateTranslatableText(const QString &text)
{
#ifndef QMLDESIGNER_TEST

    if (QmlDesignerPlugin::instance()->settings().value(
            DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION).toInt())

        switch (QmlDesignerPlugin::instance()->settings().value(
                    DesignerSettingsKey::TYPE_OF_QSTR_FUNCTION).toInt()) {
        case 0: return QString(QStringLiteral("qsTr(\"%1\")")).arg(text);
        case 1: return QString(QStringLiteral("qsTrId(\"%1\")")).arg(text);
        case 2: return QString(QStringLiteral("qsTranslate(\"\"\"%1\")")).arg(text);
        default:
            break;

        }
    return QString(QStringLiteral("qsTr(\"%1\")")).arg(text);
#else
    Q_UNUSED(text)
    return QString();
#endif
}

TypeName QmlObjectNode::instanceType(const PropertyName &name) const
{
    return nodeInstance().instanceType(name);
}

bool QmlObjectNode::instanceHasBinding(const PropertyName &name) const
{
    return nodeInstance().hasBindingForProperty(name);
}

NodeInstance QmlObjectNode::nodeInstance() const
{
    return nodeInstanceView()->instanceForModelNode(modelNode());
}

QmlObjectNode QmlObjectNode::nodeForInstance(const NodeInstance &instance) const
{
    return QmlObjectNode(ModelNode(instance.modelNode(), view()));
}

QmlItemNode QmlObjectNode::itemForInstance(const NodeInstance &instance) const
{
    return QmlItemNode(ModelNode(instance.modelNode(), view()));
}

QmlObjectNode::QmlObjectNode()
{
}

QmlObjectNode::QmlObjectNode(const ModelNode &modelNode)
    : QmlModelNodeFacade(modelNode)
{
}

bool QmlObjectNode::isValidQmlObjectNode(const ModelNode &modelNode)
{
    return isValidQmlModelNodeFacade(modelNode);
}

bool QmlObjectNode::isValid() const
{
    return isValidQmlObjectNode(modelNode());
}

bool QmlObjectNode::hasError() const
{
    if (isValid())
        return nodeInstance().hasError();
    else
        qDebug() << "called hasError() on an invalid QmlObjectNode";
    return false;
}

QString QmlObjectNode::error() const
{
    if (hasError())
        return nodeInstance().error();
    return QString();
}

bool QmlObjectNode::hasNodeParent() const
{
    return modelNode().hasParentProperty();
}

bool QmlObjectNode::hasInstanceParent() const
{
    return nodeInstance().parentId() >= 0 && nodeInstanceView()->hasInstanceForId(nodeInstance().parentId());
}

bool QmlObjectNode::hasInstanceParentItem() const
{
    return isValid()
           && nodeInstance().parentId() >= 0
           && nodeInstanceView()->hasInstanceForId(nodeInstance().parentId())
           && QmlItemNode::isItemOrWindow(view()->modelNodeForInternalId(nodeInstance().parentId()));
}


void QmlObjectNode::setParentProperty(const NodeAbstractProperty &parentProeprty)
{
    return modelNode().setParentProperty(parentProeprty);
}

QmlObjectNode QmlObjectNode::instanceParent() const
{
    if (hasInstanceParent())
        return nodeForInstance(nodeInstanceView()->instanceForId(nodeInstance().parentId()));

    return QmlObjectNode();
}

QmlItemNode QmlObjectNode::instanceParentItem() const
{
    if (hasInstanceParentItem())
        return itemForInstance(nodeInstanceView()->instanceForId(nodeInstance().parentId()));

    return QmlItemNode();
}

QmlItemNode QmlObjectNode::modelParentItem() const
{
    return modelNode().parentProperty().parentModelNode();
}

void QmlObjectNode::setId(const QString &id)
{
    modelNode().setIdWithRefactoring(id);
}

QString QmlObjectNode::id() const
{
    return modelNode().id();
}

QString QmlObjectNode::validId()
{
    return modelNode().validId();
}

bool QmlObjectNode::hasDefaultPropertyName() const
{
    return modelNode().metaInfo().hasDefaultProperty();
}

PropertyName QmlObjectNode::defaultPropertyName() const
{
    return modelNode().metaInfo().defaultPropertyName();
}

void QmlObjectNode::setParent(const QmlObjectNode &newParent)
{
    if (newParent.hasDefaultPropertyName())
        newParent.modelNode().defaultNodeAbstractProperty().reparentHere(modelNode());
}

QmlItemNode QmlObjectNode::toQmlItemNode() const
{
    return QmlItemNode(modelNode());
}

QmlVisualNode QmlObjectNode::toQmlVisualNode() const
{
     return QmlVisualNode(modelNode());
}

QString QmlObjectNode::simplifiedTypeName() const
{
    return modelNode().simplifiedTypeName();
}

QStringList QmlObjectNode::allStateNames() const
{
    return nodeInstance().allStateNames();
}

QmlObjectNode *QmlObjectNode::getQmlObjectNodeOfCorrectType(const ModelNode &modelNode)
{
    // Create QmlObjectNode of correct type for the modelNode
    // Note: Currently we are only interested in differentiating 3D nodes, so no check for
    // visual nodes is done for efficiency reasons
    if (modelNode.isValid() && modelNode.isSubclassOf("QtQuick3D.Node"))
        return new Qml3DNode(modelNode);
    return new QmlObjectNode(modelNode);
}

bool QmlObjectNode::isBlocked(const PropertyName &propName) const
{
    Q_UNUSED(propName)
    return false;
}

} //QmlDesigner
