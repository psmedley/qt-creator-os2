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

#include "qttestconfiguration.h"
#include "qttestconstants.h"
#include "qttestoutputreader.h"
#include "qttestsettings.h"
#include "qttest_utils.h"
#include "../autotestplugin.h"
#include "../itestframework.h"
#include "../testsettings.h"

#include <utils/stringutils.h>

namespace Autotest {
namespace Internal {

TestOutputReader *QtTestConfiguration::outputReader(const QFutureInterface<TestResultPtr> &fi,
                                                    Utils::QtcProcess *app) const
{
    auto qtSettings = static_cast<QtTestSettings *>(framework()->testSettings());
    const QtTestOutputReader::OutputMode mode = qtSettings && qtSettings->useXMLOutput.value()
            ? QtTestOutputReader::XML
            : QtTestOutputReader::PlainText;
    return new QtTestOutputReader(fi, app, buildDirectory(), projectFile(), mode, TestType::QtTest);
}

QStringList QtTestConfiguration::argumentsForTestRunner(QStringList *omitted) const
{
    QStringList arguments;
    if (AutotestPlugin::settings()->processArgs) {
        arguments.append(QTestUtils::filterInterfering(
                             runnable().command.arguments().split(' ', Qt::SkipEmptyParts),
                             omitted, false));
    }
    auto qtSettings = static_cast<QtTestSettings *>(framework()->testSettings());
    if (!qtSettings)
        return arguments;
    if (qtSettings->useXMLOutput.value())
        arguments << "-xml";
    if (!testCases().isEmpty())
        arguments << testCases();

    const QString &metricsOption = QtTestSettings::metricsTypeToOption(MetricsType(qtSettings->metrics.value()));
    if (!metricsOption.isEmpty())
        arguments << metricsOption;

    if (qtSettings->verboseBench.value())
        arguments << "-vb";

    if (qtSettings->logSignalsSlots.value())
        arguments << "-vs";

    if (isDebugRunMode() && qtSettings->noCrashHandler.value())
        arguments << "-nocrashhandler";

    if (qtSettings->limitWarnings.value() && qtSettings->maxWarnings.value() != 2000)
        arguments << "-maxwarnings" << QString::number(qtSettings->maxWarnings.value());

    return arguments;
}

Utils::Environment QtTestConfiguration::filteredEnvironment(const Utils::Environment &original) const
{
    return QTestUtils::prepareBasicEnvironment(original);
}

} // namespace Internal
} // namespace Autotest
