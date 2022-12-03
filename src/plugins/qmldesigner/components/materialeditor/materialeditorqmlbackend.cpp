/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "materialeditorqmlbackend.h"

#include "propertyeditorvalue.h"
#include "materialeditortransaction.h"
#include "materialeditorcontextobject.h"
#include <qmldesignerconstants.h>
#include <qmltimeline.h>

#include <qmlobjectnode.h>
#include <nodemetainfo.h>
#include <variantproperty.h>
#include <bindingproperty.h>

#include <coreplugin/icore.h>
#include <utils/qtcassert.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QFileInfo>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWidget>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

static QObject *variantToQObject(const QVariant &value)
{
    if (value.userType() == QMetaType::QObjectStar || value.userType() > QMetaType::User)
        return *(QObject **)value.constData();

    return nullptr;
}

namespace QmlDesigner {

class MaterialEditorImageProvider : public QQuickImageProvider
{
    QPixmap m_previewPixmap;

public:
    MaterialEditorImageProvider()
        : QQuickImageProvider(Pixmap) {}

    void setPixmap(const QPixmap &pixmap)
    {
        m_previewPixmap = pixmap;
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        Q_UNUSED(requestedSize)

        static QPixmap defaultPreview = QPixmap::fromImage(QImage(":/materialeditor/images/defaultmaterialpreview.png"));

        QPixmap pixmap{150, 150};

        if (id == "preview") {
            if (!m_previewPixmap.isNull())
                pixmap = m_previewPixmap;
            else
                pixmap = defaultPreview;
        } else {
            qWarning() << __FUNCTION__ << "Unsupported image id:" << id;
            pixmap.fill(Qt::red);
        }


        if (size)
            *size = pixmap.size();

        return pixmap;
    }
};

MaterialEditorQmlBackend::MaterialEditorQmlBackend(MaterialEditorView *materialEditor)
    : m_view(new QQuickWidget)
    , m_materialEditorTransaction(new MaterialEditorTransaction(materialEditor))
    , m_contextObject(new MaterialEditorContextObject())
    , m_materialEditorImageProvider(new MaterialEditorImageProvider())
{
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->engine()->addImportPath(propertyEditorResourcesPath() + "/imports");
    m_view->engine()->addImageProvider("materialEditor", m_materialEditorImageProvider);
    m_contextObject->setBackendValues(&m_backendValuesPropertyMap);
    m_contextObject->setModel(materialEditor->model());
    context()->setContextObject(m_contextObject.data());

    QObject::connect(&m_backendValuesPropertyMap, &DesignerPropertyMap::valueChanged,
                     materialEditor, &MaterialEditorView::changeValue);
}

MaterialEditorQmlBackend::~MaterialEditorQmlBackend()
{
}

PropertyName MaterialEditorQmlBackend::auxNamePostFix(const PropertyName &propertyName)
{
    return propertyName + "__AUX";
}

void MaterialEditorQmlBackend::createPropertyEditorValue(const QmlObjectNode &qmlObjectNode,
                                                         const PropertyName &name,
                                                         const QVariant &value,
                                                         MaterialEditorView *materialEditor)
{
    PropertyName propertyName(name);
    propertyName.replace('.', '_');
    auto valueObject = qobject_cast<PropertyEditorValue *>(variantToQObject(backendValuesPropertyMap().value(QString::fromUtf8(propertyName))));
    if (!valueObject) {
        valueObject = new PropertyEditorValue(&backendValuesPropertyMap());
        QObject::connect(valueObject, &PropertyEditorValue::valueChanged, &backendValuesPropertyMap(), &DesignerPropertyMap::valueChanged);
        QObject::connect(valueObject, &PropertyEditorValue::expressionChanged, materialEditor, &MaterialEditorView::changeExpression);
        QObject::connect(valueObject, &PropertyEditorValue::exportPropertyAsAliasRequested, materialEditor, &MaterialEditorView::exportPropertyAsAlias);
        QObject::connect(valueObject, &PropertyEditorValue::removeAliasExportRequested, materialEditor, &MaterialEditorView::removeAliasExport);
        backendValuesPropertyMap().insert(QString::fromUtf8(propertyName), QVariant::fromValue(valueObject));
    }
    valueObject->setName(name);
    valueObject->setModelNode(qmlObjectNode);

    if (qmlObjectNode.propertyAffectedByCurrentState(name) && !(qmlObjectNode.modelNode().property(name).isBindingProperty()))
        valueObject->setValue(qmlObjectNode.modelValue(name));
    else
        valueObject->setValue(value);

    if (propertyName != "id" && qmlObjectNode.currentState().isBaseState()
        && qmlObjectNode.modelNode().property(propertyName).isBindingProperty()) {
        valueObject->setExpression(qmlObjectNode.modelNode().bindingProperty(propertyName).expression());
    } else {
        if (qmlObjectNode.hasBindingProperty(name))
            valueObject->setExpression(qmlObjectNode.expression(name));
        else
            valueObject->setExpression(qmlObjectNode.instanceValue(name).toString());
    }
}

void MaterialEditorQmlBackend::setValue(const QmlObjectNode &, const PropertyName &name, const QVariant &value)
{
    // Vector*D values need to be split into their subcomponents
    if (value.type() == QVariant::Vector2D) {
        const char *suffix[2] = {"_x", "_y"};
        auto vecValue = value.value<QVector2D>();
        for (int i = 0; i < 2; ++i) {
            PropertyName subPropName(name.size() + 2, '\0');
            subPropName.replace(0, name.size(), name);
            subPropName.replace(name.size(), 2, suffix[i]);
            auto propertyValue = qobject_cast<PropertyEditorValue *>(variantToQObject(m_backendValuesPropertyMap.value(QString::fromUtf8(subPropName))));
            if (propertyValue)
                propertyValue->setValue(QVariant(vecValue[i]));
        }
    } else if (value.type() == QVariant::Vector3D) {
        const char *suffix[3] = {"_x", "_y", "_z"};
        auto vecValue = value.value<QVector3D>();
        for (int i = 0; i < 3; ++i) {
            PropertyName subPropName(name.size() + 2, '\0');
            subPropName.replace(0, name.size(), name);
            subPropName.replace(name.size(), 2, suffix[i]);
            auto propertyValue = qobject_cast<PropertyEditorValue *>(variantToQObject(m_backendValuesPropertyMap.value(QString::fromUtf8(subPropName))));
            if (propertyValue)
                propertyValue->setValue(QVariant(vecValue[i]));
        }
    } else if (value.type() == QVariant::Vector4D) {
        const char *suffix[4] = {"_x", "_y", "_z", "_w"};
        auto vecValue = value.value<QVector4D>();
        for (int i = 0; i < 4; ++i) {
            PropertyName subPropName(name.size() + 2, '\0');
            subPropName.replace(0, name.size(), name);
            subPropName.replace(name.size(), 2, suffix[i]);
            auto propertyValue = qobject_cast<PropertyEditorValue *>(
                variantToQObject(m_backendValuesPropertyMap.value(QString::fromUtf8(subPropName))));
            if (propertyValue)
                propertyValue->setValue(QVariant(vecValue[i]));
        }
    } else {
        PropertyName propertyName = name;
        propertyName.replace('.', '_');
        auto propertyValue = qobject_cast<PropertyEditorValue *>(variantToQObject(m_backendValuesPropertyMap.value(QString::fromUtf8(propertyName))));
        if (propertyValue)
            propertyValue->setValue(value);
    }
}

QQmlContext *MaterialEditorQmlBackend::context() const
{
    return m_view->rootContext();
}

MaterialEditorContextObject *MaterialEditorQmlBackend::contextObject() const
{
    return m_contextObject.data();
}

QQuickWidget *MaterialEditorQmlBackend::widget() const
{
    return m_view;
}

void MaterialEditorQmlBackend::setSource(const QUrl &url)
{
    m_view->setSource(url);
}

Internal::QmlAnchorBindingProxy &MaterialEditorQmlBackend::backendAnchorBinding()
{
    return m_backendAnchorBinding;
}

void MaterialEditorQmlBackend::updateMaterialPreview(const QPixmap &pixmap)
{
    m_materialEditorImageProvider->setPixmap(pixmap);
    QMetaObject::invokeMethod(m_view->rootObject(), "refreshPreview");
}

DesignerPropertyMap &MaterialEditorQmlBackend::backendValuesPropertyMap()
{
    return m_backendValuesPropertyMap;
}

MaterialEditorTransaction *MaterialEditorQmlBackend::materialEditorTransaction() const
{
    return m_materialEditorTransaction.data();
}

PropertyEditorValue *MaterialEditorQmlBackend::propertyValueForName(const QString &propertyName)
{
     return qobject_cast<PropertyEditorValue *>(variantToQObject(backendValuesPropertyMap().value(propertyName)));
}

void MaterialEditorQmlBackend::setup(const QmlObjectNode &selectedMaterialNode, const QString &stateName,
                                     const QUrl &qmlSpecificsFile, MaterialEditorView *materialEditor)
{
    if (selectedMaterialNode.isValid()) {
        m_contextObject->setModel(materialEditor->model());

        const PropertyNameList propertyNames = selectedMaterialNode.modelNode().metaInfo().propertyNames();
        for (const PropertyName &propertyName : propertyNames)
            createPropertyEditorValue(selectedMaterialNode, propertyName, selectedMaterialNode.instanceValue(propertyName), materialEditor);

        // model node
        m_backendModelNode.setup(selectedMaterialNode.modelNode());
        context()->setContextProperty("modelNodeBackend", &m_backendModelNode);
        context()->setContextProperty("hasMaterial", QVariant(true));

        // className
        auto valueObject = qobject_cast<PropertyEditorValue *>(variantToQObject(
            m_backendValuesPropertyMap.value(Constants::PROPERTY_EDITOR_CLASSNAME_PROPERTY)));
        if (!valueObject)
            valueObject = new PropertyEditorValue(&m_backendValuesPropertyMap);
        valueObject->setName(Constants::PROPERTY_EDITOR_CLASSNAME_PROPERTY);
        valueObject->setModelNode(selectedMaterialNode.modelNode());
        valueObject->setValue(m_backendModelNode.simplifiedTypeName());
        QObject::connect(valueObject,
                         &PropertyEditorValue::valueChanged,
                         &backendValuesPropertyMap(),
                         &DesignerPropertyMap::valueChanged);
        m_backendValuesPropertyMap.insert(Constants::PROPERTY_EDITOR_CLASSNAME_PROPERTY,
                                          QVariant::fromValue(valueObject));

        // anchors
        m_backendAnchorBinding.setup(selectedMaterialNode.modelNode());
        context()->setContextProperties(
            QVector<QQmlContext::PropertyPair>{
                {{"anchorBackend"}, QVariant::fromValue(&m_backendAnchorBinding)},
                {{"transaction"}, QVariant::fromValue(m_materialEditorTransaction.data())}
            }
        );

        contextObject()->setSpecificsUrl(qmlSpecificsFile);
        contextObject()->setStateName(stateName);

        QStringList stateNames = selectedMaterialNode.allStateNames();
        stateNames.prepend("base state");
        contextObject()->setAllStateNames(stateNames);
        contextObject()->setSelectedMaterial(selectedMaterialNode);
        contextObject()->setIsBaseState(selectedMaterialNode.isInBaseState());
        contextObject()->setHasAliasExport(selectedMaterialNode.isAliasExported());
        contextObject()->setHasActiveTimeline(QmlTimeline::hasActiveTimeline(selectedMaterialNode.view()));

        contextObject()->setSelectionChanged(false);

        NodeMetaInfo metaInfo = selectedMaterialNode.modelNode().metaInfo();
        contextObject()->setMajorVersion(metaInfo.isValid() ? metaInfo.majorVersion() : -1);
    } else {
        context()->setContextProperty("hasMaterial", QVariant(false));
    }
}

QString MaterialEditorQmlBackend::propertyEditorResourcesPath()
{
#ifdef SHARE_QML_PATH
    if (qEnvironmentVariableIsSet("LOAD_QML_FROM_SOURCE"))
        return QLatin1String(SHARE_QML_PATH) + "/propertyEditorQmlSources";
#endif
    return Core::ICore::resourcePath("qmldesigner/propertyEditorQmlSources").toString();
}

void MaterialEditorQmlBackend::emitSelectionToBeChanged()
{
    m_backendModelNode.emitSelectionToBeChanged();
}

void MaterialEditorQmlBackend::emitSelectionChanged()
{
    m_backendModelNode.emitSelectionChanged();
}

void MaterialEditorQmlBackend::setValueforAuxiliaryProperties(const QmlObjectNode &qmlObjectNode, const PropertyName &name)
{
    const PropertyName propertyName = auxNamePostFix(name);
     setValue(qmlObjectNode, propertyName, qmlObjectNode.modelNode().auxiliaryData(name));
}

} // namespace QmlDesigner
