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

#include "quicktestframework.h"
#include "quicktestparser.h"
#include "quicktesttreeitem.h"

#include "../autotestconstants.h"
#include "../testframeworkmanager.h"
#include "../qtest/qttestconstants.h"

namespace Autotest {
namespace Internal {

ITestParser *QuickTestFramework::createTestParser()
{
    return new QuickTestParser(this);
}

ITestTreeItem *QuickTestFramework::createRootNode()
{
    return new QuickTestTreeItem(this, displayName(),
                                 Utils::FilePath(), ITestTreeItem::Root);
}

const char *QuickTestFramework::name() const
{
    return QuickTest::Constants::FRAMEWORK_NAME;
}

QString QuickTestFramework::displayName() const
{
    return QCoreApplication::translate("QuickTestFramework", "Quick Test");
}

unsigned QuickTestFramework::priority() const
{
    return 5;
}

ITestSettings *QuickTestFramework::testSettings()
{
    static const Utils::Id id
            = Utils::Id(Constants::FRAMEWORK_PREFIX).withSuffix(QtTest::Constants::FRAMEWORK_NAME);
    ITestFramework *qtTestFramework = TestFrameworkManager::frameworkForId(id);
    return qtTestFramework->testSettings();
}

} // namespace Internal
} // namespace Autotest
