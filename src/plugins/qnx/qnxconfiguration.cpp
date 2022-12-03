/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: BlackBerry (qt@blackberry.com)
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

#include "qnxconfiguration.h"
#include "qnxqtversion.h"
#include "qnxutils.h"
#include "qnxtoolchain.h"

#include "debugger/debuggeritem.h"

#include <coreplugin/icore.h>

#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/kitmanager.h>

#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtversionmanager.h>
#include <qtsupport/qtkitinformation.h>

#include <qmakeprojectmanager/qmakeprojectmanagerconstants.h>

#include <debugger/debuggeritem.h>
#include <debugger/debuggeritemmanager.h>
#include <debugger/debuggerkitinformation.h>

#include <coreplugin/icore.h>
#include <utils/algorithm.h>

#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QFileInfo>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;
using namespace Debugger;

namespace Qnx {
namespace Internal {

const QLatin1String QNXEnvFileKey("EnvFile");
const QLatin1String QNXVersionKey("QNXVersion");
// For backward compatibility
const QLatin1String SdpEnvFileKey("NDKEnvFile");

const QLatin1String QNXConfiguration("QNX_CONFIGURATION");
const QLatin1String QNXTarget("QNX_TARGET");
const QLatin1String QNXHost("QNX_HOST");

QnxConfiguration::QnxConfiguration() = default;

QnxConfiguration::QnxConfiguration(const FilePath &sdpEnvFile)
{
    setDefaultConfiguration(sdpEnvFile);
    readInformation();
}

QnxConfiguration::QnxConfiguration(const QVariantMap &data)
{
    QString envFilePath = data.value(QNXEnvFileKey).toString();
    if (envFilePath.isEmpty())
        envFilePath = data.value(SdpEnvFileKey).toString();

    m_version = QnxVersionNumber(data.value(QNXVersionKey).toString());

    setDefaultConfiguration(FilePath::fromString(envFilePath));
    readInformation();
}

FilePath QnxConfiguration::envFile() const
{
    return m_envFile;
}

FilePath QnxConfiguration::qnxTarget() const
{
    return m_qnxTarget;
}

FilePath QnxConfiguration::qnxHost() const
{
    return m_qnxHost;
}

FilePath QnxConfiguration::qccCompilerPath() const
{
    return m_qccCompiler;
}

EnvironmentItems QnxConfiguration::qnxEnv() const
{
    return m_qnxEnv;
}

QnxVersionNumber QnxConfiguration::version() const
{
    return m_version;
}

QVariantMap QnxConfiguration::toMap() const
{
    QVariantMap data;
    data.insert(QLatin1String(QNXEnvFileKey), m_envFile.toString());
    data.insert(QLatin1String(QNXVersionKey), m_version.toString());
    return data;
}

bool QnxConfiguration::isValid() const
{
    return !m_qccCompiler.isEmpty() && !m_targets.isEmpty();
}

QString QnxConfiguration::displayName() const
{
    return m_configName;
}

bool QnxConfiguration::activate()
{
    if (isActive())
        return true;

    if (!isValid()) {
        QString errorMessage
                = QCoreApplication::translate("Qnx::Internal::QnxConfiguration",
                                              "The following errors occurred while activating the QNX configuration:");
        foreach (const QString &error, validationErrors())
            errorMessage += QLatin1String("\n") + error;

        QMessageBox::warning(Core::ICore::dialogParent(),
                             QCoreApplication::translate("Qnx::Internal::QnxConfiguration",
                                                         "Cannot Set Up QNX Configuration"),
                             errorMessage,
                             QMessageBox::Ok);
        return false;
    }

    foreach (const Target &target, m_targets)
        createTools(target);

    return true;
}

void QnxConfiguration::deactivate()
{
    if (!isActive())
        return;

    const Toolchains toolChainsToRemove =
        ToolChainManager::toolchains(Utils::equal(&ToolChain::compilerCommand, qccCompilerPath()));

    QList<DebuggerItem> debuggersToRemove;
    foreach (DebuggerItem debuggerItem,
             DebuggerItemManager::debuggers()) {
        if (findTargetByDebuggerPath(debuggerItem.command()))
            debuggersToRemove.append(debuggerItem);
    }

    foreach (Kit *kit, KitManager::kits()) {
        if (kit->isAutoDetected()
                && DeviceTypeKitAspect::deviceTypeId(kit) == Constants::QNX_QNX_OS_TYPE
                && toolChainsToRemove.contains(ToolChainKitAspect::cxxToolChain(kit)))
            KitManager::deregisterKit(kit);
    }

    for (ToolChain *tc : toolChainsToRemove)
        ToolChainManager::deregisterToolChain(tc);

    foreach (DebuggerItem debuggerItem, debuggersToRemove)
        DebuggerItemManager::deregisterDebugger(debuggerItem.id());
}

bool QnxConfiguration::isActive() const
{
    const bool hasToolChain = ToolChainManager::toolChain(Utils::equal(&ToolChain::compilerCommand,
                                                                       qccCompilerPath()));
    const bool hasDebugger = Utils::contains(DebuggerItemManager::debuggers(), [this](const DebuggerItem &di) {
        return findTargetByDebuggerPath(di.command());
    });

    return hasToolChain && hasDebugger;
}

bool QnxConfiguration::canCreateKits() const
{
    if (!isValid())
        return false;

    return Utils::anyOf(m_targets,
                        [this](const Target &target) -> bool { return qnxQtVersion(target); });
}

FilePath QnxConfiguration::sdpPath() const
{
    return envFile().parentDir();
}

QnxQtVersion *QnxConfiguration::qnxQtVersion(const Target &target) const
{
    foreach (QtVersion *version,
             QtVersionManager::instance()->versions(Utils::equal(&QtVersion::type,
                                                                         QString::fromLatin1(Constants::QNX_QNX_QT)))) {
        auto qnxQt = dynamic_cast<QnxQtVersion *>(version);
        if (qnxQt && qnxQt->sdpPath() == sdpPath()) {
            foreach (const Abi &qtAbi, version->qtAbis()) {
                if ((qtAbi == target.m_abi) && (qnxQt->cpuDir() == target.cpuDir()))
                    return qnxQt;
            }
        }
    }

    return nullptr;
}

QList<ToolChain *> QnxConfiguration::autoDetect(const QList<ToolChain *> &alreadyKnown)
{
    QList<ToolChain *> result;

    for (const Target &target : qAsConst(m_targets))
        result += findToolChain(alreadyKnown, target.m_abi);

    return result;
}

void QnxConfiguration::createTools(const Target &target)
{
    QnxToolChainMap toolchainMap = createToolChain(target);
    QVariant debuggerId = createDebugger(target);
    createKit(target, toolchainMap, debuggerId);
}

QVariant QnxConfiguration::createDebugger(const Target &target)
{
    Utils::Environment sysEnv = Utils::Environment::systemEnvironment();
    sysEnv.modify(qnxEnvironmentItems());
    Debugger::DebuggerItem debugger;
    debugger.setCommand(target.m_debuggerPath);
    debugger.reinitializeFromFile(sysEnv);
    debugger.setAutoDetected(true);
    debugger.setUnexpandedDisplayName(
                QCoreApplication::translate(
                    "Qnx::Internal::QnxConfiguration",
                    "Debugger for %1 (%2)")
                .arg(displayName())
                .arg(target.shortDescription()));
    return Debugger::DebuggerItemManager::registerDebugger(debugger);
}

QnxConfiguration::QnxToolChainMap QnxConfiguration::createToolChain(const Target &target)
{
    QnxToolChainMap toolChainMap;

    for (auto language : { ProjectExplorer::Constants::C_LANGUAGE_ID,
                           ProjectExplorer::Constants::CXX_LANGUAGE_ID}) {
        auto toolChain = new QnxToolChain;
        toolChain->setDetection(ToolChain::AutoDetection);
        toolChain->setLanguage(language);
        toolChain->setTargetAbi(target.m_abi);
        toolChain->setDisplayName(
                    QCoreApplication::translate(
                        "Qnx::Internal::QnxConfiguration",
                        "QCC for %1 (%2)")
                    .arg(displayName())
                    .arg(target.shortDescription()));
        toolChain->setSdpPath(sdpPath());
        toolChain->setCpuDir(target.cpuDir());
        toolChain->resetToolChain(qccCompilerPath());
        ToolChainManager::registerToolChain(toolChain);

        toolChainMap.insert(std::make_pair(language, toolChain));
    }

    return toolChainMap;
}

QList<ToolChain *> QnxConfiguration::findToolChain(const QList<ToolChain *> &alreadyKnown,
                                                   const Abi &abi)
{
    return Utils::filtered(alreadyKnown, [this, abi](ToolChain *tc) {
                                             return tc->typeId() == Constants::QNX_TOOLCHAIN_ID
                                                 && tc->targetAbi() == abi
                                                 && tc->compilerCommand() == m_qccCompiler;
                                         });
}

void QnxConfiguration::createKit(const Target &target, const QnxToolChainMap &toolChainMap,
                                 const QVariant &debugger)
{
    QnxQtVersion *qnxQt = qnxQtVersion(target);
    // Do not create incomplete kits if no qt qnx version found
    if (!qnxQt)
        return;

    const auto init = [&](Kit *k) {
        QtKitAspect::setQtVersion(k, qnxQt);
        ToolChainKitAspect::setToolChain(k, toolChainMap.at(ProjectExplorer::Constants::C_LANGUAGE_ID));
        ToolChainKitAspect::setToolChain(k, toolChainMap.at(ProjectExplorer::Constants::CXX_LANGUAGE_ID));

        if (debugger.isValid())
            DebuggerKitAspect::setDebugger(k, debugger);

        DeviceTypeKitAspect::setDeviceTypeId(k, Constants::QNX_QNX_OS_TYPE);
        // TODO: Add sysroot?

        k->setUnexpandedDisplayName(
                    QCoreApplication::translate(
                        "Qnx::Internal::QnxConfiguration",
                        "Kit for %1 (%2)")
                    .arg(displayName())
                    .arg(target.shortDescription()));

        k->setAutoDetected(true);
        k->setAutoDetectionSource(envFile().toString());
        k->setMutable(DeviceKitAspect::id(), true);

        k->setSticky(ToolChainKitAspect::id(), true);
        k->setSticky(DeviceTypeKitAspect::id(), true);
        k->setSticky(SysRootKitAspect::id(), true);
        k->setSticky(DebuggerKitAspect::id(), true);
        k->setSticky(QmakeProjectManager::Constants::KIT_INFORMATION_ID, true);

        EnvironmentKitAspect::setEnvironmentChanges(k, qnxEnvironmentItems());
    };

    // add kit with device and qt version not sticky
    KitManager::registerKit(init);
}

QStringList QnxConfiguration::validationErrors() const
{
    QStringList errorStrings;
    if (m_qccCompiler.isEmpty())
        errorStrings << QCoreApplication::translate("Qnx::Internal::QnxConfiguration",
                                                    "- No GCC compiler found.");

    if (m_targets.isEmpty())
        errorStrings << QCoreApplication::translate("Qnx::Internal::QnxConfiguration",
                                                    "- No targets found.");

    return errorStrings;
}

void QnxConfiguration::setVersion(const QnxVersionNumber &version)
{
    m_version = version;
}

void QnxConfiguration::readInformation()
{
    const QString qConfigPath = m_qnxConfiguration.pathAppended("qconfig").toString();
    const QList <ConfigInstallInformation> installInfoList = QnxUtils::installedConfigs(qConfigPath);
    if (installInfoList.isEmpty())
        return;

    for (const ConfigInstallInformation &info : installInfoList) {
        if (m_qnxHost == FilePath::fromString(info.host).canonicalPath()
                && m_qnxTarget == FilePath::fromString(info.target).canonicalPath()) {
            m_configName = info.name;
            setVersion(QnxVersionNumber(info.version));
            break;
        }
    }
}

void QnxConfiguration::setDefaultConfiguration(const FilePath &envScript)
{
    QTC_ASSERT(!envScript.isEmpty(), return);
    m_envFile = envScript;
    m_qnxEnv = QnxUtils::qnxEnvironmentFromEnvFile(m_envFile);
    foreach (const EnvironmentItem &item, m_qnxEnv) {
        if (item.name == QNXConfiguration)
            m_qnxConfiguration = FilePath::fromString(item.value).canonicalPath();
        else if (item.name == QNXTarget)
            m_qnxTarget = FilePath::fromString(item.value).canonicalPath();
        else if (item.name == QNXHost)
            m_qnxHost = FilePath::fromString(item.value).canonicalPath();
    }

    const FilePath qccPath = m_qnxHost.pathAppended("usr/bin/qcc").withExecutableSuffix();
    if (qccPath.exists())
        m_qccCompiler = qccPath;

    updateTargets();
    assignDebuggersToTargets();

    // Remove debuggerless targets.
    Utils::erase(m_targets, [](const Target &target) {
        if (target.m_debuggerPath.isEmpty())
            qWarning() << "No debugger found for" << target.m_path << "... discarded";
        return target.m_debuggerPath.isEmpty();
    });
}

EnvironmentItems QnxConfiguration::qnxEnvironmentItems() const
{
    Utils::EnvironmentItems envList;
    envList.push_back(EnvironmentItem(QNXConfiguration, m_qnxConfiguration.toString()));
    envList.push_back(EnvironmentItem(QNXTarget, m_qnxTarget.toString()));
    envList.push_back(EnvironmentItem(QNXHost, m_qnxHost.toString()));

    return envList;
}

const QnxConfiguration::Target *QnxConfiguration::findTargetByDebuggerPath(
        const FilePath &path) const
{
    const auto it = std::find_if(m_targets.begin(), m_targets.end(),
                           [path](const Target &target) { return target.m_debuggerPath == path; });
    return it == m_targets.end() ? nullptr : &(*it);
}

void QnxConfiguration::updateTargets()
{
    m_targets.clear();
    QList<QnxTarget> targets = QnxUtils::findTargets(m_qnxTarget);
    for (const auto &target : targets)
        m_targets.append(Target(target.m_abi, target.m_path));
}

void QnxConfiguration::assignDebuggersToTargets()
{
    const FilePath hostUsrBinDir = m_qnxHost.pathAppended("usr/bin");
    const FilePaths debuggerNames = hostUsrBinDir.dirEntries(
                {{HostOsInfo::withExecutableSuffix("nto*-gdb")}, QDir::Files});
    Environment sysEnv = Environment::systemEnvironment();
    sysEnv.modify(qnxEnvironmentItems());
    for (const FilePath &debuggerPath : debuggerNames) {
        DebuggerItem item;
        item.setCommand(debuggerPath);
        item.reinitializeFromFile(sysEnv);
        bool found = false;
        for (const Abi &abi : item.abis()) {
            for (Target &target : m_targets) {
                if (target.m_abi.isCompatibleWith(abi)) {
                    found = true;

                    if (target.m_debuggerPath.isEmpty()) {
                        target.m_debuggerPath = debuggerPath;
                    } else {
                        qWarning() << debuggerPath << "has the same ABI as" << target.m_debuggerPath
                                   << "... discarded";
                        break;
                    }
                }
            }
        }
        if (!found)
            qWarning() << "No target found for" << debuggerPath.toUserOutput() << "... discarded";
    }
}

QString QnxConfiguration::Target::shortDescription() const
{
    return QnxUtils::cpuDirShortDescription(cpuDir());
}

QString QnxConfiguration::Target::cpuDir() const
{
    return m_path.fileName();
}

} // namespace Internal
} // namespace Qnx
