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

#include "qttesttreeitem.h"
#include "qttestconfiguration.h"
#include "qttestparser.h"
#include "qttestframework.h"

#include <cppeditor/cppmodelmanager.h>
#include <projectexplorer/session.h>
#include <utils/qtcassert.h>

namespace Autotest {
namespace Internal {

QtTestTreeItem::QtTestTreeItem(ITestFramework *testFramework, const QString &name,
                               const Utils::FilePath &filePath, TestTreeItem::Type type)
    : TestTreeItem(testFramework, name, filePath, type)
{
    if (type == TestDataTag)
        setData(0, Qt::Checked, Qt::CheckStateRole);
}

TestTreeItem *QtTestTreeItem::copyWithoutChildren()
{
    QtTestTreeItem *copied = new QtTestTreeItem(framework());
    copied->copyBasicDataFrom(this);
    copied->m_inherited = m_inherited;
    copied->m_multiTest = m_multiTest;
    return copied;
}

QVariant QtTestTreeItem::data(int column, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        if (type() == Root)
            break;
        return QVariant(name() + nameSuffix());
    case Qt::ToolTipRole: {
        QString toolTip = TestTreeItem::data(column, role).toString();
        if (m_multiTest && type() == TestCase) {
            toolTip.append(QCoreApplication::translate("QtTestTreeItem",
                            "<p>Multiple testcases inside a single executable are not officially "
                            "supported. Depending on the implementation they might get executed "
                            "or not, but never will be explicitly selectable.</p>"));
        }
        return toolTip;
        break;
    }
    case Qt::CheckStateRole:
        switch (type()) {
        case TestDataFunction:
        case TestSpecialFunction:
            return QVariant();
        default:
            return m_multiTest ? QVariant() : checked();
        }
    case ItalicRole:
        switch (type()) {
        case TestDataFunction:
        case TestSpecialFunction:
            return true;
        default:
            return m_multiTest;
        }
    }
    return TestTreeItem::data(column, role);
}

Qt::ItemFlags QtTestTreeItem::flags(int column) const
{
    static const Qt::ItemFlags defaultFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    switch (type()) {
    case TestDataTag:
        return defaultFlags | Qt::ItemIsUserCheckable;
    case TestFunction:
        return defaultFlags | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable;
    default:
        return m_multiTest ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                           : TestTreeItem::flags(column);
    }
}

Qt::CheckState QtTestTreeItem::checked() const
{
    return m_multiTest ? Qt::Unchecked : TestTreeItem::checked();
}

bool QtTestTreeItem::canProvideTestConfiguration() const
{
    switch (type()) {
    case TestCase:
    case TestFunction:
    case TestDataTag:
        return !m_multiTest;
    default:
        return false;
    }
}

bool QtTestTreeItem::canProvideDebugConfiguration() const
{
    return canProvideTestConfiguration();
}

ITestConfiguration *QtTestTreeItem::testConfiguration() const
{
    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    QTC_ASSERT(project, return nullptr);
    const auto cppMM = CppEditor::CppModelManager::instance();
    QTC_ASSERT(cppMM, return nullptr);

    QtTestConfiguration *config = nullptr;
    switch (type()) {
    case TestCase:
        config = new QtTestConfiguration(framework());
        config->setTestCaseCount(childCount());
        config->setProjectFile(proFile());
        config->setProject(project);
        break;
    case TestFunction: {
        TestTreeItem *parent = parentItem();
        config = new QtTestConfiguration(framework());
        config->setTestCases(QStringList(name()));
        config->setProjectFile(parent->proFile());
        config->setProject(project);
        break;
    }
    case TestDataTag: {
        const TestTreeItem *function = parentItem();
        const TestTreeItem *parent = function ? function->parentItem() : nullptr;
        if (!parent)
            return nullptr;
        const QString functionWithTag = function->name() + ':' + name();
        config = new QtTestConfiguration(framework());
        config->setTestCases(QStringList(functionWithTag));
        config->setProjectFile(parent->proFile());
        config->setProject(project);
        break;
    }
    default:
        return nullptr;
    }
    if (config)
        config->setInternalTargets(cppMM->internalTargets(filePath()));
    return config;
}

static void fillTestConfigurationsFromCheckState(const TestTreeItem *item,
                                                 QList<ITestConfiguration *> &testConfigurations)
{
    const auto cppMM = CppEditor::CppModelManager::instance();
    QTC_ASSERT(cppMM, return);
    QTC_ASSERT(item, return);
    if (item->type() == TestTreeItem::GroupNode) {
        for (int row = 0, count = item->childCount(); row < count; ++row)
            fillTestConfigurationsFromCheckState(item->childItem(row), testConfigurations);
        return;
    }
    QTC_ASSERT(item->type() == TestTreeItem::TestCase, return);
    QtTestConfiguration *testConfig = nullptr;
    switch (item->checked()) {
    case Qt::Unchecked:
        return;
    case Qt::Checked:
        testConfig = static_cast<QtTestConfiguration *>(item->testConfiguration());
        QTC_ASSERT(testConfig, return);
        testConfigurations << testConfig;
        return;
    case Qt::PartiallyChecked:
        QStringList testCases;
        item->forFirstLevelChildren([&testCases](ITestTreeItem *grandChild) {
            if (grandChild->checked() == Qt::Checked) {
                testCases << grandChild->name();
            } else if (grandChild->checked() == Qt::PartiallyChecked) {
                const QString funcName = grandChild->name();
                grandChild->forFirstLevelChildren([&testCases, &funcName](ITestTreeItem *dataTag) {
                    if (dataTag->checked() == Qt::Checked)
                        testCases << funcName + ':' + dataTag->name();
                });
            }
        });

        testConfig = new QtTestConfiguration(item->framework());
        testConfig->setTestCases(testCases);
        testConfig->setProjectFile(item->proFile());
        testConfig->setProject(ProjectExplorer::SessionManager::startupProject());
        testConfig->setInternalTargets(cppMM->internalTargets(item->filePath()));
        testConfigurations << testConfig;
    }
}

static void collectFailedTestInfo(TestTreeItem *item, QList<ITestConfiguration *> &testConfigs)
{
    const auto cppMM = CppEditor::CppModelManager::instance();
    QTC_ASSERT(cppMM, return);
    QTC_ASSERT(item, return);
    if (item->type() == TestTreeItem::GroupNode) {
        for (int row = 0, count = item->childCount(); row < count; ++row)
            collectFailedTestInfo(item->childItem(row), testConfigs);
        return;
    }
    QTC_ASSERT(item->type() == TestTreeItem::TestCase, return);
    QStringList testCases;
    item->forFirstLevelChildren([&testCases](ITestTreeItem *func) {
        if (func->type() == TestTreeItem::TestFunction && func->data(0, FailedRole).toBool()) {
            testCases << func->name();
        } else {
            func->forFirstLevelChildren([&testCases, func](ITestTreeItem *dataTag) {
                if (dataTag->data(0, FailedRole).toBool())
                    testCases << func->name() + ':' + dataTag->name();
            });
        }
    });
    if (testCases.isEmpty())
        return;

    QtTestConfiguration *testConfig = new QtTestConfiguration(item->framework());
    testConfig->setTestCases(testCases);
    testConfig->setProjectFile(item->proFile());
    testConfig->setProject(ProjectExplorer::SessionManager::startupProject());
    testConfig->setInternalTargets(cppMM->internalTargets(item->filePath()));
    testConfigs << testConfig;
}

ITestConfiguration *QtTestTreeItem::debugConfiguration() const
{
    QtTestConfiguration *config = static_cast<QtTestConfiguration *>(testConfiguration());
    if (config)
        config->setRunMode(TestRunMode::Debug);
    return config;
}

QList<ITestConfiguration *> QtTestTreeItem::getAllTestConfigurations() const
{
    QList<ITestConfiguration *> result;

    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    if (!project || type() != Root)
        return result;

    forFirstLevelChildren([&result](ITestTreeItem *child) {
        if (child->type() == TestCase) {
            ITestConfiguration *tc = child->testConfiguration();
            QTC_ASSERT(tc, return);
            result << tc;
        } else if (child->type() == GroupNode) {
            child->forFirstLevelChildren([&result](ITestTreeItem *groupChild) {
                ITestConfiguration *tc = groupChild->testConfiguration();
                QTC_ASSERT(tc, return);
                result << tc;
            });
        }
    });
    return result;
}

QList<ITestConfiguration *> QtTestTreeItem::getSelectedTestConfigurations() const
{
    QList<ITestConfiguration *> result;
    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    if (!project || type() != Root)
        return result;

    for (int row = 0, count = childCount(); row < count; ++row)
        fillTestConfigurationsFromCheckState(childItem(row), result);

    return result;
}

QList<ITestConfiguration *> QtTestTreeItem::getFailedTestConfigurations() const
{
    QList<ITestConfiguration *> result;
    QTC_ASSERT(type() == TestTreeItem::Root, return result);
    for (int row = 0, end = childCount(); row < end; ++row)
        collectFailedTestInfo(childItem(row), result);
    return result;
}

QList<ITestConfiguration *> QtTestTreeItem::getTestConfigurationsForFile(const Utils::FilePath &fileName) const
{
    QList<ITestConfiguration *> result;

    ProjectExplorer::Project *project = ProjectExplorer::SessionManager::startupProject();
    if (!project || type() != Root)
        return result;

    QHash<TestTreeItem *, QStringList> testFunctions;
    forAllChildItems([&testFunctions, &fileName](TestTreeItem *node) {
        if (node->type() == Type::TestFunction && node->filePath() == fileName) {
            QTC_ASSERT(node->parentItem(), return);
            TestTreeItem *testCase = node->parentItem();
            QTC_ASSERT(testCase->type() == Type::TestCase, return);
            testFunctions[testCase] << node->name();
        }
    });

    for (auto it = testFunctions.cbegin(), end = testFunctions.cend(); it != end; ++it) {
        TestConfiguration *tc = static_cast<TestConfiguration *>(it.key()->testConfiguration());
        QTC_ASSERT(tc, continue);
        tc->setTestCases(it.value());
        result << tc;
    }

    return result;
}

TestTreeItem *QtTestTreeItem::find(const TestParseResult *result)
{
    QTC_ASSERT(result, return nullptr);

    switch (type()) {
    case Root:
        if (result->framework->grouping()) {
            const Utils::FilePath path = result->fileName.absolutePath();
            for (int row = 0; row < childCount(); ++row) {
                TestTreeItem *group = childItem(row);
                if (group->filePath() != path)
                    continue;
                if (auto groupChild = group->findChildByFile(result->fileName))
                    return groupChild;
            }
            return nullptr;
        }
        return findChildByNameAndFile(result->name, result->fileName);
    case GroupNode:
        return findChildByNameAndFile(result->name, result->fileName);
    case TestCase: {
        const QtTestParseResult *qtResult = static_cast<const QtTestParseResult *>(result);
        return findChildByNameAndInheritanceAndMultiTest(qtResult->displayName,
                                                         qtResult->inherited(),
                                                         qtResult->runsMultipleTestcases());
    }
    case TestFunction:
    case TestDataFunction:
    case TestSpecialFunction:
        return findChildByName(result->name);
    default:
        return nullptr;
    }
}

TestTreeItem *QtTestTreeItem::findChild(const TestTreeItem *other)
{
    QTC_ASSERT(other, return nullptr);
    const Type otherType = other->type();
    switch (type()) {
    case Root:
        return findChildByFileNameAndType(other->filePath(), other->name(), otherType);
    case GroupNode:
        return otherType == TestCase ? findChildByNameAndFile(other->name(), other->filePath()) : nullptr;
    case TestCase: {
        if (otherType != TestFunction && otherType != TestDataFunction && otherType != TestSpecialFunction)
            return nullptr;
        auto qtOther = static_cast<const QtTestTreeItem *>(other);
        return findChildByNameAndInheritanceAndMultiTest(other->name(), qtOther->inherited(),
                                                         qtOther->runsMultipleTestcases());
    }
    case TestFunction:
    case TestDataFunction:
    case TestSpecialFunction:
        return otherType == TestDataTag ? findChildByName(other->name()) : nullptr;
    default:
        return nullptr;
    }
}

bool QtTestTreeItem::modify(const TestParseResult *result)
{
    QTC_ASSERT(result, return false);

    switch (type()) {
    case TestCase:
        return modifyTestCaseOrSuiteContent(result);
    case TestFunction:
    case TestDataFunction:
    case TestSpecialFunction:
        return modifyTestFunctionContent(result);
    case TestDataTag:
        return modifyDataTagContent(result);
    default:
        return false;
    }
}

TestTreeItem *QtTestTreeItem::createParentGroupNode() const
{
    const Utils::FilePath &absPath = filePath().absolutePath();
    return new QtTestTreeItem(framework(), absPath.baseName(), absPath, TestTreeItem::GroupNode);
}

bool QtTestTreeItem::isGroupable() const
{
    return type() == TestCase;
}

TestTreeItem *QtTestTreeItem::findChildByFileNameAndType(const Utils::FilePath &file,
                                                         const QString &name, Type type) const
{
    return findFirstLevelChildItem([file, name, type](const TestTreeItem *other) {
        return other->type() == type && other->filePath() == file && other->name() == name;
    });
}

TestTreeItem *QtTestTreeItem::findChildByNameAndInheritanceAndMultiTest(const QString &name,
                                                                        bool inherited,
                                                                        bool multiTest) const
{
    return findFirstLevelChildItem([name, inherited, multiTest](const TestTreeItem *other) {
        const QtTestTreeItem *qtOther = static_cast<const QtTestTreeItem *>(other);
        return qtOther->inherited() == inherited && qtOther->runsMultipleTestcases() == multiTest
                && qtOther->name() == name;
    });
}

QString QtTestTreeItem::nameSuffix() const
{
    static const QString inherited{QCoreApplication::translate("QtTestTreeItem", "inherited")};
    static const QString multi{QCoreApplication::translate("QtTestTreeItem", "multiple testcases")};
    QString suffix;
    if (m_inherited)
        suffix.append(inherited);
    if (m_multiTest && type() == TestCase) {
        if (m_inherited)
            suffix.append(", ");
        suffix.append(multi);
    }
    return suffix.isEmpty() ? suffix : QString{" [" + suffix + "]"};
}

} // namespace Internal
} // namespace Autotest
