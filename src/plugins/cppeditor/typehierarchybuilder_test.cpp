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

#include "typehierarchybuilder_test.h"

#include "cppmodelmanager.h"
#include "cpptoolstestcase.h"
#include "typehierarchybuilder.h"

#include <cplusplus/Overview.h>
#include <cplusplus/SymbolVisitor.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QtTest>

using namespace CPlusPlus;
using CppEditor::Internal::Tests::CppTestDocument;

Q_DECLARE_METATYPE(QList<CppTestDocument>)

namespace CppEditor::Internal {
namespace {

QString toString(const TypeHierarchy &hierarchy, int indent = 0)
{
    Symbol *symbol = hierarchy.symbol();
    QString result = QString(indent, QLatin1Char(' '))
        + Overview().prettyName(symbol->name()) + QLatin1Char('\n');

    QList<TypeHierarchy> sortedHierarchy = hierarchy.hierarchy();
    Overview oo;
    Utils::sort(sortedHierarchy, [&oo](const TypeHierarchy &h1, const TypeHierarchy &h2) -> bool {
        return oo.prettyName(h1.symbol()->name()) < oo.prettyName(h2.symbol()->name());
    });
    for (const TypeHierarchy &childHierarchy : qAsConst(sortedHierarchy))
        result += toString(childHierarchy, indent + 2);
    return result;
}

class FindFirstClassInDocument: private SymbolVisitor
{
public:
    FindFirstClassInDocument() = default;

    Class *operator()(const Document::Ptr &document)
    {
        accept(document->globalNamespace());
        return m_clazz;
    }

private:
    bool preVisit(Symbol *symbol) override
    {
        if (m_clazz)
            return false;

        if (Class *c = symbol->asClass()) {
            m_clazz = c;
            return false;
        }

        return true;
    }

private:
    Class *m_clazz = nullptr;
};

class TypeHierarchyBuilderTestCase : public CppEditor::Tests::TestCase
{
public:
    TypeHierarchyBuilderTestCase(const QList<CppTestDocument> &documents,
                                 const QString &expectedHierarchy)
    {
        QVERIFY(succeededSoFar());

        CppEditor::Tests::TemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());

        QList<CppTestDocument> documents_ = documents;

        // Write files
        QSet<QString> filePaths;
        for (auto &document : documents_) {
            document.setBaseDirectory(temporaryDir.path());
            QVERIFY(document.writeToDisk());
            filePaths << document.filePath();
        }

        // Parse files
        QVERIFY(parseFiles(filePaths));
        const Snapshot snapshot = globalSnapshot();

        // Get class for which to generate the hierarchy
        const Document::Ptr firstDocument = snapshot.document(documents_.first().filePath());
        QVERIFY(firstDocument);
        QVERIFY(firstDocument->diagnosticMessages().isEmpty());
        Class *clazz = FindFirstClassInDocument()(firstDocument);
        QVERIFY(clazz);

        // Generate and compare hierarchies
        const TypeHierarchy hierarchy
                = TypeHierarchyBuilder::buildDerivedTypeHierarchy(clazz, snapshot);

        const QString actualHierarchy = toString(hierarchy);
//        Uncomment for updating/generating reference data:
//        qDebug() << actualHierarchy;
        QCOMPARE(actualHierarchy, expectedHierarchy);
    }
};

} // anonymous namespace

void TypeHierarchyBuilderTest::test_data()
{
    QTest::addColumn<QList<CppTestDocument> >("documents");
    QTest::addColumn<QString>("expectedHierarchy");

    QTest::newRow("basic-single-document")
        << (QList<CppTestDocument>()
            << CppTestDocument("a.h",
                            "class A {};\n"
                            "class B : public A {};\n"
                            "class C1 : public B {};\n"
                            "class C2 : public B {};\n"
                            "class D : public C1 {};\n"))
        << QString::fromLatin1(
            "A\n"
            "  B\n"
            "    C1\n"
            "      D\n"
            "    C2\n" );

    QTest::newRow("basic-multiple-documents")
        << (QList<CppTestDocument>()
            << CppTestDocument("a.h",
                            "class A {};")
            << CppTestDocument("b.h",
                            "#include \"a.h\"\n"
                            "class B : public A {};")
            << CppTestDocument("c1.h",
                            "#include \"b.h\"\n"
                            "class C1 : public B {};")
            << CppTestDocument("c2.h",
                            "#include \"b.h\"\n"
                            "class C2 : public B {};")
            << CppTestDocument("d.h",
                            "#include \"c1.h\"\n"
                            "class D : public C1 {};"))
        << QString::fromLatin1(
            "A\n"
            "  B\n"
            "    C1\n"
            "      D\n"
            "    C2\n"
            );
}

void TypeHierarchyBuilderTest::test()
{
    QFETCH(QList<CppTestDocument>, documents);
    QFETCH(QString, expectedHierarchy);

    TypeHierarchyBuilderTestCase(documents, expectedHierarchy);
}

} // namespace CppEditor::Internal
