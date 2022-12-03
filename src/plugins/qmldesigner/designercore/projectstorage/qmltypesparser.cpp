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

#include "qmltypesparser.h"

#include "projectstorage.h"
#include "sourcepathcache.h"

#include <sqlitedatabase.h>

#include <qmlcompiler/qqmljstypedescriptionreader_p.h>
#include <qmldom/qqmldomtop_p.h>

#include <QDateTime>

#include <algorithm>
#include <tuple>

namespace QmlDesigner {

namespace QmlDom = QQmlJS::Dom;

namespace {

void appendImports(Storage::Imports &imports,
                   const QString &dependency,
                   SourceId sourceId,
                   QmlTypesParser::ProjectStorage &storage)
{
    auto spaceFound = std::find_if(dependency.begin(), dependency.end(), [](QChar c) {
        return c.isSpace();
    });

    Utils::PathString moduleName{QStringView(dependency.begin(), spaceFound)};
    moduleName.append("-cppnative");
    ModuleId cppModuleId = storage.moduleId(moduleName);

    auto majorVersionFound = std::find_if(spaceFound, dependency.end(), [](QChar c) {
        return c.isDigit();
    });
    auto majorVersionEnd = std::find_if(majorVersionFound, dependency.end(), [](QChar c) {
        return !c.isDigit();
    });

    Storage::Version version;

    QStringView majorVersionString(majorVersionFound, majorVersionEnd);
    if (!majorVersionString.isEmpty()) {
        version.major.value = majorVersionString.toInt();

        auto minorVersionFound = std::find_if(majorVersionEnd, dependency.end(), [](QChar c) {
            return c.isDigit();
        });
        auto minorVersionEnd = std::find_if(minorVersionFound, dependency.end(), [](QChar c) {
            return !c.isDigit();
        });
        QStringView minorVersionString(minorVersionFound, minorVersionEnd);
        if (!minorVersionString.isEmpty())
            version.minor.value = QStringView(minorVersionFound, minorVersionEnd).toInt();
    }

    imports.emplace_back(cppModuleId, version, sourceId);
}

void addImports(Storage::Imports &imports,
                SourceId sourceId,
                const QStringList &dependencies,
                QmlTypesParser::ProjectStorage &storage)
{
    for (const QString &dependency : dependencies)
        appendImports(imports, dependency, sourceId, storage);

    imports.emplace_back(storage.moduleId("QML"), Storage::Version{}, sourceId);
    imports.emplace_back(storage.moduleId("QtQml-cppnative"), Storage::Version{}, sourceId);
}

Storage::TypeAccessSemantics createTypeAccessSemantics(QQmlJSScope::AccessSemantics accessSematics)
{
    switch (accessSematics) {
    case QQmlJSScope::AccessSemantics::Reference:
        return Storage::TypeAccessSemantics::Reference;
    case QQmlJSScope::AccessSemantics::Value:
        return Storage::TypeAccessSemantics::Value;
    case QQmlJSScope::AccessSemantics::None:
        return Storage::TypeAccessSemantics::None;
    case QQmlJSScope::AccessSemantics::Sequence:
        return Storage::TypeAccessSemantics::Sequence;
    }

    return Storage::TypeAccessSemantics::None;
}

Storage::Version createVersion(QTypeRevision qmlVersion)
{
    return Storage::Version{qmlVersion.majorVersion(), qmlVersion.minorVersion()};
}

Storage::ExportedTypes createExports(const QList<QQmlJSScope::Export> &qmlExports,
                                     const QQmlJSScope &component,
                                     QmlTypesParser::ProjectStorage &storage,
                                     ModuleId cppModuleId)
{
    Storage::ExportedTypes exportedTypes;
    exportedTypes.reserve(Utils::usize(qmlExports));

    for (const QQmlJSScope::Export &qmlExport : qmlExports) {
        exportedTypes.emplace_back(storage.moduleId(Utils::SmallString{qmlExport.package()}),
                                   Utils::SmallString{qmlExport.type()},
                                   createVersion(qmlExport.version()));
    }

    exportedTypes.emplace_back(cppModuleId, Utils::SmallString{component.internalName()});

    return exportedTypes;
}

Storage::PropertyDeclarationTraits createPropertyDeclarationTraits(const QQmlJSMetaProperty &qmlProperty)
{
    Storage::PropertyDeclarationTraits traits{};

    if (qmlProperty.isList())
        traits = traits | Storage::PropertyDeclarationTraits::IsList;

    if (qmlProperty.isPointer())
        traits = traits | Storage::PropertyDeclarationTraits::IsPointer;

    if (!qmlProperty.isWritable())
        traits = traits | Storage::PropertyDeclarationTraits::IsReadOnly;

    return traits;
}

Storage::PropertyDeclarations createProperties(const QHash<QString, QQmlJSMetaProperty> &qmlProperties)
{
    Storage::PropertyDeclarations propertyDeclarations;
    propertyDeclarations.reserve(Utils::usize(qmlProperties));

    for (const QQmlJSMetaProperty &qmlProperty : qmlProperties) {
        propertyDeclarations.emplace_back(Utils::SmallString{qmlProperty.propertyName()},
                                          Storage::NativeType{
                                              Utils::SmallString{qmlProperty.typeName()}},
                                          createPropertyDeclarationTraits(qmlProperty));
    }

    return propertyDeclarations;
}

Storage::ParameterDeclarations createParameters(const QQmlJSMetaMethod &qmlMethod)
{
    Storage::ParameterDeclarations parameterDeclarations;

    const QStringList &parameterNames = qmlMethod.parameterNames();
    const QStringList &parameterTypeNames = qmlMethod.parameterTypeNames();
    auto currentName = parameterNames.begin();
    auto currentType = parameterTypeNames.begin();
    auto nameEnd = parameterNames.end();
    auto typeEnd = parameterTypeNames.end();

    for (; currentName != nameEnd && currentType != typeEnd; ++currentName, ++currentType) {
        parameterDeclarations.emplace_back(Utils::SmallString{*currentName},
                                           Utils::SmallString{*currentType});
    }

    return parameterDeclarations;
}

std::tuple<Storage::FunctionDeclarations, Storage::SignalDeclarations> createFunctionAndSignals(
    const QMultiHash<QString, QQmlJSMetaMethod> &qmlMethods)
{
    std::tuple<Storage::FunctionDeclarations, Storage::SignalDeclarations> functionAndSignalDeclarations;
    Storage::FunctionDeclarations &functionsDeclarations{std::get<0>(functionAndSignalDeclarations)};
    functionsDeclarations.reserve(Utils::usize(qmlMethods));
    Storage::SignalDeclarations &signalDeclarations{std::get<1>(functionAndSignalDeclarations)};
    signalDeclarations.reserve(Utils::usize(qmlMethods));

    for (const QQmlJSMetaMethod &qmlMethod : qmlMethods) {
        if (qmlMethod.methodType() != QQmlJSMetaMethod::Type::Signal) {
            functionsDeclarations.emplace_back(Utils::SmallString{qmlMethod.methodName()},
                                               Utils::SmallString{qmlMethod.returnTypeName()},
                                               createParameters(qmlMethod));
        } else {
            signalDeclarations.emplace_back(Utils::SmallString{qmlMethod.methodName()},
                                            createParameters(qmlMethod));
        }
    }

    return functionAndSignalDeclarations;
}

Storage::EnumeratorDeclarations createEnumeratorsWithValues(const QQmlJSMetaEnum &qmlEnumeration)
{
    Storage::EnumeratorDeclarations enumeratorDeclarations;

    const QStringList &keys = qmlEnumeration.keys();
    const QList<int> &values = qmlEnumeration.values();
    auto currentKey = keys.begin();
    auto currentValue = values.begin();
    auto keyEnd = keys.end();
    auto valueEnd = values.end();

    for (; currentKey != keyEnd && currentValue != valueEnd; ++currentKey, ++currentValue)
        enumeratorDeclarations.emplace_back(Utils::SmallString{*currentKey}, *currentValue);

    return enumeratorDeclarations;
}

Storage::EnumeratorDeclarations createEnumeratorsWithoutValues(const QQmlJSMetaEnum &qmlEnumeration)
{
    Storage::EnumeratorDeclarations enumeratorDeclarations;

    for (const QString &key : qmlEnumeration.keys())
        enumeratorDeclarations.emplace_back(Utils::SmallString{key});

    return enumeratorDeclarations;
}

Storage::EnumeratorDeclarations createEnumerators(const QQmlJSMetaEnum &qmlEnumeration)
{
    if (qmlEnumeration.hasValues())
        return createEnumeratorsWithValues(qmlEnumeration);

    return createEnumeratorsWithoutValues(qmlEnumeration);
}

Storage::EnumerationDeclarations createEnumeration(const QHash<QString, QQmlJSMetaEnum> &qmlEnumerations)
{
    Storage::EnumerationDeclarations enumerationDeclarations;
    enumerationDeclarations.reserve(Utils::usize(qmlEnumerations));

    for (const QQmlJSMetaEnum &qmlEnumeration : qmlEnumerations) {
        enumerationDeclarations.emplace_back(Utils::SmallString{qmlEnumeration.name()},
                                             createEnumerators(qmlEnumeration));
    }

    return enumerationDeclarations;
}

void addType(Storage::Types &types,
             SourceId sourceId,
             ModuleId cppModuleId,
             const QQmlJSScope &component,
             QmlTypesParser::ProjectStorage &storage)
{
    auto [functionsDeclarations, signalDeclarations] = createFunctionAndSignals(component.ownMethods());
    types.emplace_back(Utils::SmallString{component.internalName()},
                       Storage::NativeType{Utils::SmallString{component.baseTypeName()}},
                       createTypeAccessSemantics(component.accessSemantics()),
                       sourceId,
                       createExports(component.exports(), component, storage, cppModuleId),
                       createProperties(component.ownProperties()),
                       std::move(functionsDeclarations),
                       std::move(signalDeclarations),
                       createEnumeration(component.ownEnumerations()));
}

void addTypes(Storage::Types &types,
              const Storage::ProjectData &projectData,
              const QHash<QString, QQmlJSScope::Ptr> &objects,
              QmlTypesParser::ProjectStorage &storage)
{
    types.reserve(Utils::usize(objects) + types.size());

    for (const auto &object : objects)
        addType(types, projectData.sourceId, projectData.moduleId, *object.get(), storage);
}

} // namespace

void QmlTypesParser::parse(const QString &sourceContent,
                           Storage::Imports &imports,
                           Storage::Types &types,
                           const Storage::ProjectData &projectData)
{
    QQmlJSTypeDescriptionReader reader({}, sourceContent);
    QHash<QString, QQmlJSScope::Ptr> components;
    QStringList dependencies;
    bool isValid = reader(&components, &dependencies);
    if (!isValid)
        throw CannotParseQmlTypesFile{};

    addImports(imports, projectData.sourceId, dependencies, m_storage);
    addTypes(types, projectData, components, m_storage);
}

} // namespace QmlDesigner
