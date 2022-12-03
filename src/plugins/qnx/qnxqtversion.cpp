/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: KDAB (info@kdab.com)
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

#include "qnxqtversion.h"

#include "qnxconstants.h"
#include "qnxutils.h"

#include <coreplugin/featureprovider.h>
#include <proparser/profileevaluator.h>

#include <qtsupport/qtconfigwidget.h>
#include <qtsupport/qtsupportconstants.h>

#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/pathchooser.h>

#include <QDir>
#include <QHBoxLayout>

using namespace ProjectExplorer;
using namespace Utils;

namespace Qnx {
namespace Internal {

const char SDP_PATH_KEY[] = "SDKPath";

const char QNX_TARGET_KEY[] = "QNX_TARGET";
const char QNX_HOST_KEY[]   = "QNX_HOST";
const char QNX_QNX_FEATURE[] = "QtSupport.Wizards.FeatureQNX";

class QnxBaseQtConfigWidget : public QtSupport::QtConfigWidget
{
public:
    explicit QnxBaseQtConfigWidget(QnxQtVersion *version)
    {
        QTC_ASSERT(version, return);

        auto layout = new QHBoxLayout(this);
        auto sdpPathChooser(new PathChooser);
        layout->addWidget(sdpPathChooser);

        sdpPathChooser->setExpectedKind(PathChooser::ExistingDirectory);
        sdpPathChooser->setHistoryCompleter("Qnx.Sdp.History");
        sdpPathChooser->setFilePath(version->sdpPath());

        connect(sdpPathChooser, &PathChooser::rawPathChanged, [this, version, sdpPathChooser] {
            version->setSdpPath(sdpPathChooser->filePath());
            emit changed();
        });
    }
};

QnxQtVersion::QnxQtVersion() = default;

QString QnxQtVersion::description() const
{
    //: Qt Version is meant for QNX
    return QCoreApplication::translate("Qnx::Internal::QnxQtVersion", "QNX %1")
            .arg(QnxUtils::cpuDirShortDescription(cpuDir()));
}

QSet<Id> QnxQtVersion::availableFeatures() const
{
    QSet<Id> features = QtSupport::QtVersion::availableFeatures();
    features.insert(QNX_QNX_FEATURE);
    features.remove(QtSupport::Constants::FEATURE_QT_CONSOLE);
    features.remove(QtSupport::Constants::FEATURE_QT_WEBKIT);
    return features;
}

QSet<Id> QnxQtVersion::targetDeviceTypes() const
{
    return {Constants::QNX_QNX_OS_TYPE};
}

QString QnxQtVersion::qnxHost() const
{
    if (!m_environmentUpToDate)
        updateEnvironment();

    for (const EnvironmentItem &item : qAsConst(m_qnxEnv)) {
        if (item.name == QLatin1String(QNX_HOST_KEY))
            return item.value;
    }

    return QString();
}

FilePath QnxQtVersion::qnxTarget() const
{
    if (!m_environmentUpToDate)
        updateEnvironment();

    for (EnvironmentItem &item : m_qnxEnv) {
        if (item.name == QNX_TARGET_KEY)
            return FilePath::fromUserInput(item.value);
    }

    return FilePath();
}

QString QnxQtVersion::cpuDir() const
{
    const Abis abis = qtAbis();
    if (abis.empty())
        return QString();
    return QnxUtils::cpuDirFromAbi(abis.at(0));
}

QVariantMap QnxQtVersion::toMap() const
{
    QVariantMap result = QtVersion::toMap();
    result.insert(SDP_PATH_KEY, sdpPath().toVariant());
    return result;
}

void QnxQtVersion::fromMap(const QVariantMap &map)
{
    QtVersion::fromMap(map);
    setSdpPath(FilePath::fromVariant(map.value(SDP_PATH_KEY)));
}

Abis QnxQtVersion::detectQtAbis() const
{
    ensureMkSpecParsed();
    return QnxUtils::convertAbis(QtVersion::detectQtAbis());
}

void QnxQtVersion::addToEnvironment(const Kit *k, Environment &env) const
{
    QtSupport::QtVersion::addToEnvironment(k, env);
    updateEnvironment();
    env.modify(m_qnxEnv);
}

void QnxQtVersion::setupQmakeRunEnvironment(Environment &env) const
{
    if (!sdpPath().isEmpty())
        updateEnvironment();

    env.modify(m_qnxEnv);
}

QtSupport::QtConfigWidget *QnxQtVersion::createConfigurationWidget() const
{
    return new QnxBaseQtConfigWidget(const_cast<QnxQtVersion *>(this));
}

bool QnxQtVersion::isValid() const
{
    return QtSupport::QtVersion::isValid() && !sdpPath().isEmpty();
}

QString QnxQtVersion::invalidReason() const
{
    if (sdpPath().isEmpty())
        return QCoreApplication::translate("Qnx::Internal::QnxQtVersion",
                                           "No SDP path was set up.");
    return QtSupport::QtVersion::invalidReason();
}

FilePath QnxQtVersion::sdpPath() const
{
    return m_sdpPath;
}

void QnxQtVersion::setSdpPath(const FilePath &sdpPath)
{
    if (m_sdpPath == sdpPath)
        return;

    m_sdpPath = sdpPath;
    m_environmentUpToDate = false;
}

void QnxQtVersion::updateEnvironment() const
{
    if (!m_environmentUpToDate) {
        m_qnxEnv = environment();
        m_environmentUpToDate = true;
    }
}

EnvironmentItems QnxQtVersion::environment() const
{
    return QnxUtils::qnxEnvironment(sdpPath());
}


// Factory

QnxQtVersionFactory::QnxQtVersionFactory()
{
    setQtVersionCreator([] { return new QnxQtVersion; });
    setSupportedType(Constants::QNX_QNX_QT);
    setPriority(50);
    setRestrictionChecker([](const SetupData &setup) { return setup.isQnx; });
}

} // namespace Internal
} // namespace Qnx
