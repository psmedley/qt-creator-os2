/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "mcusupportsdk.h"
#include "mcuhelpers.h"
#include "mcukitmanager.h"
#include "mculegacyconstants.h"
#include "mcupackage.h"
#include "mcusupportconstants.h"
#include "mcusupportoptions.h"
#include "mcusupportplugin.h"
#include "mcusupportversiondetection.h"
#include "mcutarget.h"
#include "mcutargetdescription.h"
#include "mcutargetfactory.h"
#include "mcutargetfactorylegacy.h"

#include <baremetal/baremetalconstants.h>
#include <coreplugin/icore.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/toolchainmanager.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>

#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include <memory>

using namespace Utils;

namespace McuSupport {
namespace Internal {
namespace Sdk {

namespace {
const char CMAKE_ENTRIES[]{"cmakeEntries"};
} // namespace

static FilePath findInProgramFiles(const QString &folder)
{
    for (auto envVar : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
        if (!qEnvironmentVariableIsSet(envVar))
            continue;
        const FilePath dir = FilePath::fromUserInput(qEnvironmentVariable(envVar)) / folder;
        if (dir.exists())
            return dir;
    }
    return {};
}

McuPackagePtr createQtForMCUsPackage(const SettingsHandler::Ptr &settingsHandler)
{
    return McuPackagePtr{
        new McuPackage(settingsHandler,
                       McuPackage::tr("Qt for MCUs SDK"),
                       FileUtils::homePath(),                           // defaultPath
                       FilePath("bin/qmltocpp").withExecutableSuffix(), // detectionPath
                       Constants::SETTINGS_KEY_PACKAGE_QT_FOR_MCUS_SDK, // settingsKey
                       QStringLiteral("Qul_ROOT"),                      // cmakeVarName
                       QStringLiteral("Qul_DIR"))};                     // envVarName
}

static McuPackageVersionDetector *generatePackageVersionDetector(const QString &envVar)
{
    if (envVar.startsWith("EVK"))
        return new McuPackageXmlVersionDetector("*_manifest_*.xml", "ksdk", "version", ".*");

    if (envVar.startsWith("STM32"))
        return new McuPackageXmlVersionDetector("package.xml",
                                                "PackDescription",
                                                "Release",
                                                R"(\b(\d+\.\d+\.\d+)\b)");

    if (envVar.startsWith("RGL"))
        return new McuPackageDirectoryVersionDetector("rgl_*_obj_*", R"(\d+\.\d+\.\w+)", false);

    return nullptr;
}

/// Create the McuPackage by checking the "boardSdk" property in the JSON file for the board.
/// The name of the environment variable pointing to the the SDK for the board will be defined in the "envVar" property
/// inside the "boardSdk".
McuPackagePtr createBoardSdkPackage(const SettingsHandler::Ptr &settingsHandler,
                                    const McuTargetDescription &desc)
{
    const auto generateSdkName = [](const QString &envVar) {
        qsizetype postfixPos = envVar.indexOf("_SDK_PATH");
        if (postfixPos < 0) {
            postfixPos = envVar.indexOf("_DIR");
        }
        const QString sdkName = postfixPos > 0 ? envVar.left(postfixPos) : envVar;
        return QString{"MCU SDK (%1)"}.arg(sdkName);
    };
    const QString sdkName = desc.boardSdk.name.isEmpty() ? generateSdkName(desc.boardSdk.envVar)
                                                         : desc.boardSdk.name;

    const FilePath defaultPath = [&] {
        const auto envVar = desc.boardSdk.envVar.toLatin1();
        if (qEnvironmentVariableIsSet(envVar))
            return FilePath::fromUserInput(qEnvironmentVariable(envVar));
        if (!desc.boardSdk.defaultPath.isEmpty()) {
            FilePath defaultPath = FilePath::fromUserInput(QDir::rootPath()
                                                           + desc.boardSdk.defaultPath.toString());
            if (defaultPath.exists())
                return defaultPath;
        }
        return FilePath();
    }();

    const auto versionDetector = generatePackageVersionDetector(desc.boardSdk.envVar);

    return McuPackagePtr{new McuPackage(settingsHandler,
                                        sdkName,
                                        defaultPath,
                                        {},                   // detection path
                                        desc.boardSdk.envVar, // settings key
                                        "QUL_BOARD_SDK_DIR",  // cmake var
                                        desc.boardSdk.envVar, // env var
                                        {},                   // download URL
                                        versionDetector)};
}

McuPackagePtr createFreeRTOSSourcesPackage(const SettingsHandler::Ptr &settingsHandler,
                                           const QString &envVar,
                                           const FilePath &boardSdkDir,
                                           const FilePath &freeRTOSBoardSdkSubDir)
{
    const QString envVarPrefix = removeRtosSuffix(envVar);

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar.toLatin1()))
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar.toLatin1()));
    else if (!boardSdkDir.isEmpty() && !freeRTOSBoardSdkSubDir.isEmpty())
        defaultPath = boardSdkDir / freeRTOSBoardSdkSubDir.toString();

    return McuPackagePtr{
        new McuPackage(settingsHandler,
                       QString::fromLatin1("FreeRTOS Sources (%1)").arg(envVarPrefix),
                       defaultPath,
                       {}, // detection path
                       QString{Constants::SETTINGS_KEY_FREERTOS_PREFIX}.append(envVarPrefix),
                       "FREERTOS_DIR",           // cmake var
                       envVar,                   // env var
                       "https://freertos.org")}; // download url
}

McuPackagePtr createUnsupportedToolChainFilePackage(const SettingsHandler::Ptr &settingsHandler,
                                                    const FilePath &qtForMCUSdkPath)
{
    const FilePath toolchainFilePath = qtForMCUSdkPath / Constants::QUL_TOOLCHAIN_CMAKE_DIR
                                       / "unsupported.cmake";
    return McuPackagePtr{new McuPackage(settingsHandler,
                                        {},
                                        toolchainFilePath,
                                        {},
                                        {},
                                        Constants::TOOLCHAIN_FILE_CMAKE_VARIABLE,
                                        {})};
}

McuToolChainPackagePtr createUnsupportedToolChainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    return McuToolChainPackagePtr{new McuToolChainPackage(
        settingsHandler, {}, {}, {}, {}, McuToolChainPackage::ToolChainType::Unsupported)};
}

McuToolChainPackagePtr createMsvcToolChainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    return McuToolChainPackagePtr{new McuToolChainPackage(settingsHandler,
                                                          {},
                                                          {},
                                                          {},
                                                          {},
                                                          McuToolChainPackage::ToolChainType::MSVC)};
}

McuToolChainPackagePtr createGccToolChainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    return McuToolChainPackagePtr{new McuToolChainPackage(settingsHandler,
                                                          {},
                                                          {},
                                                          {},
                                                          {},
                                                          McuToolChainPackage::ToolChainType::GCC)};
}

McuToolChainPackagePtr createArmGccToolchainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "ARMGCC_DIR";

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar))
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));
    if (defaultPath.isEmpty() && HostOsInfo::isWindowsHost()) {
        const FilePath installDir = findInProgramFiles("GNU Tools ARM Embedded");
        if (installDir.exists()) {
            // If GNU Tools installation dir has only one sub dir,
            // select the sub dir, otherwise the installation dir.
            const FilePaths subDirs = installDir.dirEntries(QDir::Dirs | QDir::NoDotAndDotDot);
            if (subDirs.count() == 1)
                defaultPath = subDirs.first();
        }
    }

    const Utils::FilePath detectionPath = FilePath("bin/arm-none-eabi-g++").withExecutableSuffix();
    const auto versionDetector
        = new McuPackageExecutableVersionDetector(detectionPath,
                                                  {"--version"},
                                                  "\\b(\\d+\\.\\d+\\.\\d+)\\b");

    return McuToolChainPackagePtr{
        new McuToolChainPackage(settingsHandler,
                                McuPackage::tr("GNU Arm Embedded Toolchain"),
                                defaultPath,
                                detectionPath,
                                "GNUArmEmbeddedToolchain",                  // settingsKey
                                McuToolChainPackage::ToolChainType::ArmGcc, // toolchainType
                                Constants::TOOLCHAIN_DIR_CMAKE_VARIABLE,    // cmake var
                                envVar,                                     // env var
                                versionDetector)};
}

McuToolChainPackagePtr createGhsToolchainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "GHS_COMPILER_DIR";

    const FilePath defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));

    const auto versionDetector
        = new McuPackageExecutableVersionDetector(FilePath("as850").withExecutableSuffix(),
                                                  {"-V"},
                                                  "\\bv(\\d+\\.\\d+\\.\\d+)\\b");

    return McuToolChainPackagePtr{
        new McuToolChainPackage(settingsHandler,
                                "Green Hills Compiler",
                                defaultPath,
                                FilePath("ccv850").withExecutableSuffix(), // detectionPath
                                "GHSToolchain",                            // settingsKey
                                McuToolChainPackage::ToolChainType::GHS,   // toolchainType
                                Constants::TOOLCHAIN_DIR_CMAKE_VARIABLE,   // cmake var
                                envVar,                                    // env var
                                versionDetector)};
}

McuToolChainPackagePtr createGhsArmToolchainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "GHS_ARM_COMPILER_DIR";

    const FilePath defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));

    const auto versionDetector
        = new McuPackageExecutableVersionDetector(FilePath("asarm").withExecutableSuffix(),
                                                  {"-V"},
                                                  "\\bv(\\d+\\.\\d+\\.\\d+)\\b");

    return McuToolChainPackagePtr{
        new McuToolChainPackage(settingsHandler,
                                "Green Hills Compiler for ARM",
                                defaultPath,
                                FilePath("cxarm").withExecutableSuffix(),   // detectionPath
                                "GHSArmToolchain",                          // settingsKey
                                McuToolChainPackage::ToolChainType::GHSArm, // toolchainType
                                Constants::TOOLCHAIN_DIR_CMAKE_VARIABLE,    // cmake var
                                envVar,                                     // env var
                                versionDetector)};
}

McuToolChainPackagePtr createIarToolChainPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "IAR_ARM_COMPILER_DIR";

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar))
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));
    else {
        const ProjectExplorer::ToolChain *tc = ProjectExplorer::ToolChainManager::toolChain(
            [](const ProjectExplorer::ToolChain *t) {
                return t->typeId() == BareMetal::Constants::IAREW_TOOLCHAIN_TYPEID;
            });
        if (tc) {
            const FilePath compilerExecPath = tc->compilerCommand();
            defaultPath = compilerExecPath.parentDir().parentDir();
        }
    }

    const FilePath detectionPath = FilePath("bin/iccarm").withExecutableSuffix();
    const auto versionDetector
        = new McuPackageExecutableVersionDetector(detectionPath,
                                                  {"--version"},
                                                  "\\bV(\\d+\\.\\d+\\.\\d+)\\.\\d+\\b");

    return McuToolChainPackagePtr{
        new McuToolChainPackage(settingsHandler,
                                "IAR ARM Compiler",
                                defaultPath,
                                detectionPath,
                                "IARToolchain",                          // settings key
                                McuToolChainPackage::ToolChainType::IAR, // toolchainType
                                Constants::TOOLCHAIN_DIR_CMAKE_VARIABLE, // cmake var
                                envVar,                                  // env var
                                versionDetector)};
}

static McuPackagePtr createStm32CubeProgrammerPackage(const SettingsHandler::Ptr &settingsHandler)
{
    FilePath defaultPath;
    const QString cubePath = "STMicroelectronics/STM32Cube/STM32CubeProgrammer";
    if (HostOsInfo::isWindowsHost()) {
        const FilePath programPath = findInProgramFiles(cubePath);
        if (!programPath.isEmpty())
            defaultPath = programPath;
    } else {
        const FilePath programPath = FileUtils::homePath() / cubePath;
        if (programPath.exists())
            defaultPath = programPath;
    }

    const FilePath detectionPath = FilePath::fromString(
        QLatin1String(Utils::HostOsInfo::isWindowsHost() ? "/bin/STM32_Programmer_CLI.exe"
                                                         : "/bin/STM32_Programmer.sh"));

    return McuPackagePtr{
        new McuPackage(settingsHandler,
                       McuPackage::tr("STM32CubeProgrammer"),
                       defaultPath,
                       detectionPath,
                       "Stm32CubeProgrammer",
                       {},                                                           // cmake var
                       {},                                                           // env var
                       "https://www.st.com/en/development-tools/stm32cubeprog.html", // download url
                       nullptr, // version detector
                       true,    // add to path
                       "/bin"   // relative path modifier
                       )};
}

static McuPackagePtr createMcuXpressoIdePackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "MCUXpressoIDE_PATH";

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar)) {
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));
    } else if (HostOsInfo::isWindowsHost()) {
        const FilePath programPath = FilePath::fromString(QDir::rootPath()) / "nxp";
        if (programPath.exists()) {
            defaultPath = programPath;
            // If default dir has exactly one sub dir that could be the IDE path, pre-select that.
            const FilePaths subDirs = defaultPath.dirEntries(
                {{"MCUXpressoIDE*"}, QDir::Dirs | QDir::NoDotAndDotDot});
            if (subDirs.count() == 1)
                defaultPath = subDirs.first();
        }
    } else {
        const FilePath programPath = FilePath::fromString("/usr/local/mcuxpressoide/");
        if (programPath.exists())
            defaultPath = programPath;
    }

    return McuPackagePtr{new McuPackage(settingsHandler,
                                        "MCUXpresso IDE",
                                        defaultPath,
                                        FilePath("ide/binaries/crt_emu_cm_redlink")
                                            .withExecutableSuffix(), // detection path
                                        "MCUXpressoIDE",             // settings key
                                        "MCUXPRESSO_IDE_PATH",       // cmake var
                                        envVar,
                                        "https://www.nxp.com/mcuxpresso/ide")}; // download url
}

static McuPackagePtr createCypressProgrammerPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "CYPRESS_AUTO_FLASH_UTILITY_DIR";

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar)) {
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));
    } else if (HostOsInfo::isWindowsHost()) {
        const FilePath candidate = findInProgramFiles("Cypress");
        if (candidate.exists()) {
            // "Cypress Auto Flash Utility 1.0"
            const auto subDirs = candidate.dirEntries({{"Cypress Auto Flash Utility*"}, QDir::Dirs},
                                                      QDir::Unsorted);
            if (!subDirs.empty())
                defaultPath = subDirs.first();
        }
    }

    return McuPackagePtr{new McuPackage(settingsHandler,
                                        "Cypress Auto Flash Utility",
                                        defaultPath,
                                        FilePath("/bin/openocd").withExecutableSuffix(),
                                        "CypressAutoFlashUtil",            // settings key
                                        "INFINEON_AUTO_FLASH_UTILITY_DIR", // cmake var
                                        envVar)};                          // env var
}

static McuPackagePtr createRenesasProgrammerPackage(const SettingsHandler::Ptr &settingsHandler)
{
    const char envVar[] = "RENESAS_FLASH_PROGRAMMER_PATH";

    FilePath defaultPath;
    if (qEnvironmentVariableIsSet(envVar)) {
        defaultPath = FilePath::fromUserInput(qEnvironmentVariable(envVar));
    } else if (HostOsInfo::isWindowsHost()) {
        const FilePath candidate = findInProgramFiles("Renesas Electronics/Programming Tools");
        if (candidate.exists()) {
            // "Renesas Flash Programmer V3.09"
            const auto subDirs = candidate.dirEntries({{"Renesas Flash Programmer*"}, QDir::Dirs},
                                                      QDir::Unsorted);
            if (!subDirs.empty())
                defaultPath = subDirs.first();
        }
    }

    return McuPackagePtr{new McuPackage(settingsHandler,
                                        "Renesas Flash Programmer",
                                        defaultPath,
                                        FilePath("rfp-cli").withExecutableSuffix(),
                                        "RenesasFlashProgrammer",        // settings key
                                        "RENESAS_FLASH_PROGRAMMER_PATH", // cmake var
                                        envVar)};                        // env var
}

static McuAbstractTargetFactory::Ptr createFactory(bool isLegacy,
                                                   const SettingsHandler::Ptr &settingsHandler,
                                                   const FilePath &qtMcuSdkPath)
{
    McuAbstractTargetFactory::Ptr result;
    if (isLegacy) {
        static const QHash<QString, ToolchainCompilerCreator> toolchainCreators = {
            {{"armgcc"},
             {[settingsHandler] { return createArmGccToolchainPackage(settingsHandler); }}},
            {{"greenhills"},
             [settingsHandler] { return createGhsToolchainPackage(settingsHandler); }},
            {{"iar"}, {[settingsHandler] { return createIarToolChainPackage(settingsHandler); }}},
            {{"msvc"}, {[settingsHandler] { return createMsvcToolChainPackage(settingsHandler); }}},
            {{"gcc"}, {[settingsHandler] { return createGccToolChainPackage(settingsHandler); }}},
            {{"arm-greenhills"},
             {[settingsHandler] { return createGhsArmToolchainPackage(settingsHandler); }}},
        };

        const FilePath toolchainFilePrefix = qtMcuSdkPath / Constants::QUL_TOOLCHAIN_CMAKE_DIR;
        static const QHash<QString, McuPackagePtr> toolchainFiles = {
            {{"armgcc"},
             McuPackagePtr{new McuPackage{settingsHandler,
                                          {},
                                          toolchainFilePrefix / "armgcc.cmake",
                                          {},
                                          {},
                                          Constants::TOOLCHAIN_FILE_CMAKE_VARIABLE,
                                          {}}}},

            {{"iar"},
             McuPackagePtr{new McuPackage{settingsHandler,
                                          {},
                                          toolchainFilePrefix / "iar.cmake",
                                          {},
                                          {},
                                          Constants::TOOLCHAIN_FILE_CMAKE_VARIABLE,
                                          {}}}},
            {"greenhills",
             McuPackagePtr{new McuPackage{settingsHandler,
                                          {},
                                          toolchainFilePrefix / "ghs.cmake",
                                          {},
                                          {},
                                          Constants::TOOLCHAIN_FILE_CMAKE_VARIABLE,
                                          {}}}},
            {"arm-greenhills",
             McuPackagePtr{new McuPackage{settingsHandler,
                                          {},
                                          toolchainFilePrefix / "arm-ghs.cmake",
                                          {},
                                          {},
                                          Constants::TOOLCHAIN_FILE_CMAKE_VARIABLE,
                                          {}}}},
        };

        // Note: the vendor name (the key of the hash) is case-sensitive. It has to match the "platformVendor" key in the
        // json file.
        static const QHash<QString, McuPackagePtr> vendorPkgs = {
            {{"ST"}, McuPackagePtr{createStm32CubeProgrammerPackage(settingsHandler)}},
            {{"NXP"}, McuPackagePtr{createMcuXpressoIdePackage(settingsHandler)}},
            {{"CYPRESS"}, McuPackagePtr{createCypressProgrammerPackage(settingsHandler)}},
            {{"RENESAS"}, McuPackagePtr{createRenesasProgrammerPackage(settingsHandler)}},
        };

        result = std::make_unique<McuTargetFactoryLegacy>(toolchainCreators,
                                                          toolchainFiles,
                                                          vendorPkgs,
                                                          settingsHandler);
    } else {
        result = std::make_unique<McuTargetFactory>(settingsHandler);
    }
    return result;
}

McuSdkRepository targetsFromDescriptions(const QList<McuTargetDescription> &descriptions,
                                         const SettingsHandler::Ptr &settingsHandler,
                                         const FilePath &qtForMCUSdkPath,
                                         bool isLegacy)
{
    Targets mcuTargets;
    Packages mcuPackages;

    McuAbstractTargetFactory::Ptr targetFactory = createFactory(isLegacy,
                                                                settingsHandler,
                                                                qtForMCUSdkPath);
    for (const McuTargetDescription &desc : descriptions) {
        auto [targets, packages] = targetFactory->createTargets(desc, qtForMCUSdkPath);
        mcuTargets.append(targets);
        mcuPackages.unite(packages);
    }

    if (isLegacy) {
        auto [toolchainPkgs, vendorPkgs]{targetFactory->getAdditionalPackages()};
        for (McuToolChainPackagePtr &package : toolchainPkgs) {
            mcuPackages.insert(package);
        }
        for (McuPackagePtr &package : vendorPkgs) {
            mcuPackages.insert(package);
        }
    }
    return McuSdkRepository{mcuTargets, mcuPackages};
}

Utils::FilePath kitsPath(const Utils::FilePath &qtMcuSdkPath)
{
    return qtMcuSdkPath / "kits/";
}

static QFileInfoList targetDescriptionFiles(const Utils::FilePath &dir)
{
    const QDir kitsDir(kitsPath(dir).toString(), "*.json");
    return kitsDir.entryInfoList();
}

static PackageDescription parsePackage(const QJsonObject &cmakeEntry)
{
    return {cmakeEntry["label"].toString(),
            cmakeEntry["envVar"].toString(),
            cmakeEntry["cmakeVar"].toString(),
            cmakeEntry["description"].toString(),
            cmakeEntry["setting"].toString(),
            FilePath::fromString(cmakeEntry["defaultValue"].toString()),
            FilePath::fromString(cmakeEntry["validation"].toString()),
            {},
            false};
}

static QList<PackageDescription> parsePackages(const QJsonArray &cmakeEntries)
{
    QList<PackageDescription> result;
    for (const auto &cmakeEntryRef : cmakeEntries) {
        const QJsonObject cmakeEntry{cmakeEntryRef.toObject()};
        result.push_back(parsePackage(cmakeEntry));
    }
    return result;
}

McuTargetDescription parseDescriptionJson(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    const QJsonObject target = document.object();
    const QString qulVersion = target.value("qulVersion").toString();
    const QJsonObject platform = target.value("platform").toObject();
    const QString compatVersion = target.value("compatVersion").toString();
    const QJsonObject toolchain = target.value("toolchain").toObject();
    const QJsonObject toolchainFile = toolchain.value("file").toObject();
    const QJsonObject compiler = toolchain.value("compiler").toObject();
    const QJsonObject boardSdk = target.value("boardSdk").toObject();
    const QJsonObject freeRTOS = target.value("freeRTOS").toObject();

    const PackageDescription toolchainPackage = parsePackage(compiler);
    const PackageDescription toolchainFilePackage = parsePackage(toolchainFile);
    const QList<PackageDescription> boardSDKEntries = parsePackages(
        boardSdk.value(CMAKE_ENTRIES).toArray());
    const QList<PackageDescription> freeRtosEntries = parsePackages(
        freeRTOS.value(CMAKE_ENTRIES).toArray());

    const QVariantList toolchainVersions = toolchain.value("versions").toArray().toVariantList();
    const auto toolchainVersionsList = Utils::transform<QStringList>(toolchainVersions,
                                                                     [&](const QVariant &version) {
                                                                         return version.toString();
                                                                     });
    const QVariantList boardSdkVersions = boardSdk.value("versions").toArray().toVariantList();
    const auto boardSdkVersionsList = Utils::transform<QStringList>(boardSdkVersions,
                                                                    [&](const QVariant &version) {
                                                                        return version.toString();
                                                                    });

    const QVariantList colorDepths = platform.value("colorDepths").toArray().toVariantList();
    const auto colorDepthsVector = Utils::transform<QVector<int>>(colorDepths,
                                                                  [&](const QVariant &colorDepth) {
                                                                      return colorDepth.toInt();
                                                                  });
    const QString platformName = platform.value("platformName").toString();

    return {qulVersion,
            compatVersion,
            {
                platform.value("id").toString(),
                platformName,
                platform.value("vendor").toString(),
                colorDepthsVector,
                platformName == "Desktop" ? McuTargetDescription::TargetType::Desktop
                                          : McuTargetDescription::TargetType::MCU,
            },
            {toolchain.value("id").toString(),
             toolchainVersionsList,
             toolchainPackage,
             toolchainFilePackage},
            {
                boardSdk.value("name").toString(),
                FilePath::fromString(boardSdk.value("defaultPath").toString()),
                boardSdk.value("envVar").toString(),
                boardSdkVersionsList,
                boardSDKEntries,
            },
            {
                freeRTOS.value("envVar").toString(),
                FilePath::fromString(freeRTOS.value("boardSdkSubDir").toString()),
                freeRtosEntries,
            }};
}

// https://doc.qt.io/qtcreator/creator-developing-mcu.html#supported-qt-for-mcus-sdks
static const QString legacySupportVersionFor(const QString &sdkVersion)
{
    static const QHash<QString, QString> oldSdkQtcRequiredVersion
        = {{{"1.0"}, {"4.11.x"}}, {{"1.1"}, {"4.12.0 or 4.12.1"}}, {{"1.2"}, {"4.12.2 or 4.12.3"}}};
    if (oldSdkQtcRequiredVersion.contains(sdkVersion))
        return oldSdkQtcRequiredVersion.value(sdkVersion);

    if (QVersionNumber::fromString(sdkVersion).majorVersion() == 1)
        return "4.12.4 up to 6.0";

    return QString();
}

bool checkDeprecatedSdkError(const Utils::FilePath &qulDir, QString &message)
{
    const McuPackagePathVersionDetector versionDetector("(?<=\\bQtMCUs.)(\\d+\\.\\d+)");
    const QString sdkDetectedVersion = versionDetector.parseVersion(qulDir);
    const QString legacyVersion = legacySupportVersionFor(sdkDetectedVersion);

    if (!legacyVersion.isEmpty()) {
        message = McuTarget::tr("Qt for MCUs SDK version %1 detected, "
                                "only supported by Qt Creator version %2. "
                                "This version of Qt Creator requires Qt for MCUs %3 or greater.")
                      .arg(sdkDetectedVersion,
                           legacyVersion,
                           McuSupportOptions::minimalQulVersion().toString());
        return true;
    }

    return false;
}

McuSdkRepository targetsAndPackages(const Utils::FilePath &qtForMCUSdkPath,
                                    const SettingsHandler::Ptr &settingsHandler)
{
    QList<McuTargetDescription> descriptions;
    bool isLegacy{false};

    auto descriptionFiles = targetDescriptionFiles(qtForMCUSdkPath);
    for (const QFileInfo &fileInfo : descriptionFiles) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QFile::ReadOnly))
            continue;
        const McuTargetDescription desc = parseDescriptionJson(file.readAll());
        const auto pth = Utils::FilePath::fromString(fileInfo.filePath());
        bool ok = false;
        const int compatVersion = desc.compatVersion.toInt(&ok);
        if (!desc.compatVersion.isEmpty() && ok && compatVersion > MAX_COMPATIBILITY_VERSION) {
            printMessage(McuTarget::tr("Skipped %1. Unsupported version \"%2\".")
                             .arg(QDir::toNativeSeparators(pth.fileNameWithPathComponents(1)),
                                  desc.qulVersion),
                         false);
            continue;
        }

        const auto qulVersion{QVersionNumber::fromString(desc.qulVersion)};
        isLegacy = McuSupportOptions::isLegacyVersion(qulVersion);

        if (qulVersion < McuSupportOptions::minimalQulVersion()) {
            const QString legacyVersion = legacySupportVersionFor(desc.qulVersion);
            const QString qtcSupportText
                = !legacyVersion.isEmpty()
                      ? McuTarget::tr("Detected version \"%1\", only supported by Qt Creator %2.")
                            .arg(desc.qulVersion, legacyVersion)
                      : McuTarget::tr("Unsupported version \"%1\".").arg(desc.qulVersion);
            printMessage(McuTarget::tr("Skipped %1. %2 Qt for MCUs version >= %3 required.")
                             .arg(QDir::toNativeSeparators(pth.fileNameWithPathComponents(1)),
                                  qtcSupportText,
                                  McuSupportOptions::minimalQulVersion().toString()),
                         false);
            continue;
        }
        descriptions.append(desc);
    }

    // No valid description means invalid or old SDK installation.
    if (descriptions.empty()) {
        if (kitsPath(qtForMCUSdkPath).exists()) {
            printMessage(McuTarget::tr("No valid kit descriptions found at %1.")
                             .arg(kitsPath(qtForMCUSdkPath).toUserOutput()),
                         true);
            return McuSdkRepository{};
        } else {
            QString deprecationMessage;
            if (checkDeprecatedSdkError(qtForMCUSdkPath, deprecationMessage)) {
                printMessage(deprecationMessage, true);
                return McuSdkRepository{};
            }
        }
    }
    McuSdkRepository repo = targetsFromDescriptions(descriptions,
                                                    settingsHandler,
                                                    qtForMCUSdkPath,
                                                    isLegacy);

    // Keep targets sorted lexicographically
    Utils::sort(repo.mcuTargets, [](const McuTargetPtr &lhs, const McuTargetPtr &rhs) {
        return McuKitManager::generateKitNameFromTarget(lhs.get())
               < McuKitManager::generateKitNameFromTarget(rhs.get());
    });
    return repo;
}

} // namespace Sdk
} // namespace Internal
} // namespace McuSupport
