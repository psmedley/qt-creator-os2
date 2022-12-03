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

#include "baseqtversion.h"
#include "qtconfigwidget.h"
#include "qtkitinformation.h"

#include "qtversionfactory.h"
#include "qtversionmanager.h"
#include "profilereader.h"

#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <proparser/qmakevfs.h>
#include <projectexplorer/deployablefile.h>
#include <projectexplorer/deploymentdata.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>
#include <qtsupport/qtkitinformation.h>
#include <qtsupport/qtsupportconstants.h>

#include <utils/algorithm.h>
#include <utils/buildablehelperlibrary.h>
#include <utils/displayname.h>
#include <utils/fileinprojectfinder.h>
#include <utils/hostosinfo.h>
#include <utils/macroexpander.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/runextensions.h>
#include <utils/stringutils.h>
#include <utils/winutils.h>

#include <resourceeditor/resourcenode.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

using namespace Core;
using namespace ProjectExplorer;
using namespace QtSupport::Internal;
using namespace Utils;

namespace QtSupport {
namespace Internal {

const char QTVERSIONAUTODETECTED[] = "isAutodetected";
const char QTVERSIONDETECTIONSOURCE[] = "autodetectionSource";
const char QTVERSION_OVERRIDE_FEATURES[] = "overrideFeatures";
const char QTVERSIONQMAKEPATH[] = "QMakePath";
const char QTVERSIONSOURCEPATH[] = "SourcePath";

const char QTVERSION_ABIS[] = "Abis";

const char MKSPEC_VALUE_LIBINFIX[] = "QT_LIBINFIX";
const char MKSPEC_VALUE_NAMESPACE[] = "QT_NAMESPACE";

// --------------------------------------------------------------------
// QtVersionData:
// --------------------------------------------------------------------

class QtVersionData
{
public:
    bool installed = true;
    bool hasExamples = false;
    bool hasDemos = false;
    bool hasDocumentation = false;
    bool hasQtAbis = false;

    DisplayName unexpandedDisplayName;
    QString qtVersionString;

    FilePath sourcePath;
    FilePath qtSources;

    Utils::FilePath prefix;

    Utils::FilePath binPath;
    Utils::FilePath libExecPath;
    Utils::FilePath configurationPath;
    Utils::FilePath dataPath;
    Utils::FilePath demosPath;
    Utils::FilePath docsPath;
    Utils::FilePath examplesPath;
    // Utils::FilePath frameworkPath; // is derived from libraryPath
    Utils::FilePath headerPath;
    Utils::FilePath importsPath;
    Utils::FilePath libraryPath;
    Utils::FilePath pluginPath;
    Utils::FilePath qmlPath;
    Utils::FilePath translationsPath;

    Utils::FilePath hostBinPath;
    Utils::FilePath hostLibexecPath;
    Utils::FilePath hostDataPath;
    Utils::FilePath hostPrefixPath;

    Abis qtAbis;
};

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

static QSet<Id> versionedIds(const QByteArray &prefix, int major, int minor)
{
    QSet<Id> result;
    result.insert(Id::fromName(prefix));

    if (major < 0)
        return result;

    const QByteArray majorStr = QString::number(major).toLatin1();
    const QByteArray featureMajor = prefix + majorStr;
    const QByteArray featureDotMajor = prefix + '.' + majorStr;

    result.insert(Id::fromName(featureMajor));
    result.insert(Id::fromName(featureDotMajor));

    for (int i = 0; i <= minor; ++i) {
        const QByteArray minorStr = QString::number(i).toLatin1();
        result.insert(Id::fromName(featureMajor + '.' + minorStr));
        result.insert(Id::fromName(featureDotMajor + '.' + minorStr));
    }

    // FIXME: Terrible hack. Get rid of using version numbers as tags!
    if (major > 5)
        result.unite(versionedIds(prefix, major - 1, 15));

    return result;
}

// Wrapper to make the std::unique_ptr<Utils::MacroExpander> "copyable":
class MacroExpanderWrapper
{
public:
    MacroExpanderWrapper() = default;
    MacroExpanderWrapper(const MacroExpanderWrapper &other) { Q_UNUSED(other) }
    MacroExpanderWrapper(MacroExpanderWrapper &&other) = default;

    MacroExpander *macroExpander(const QtVersion *qtversion) const;
private:
    mutable std::unique_ptr<MacroExpander> m_expander;
};

enum HostBinaries { Designer, Linguist, Rcc, Uic, QScxmlc };

class QtVersionPrivate
{
public:
    QtVersionPrivate(QtVersion *parent)
        : q(parent)
    {}

    void updateVersionInfo();

    FilePath findHostBinary(HostBinaries binary) const;
    void updateMkspec();
    QHash<ProKey, ProString> versionInfo();
    static bool queryQMakeVariables(const FilePath &binary,
                                    const Environment &env,
                                    QHash<ProKey, ProString> *versionInfo,
                                    QString *error);
    enum PropertyVariant { PropertyVariantDev, PropertyVariantGet, PropertyVariantSrc };
    QString qmakeProperty(const QByteArray &name, PropertyVariant variant = PropertyVariantGet);
    static QString qmakeProperty(const QHash<ProKey, ProString> &versionInfo,
                                 const QByteArray &name,
                                 PropertyVariant variant = PropertyVariantGet);
    static FilePath mkspecDirectoryFromVersionInfo(const QHash<ProKey, ProString> &versionInfo,
                                                   const FilePath &qmakeCommand);
    static FilePath mkspecFromVersionInfo(const QHash<ProKey,ProString> &versionInfo,
                                          const FilePath &qmakeCommand);
    static FilePath sourcePath(const QHash<ProKey,ProString> &versionInfo);
    void setId(int id); // used by the qtversionmanager for legacy restore
                        // and by the qtoptionspage to replace Qt versions

    FilePaths qtCorePaths();

public:
    QtVersion *q;
    int m_id = -1;
    bool m_isAutodetected = false;
    QString m_type;

    QtVersionData m_data;

    bool m_isUpdating = false;
    bool m_mkspecUpToDate = false;
    bool m_mkspecReadUpToDate = false;
    bool m_defaultConfigIsDebug = true;
    bool m_defaultConfigIsDebugAndRelease = true;
    bool m_frameworkBuild = false;
    bool m_versionInfoUpToDate = false;
    bool m_qmakeIsExecutable = true;

    QString m_detectionSource;
    QSet<Utils::Id> m_overrideFeatures;

    FilePath m_mkspec;
    FilePath m_mkspecFullPath;

    QHash<QString, QString> m_mkspecValues;

    QHash<ProKey, ProString> m_versionInfo;

    FilePath m_qmakeCommand;

    FilePath m_rccPath;
    FilePath m_uicPath;
    FilePath m_designerPath;
    FilePath m_linguistPath;
    FilePath m_qscxmlcPath;
    FilePath m_qmlRuntimePath;
    FilePath m_qmlplugindumpPath;

    MacroExpanderWrapper m_expander;
};

///////////////
// MacroExpanderWrapper
///////////////
MacroExpander *MacroExpanderWrapper::macroExpander(const QtVersion *qtversion) const
{
    if (!m_expander)
        m_expander = QtVersion::createMacroExpander([qtversion]() { return qtversion; });
    return m_expander.get();
}

} // Internal

///////////////
// QtVersionNumber
///////////////
QtVersionNumber::QtVersionNumber(int ma, int mi, int p)
    : majorVersion(ma), minorVersion(mi), patchVersion(p)
{ }

QtVersionNumber::QtVersionNumber(const QString &versionString)
{
    if (::sscanf(versionString.toLatin1().constData(), "%d.%d.%d",
           &majorVersion, &minorVersion, &patchVersion) != 3)
        majorVersion = minorVersion = patchVersion = -1;
}

QSet<Id> QtVersionNumber::features() const
{
    return versionedIds(Constants::FEATURE_QT_PREFIX, majorVersion, minorVersion);
}

bool QtVersionNumber::matches(int major, int minor, int patch) const
{
    if (major < 0)
        return true;
    if (major != majorVersion)
        return false;

    if (minor < 0)
        return true;
    if (minor != minorVersion)
        return false;

    if (patch < 0)
        return true;
    return (patch == patchVersion);
}

bool QtVersionNumber::operator <(const QtVersionNumber &b) const
{
    if (majorVersion != b.majorVersion)
        return majorVersion < b.majorVersion;
    if (minorVersion != b.minorVersion)
        return minorVersion < b.minorVersion;
    return patchVersion < b.patchVersion;
}

bool QtVersionNumber::operator >(const QtVersionNumber &b) const
{
    return b < *this;
}

bool QtVersionNumber::operator ==(const QtVersionNumber &b) const
{
    return majorVersion == b.majorVersion
            && minorVersion == b.minorVersion
            && patchVersion == b.patchVersion;
}

bool QtVersionNumber::operator !=(const QtVersionNumber &b) const
{
    return !(*this == b);
}

bool QtVersionNumber::operator <=(const QtVersionNumber &b) const
{
    return !(*this > b);
}

bool QtVersionNumber::operator >=(const QtVersionNumber &b) const
{
    return b <= *this;
}

///////////////
// QtVersion
///////////////

QtVersion::QtVersion()
    : d(new QtVersionPrivate(this))
{}

QtVersion::~QtVersion()
{
    delete d;
}

QString QtVersion::defaultUnexpandedDisplayName() const
{
    QString location;
    if (qmakeFilePath().isEmpty()) {
        location = QCoreApplication::translate("QtVersion", "<unknown>");
    } else {
        // Deduce a description from '/foo/qt-folder/[qtbase]/bin/qmake' -> '/foo/qt-folder'.
        // '/usr' indicates System Qt 4.X on Linux.
        for (FilePath dir = qmakeFilePath().parentDir(); !dir.isEmpty(); dir = dir.parentDir()) {
            const QString dirName = dir.fileName();
            if (dirName == "usr") { // System-installed Qt.
                location = QCoreApplication::translate("QtVersion", "System");
                break;
            }
            location = dirName;
            // Also skip default checkouts named 'qt'. Parent dir might have descriptive name.
            if (dirName.compare("bin", Qt::CaseInsensitive)
                && dirName.compare("qtbase", Qt::CaseInsensitive)
                && dirName.compare("qt", Qt::CaseInsensitive)) {
                break;
            }
        }
    }

    return detectionSource() == "PATH" ?
        QCoreApplication::translate("QtVersion", "Qt %{Qt:Version} in PATH (%2)").arg(location) :
        QCoreApplication::translate("QtVersion", "Qt %{Qt:Version} (%2)").arg(location);
}

QSet<Id> QtVersion::availableFeatures() const
{
    QSet<Id> features = qtVersion().features(); // Qt Version features

    features.insert(Constants::FEATURE_QWIDGETS);
    features.insert(Constants::FEATURE_QT_WEBKIT);
    features.insert(Constants::FEATURE_QT_CONSOLE);

    if (qtVersion() < QtVersionNumber(4, 7, 0))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 1, 0));

    if (qtVersion().matches(4, 7, 0))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 1, 1));

    if (qtVersion().matches(4))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 0));

    if (qtVersion().matches(5, 0))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 1));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 0));

    if (qtVersion().matches(5, 1))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 2));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 1));

    if (qtVersion().matches(5, 2))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 3));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 2));

    if (qtVersion().matches(5, 3))
        return features;

    features.insert(Constants::FEATURE_QT_QUICK_UI_FILES);

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 4));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 3));

    if (qtVersion().matches(5, 4))
        return features;

    features.insert(Constants::FEATURE_QT_3D);

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 5));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 4));
    features.unite(versionedIds(Constants::FEATURE_QT_CANVAS3D_PREFIX, 1, 0));

    if (qtVersion().matches(5, 5))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 6));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_PREFIX, 1, 5));
    features.unite(versionedIds(Constants::FEATURE_QT_LABS_CONTROLS_PREFIX, 1, 0));
    features.unite(versionedIds(Constants::FEATURE_QT_CANVAS3D_PREFIX, 1, 1));

    if (qtVersion().matches(5, 6))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 7));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 0));
    features.subtract(versionedIds(Constants::FEATURE_QT_LABS_CONTROLS_PREFIX, 1, 0));

    if (qtVersion().matches(5, 7))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 8));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 1));

    if (qtVersion().matches(5, 8))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 9));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 2));

    if (qtVersion().matches(5, 9))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 10));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 3));

    if (qtVersion().matches(5, 10))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 11));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 4));

    if (qtVersion().matches(5, 11))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 12));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 5));

    if (qtVersion().matches(5, 12))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 13));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 13));

    if (qtVersion().matches(5, 13))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 14));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 14));

    if (qtVersion().matches(5, 14))
        return features;

    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 2, 15));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 2, 15));

    if (qtVersion().matches(5, 15))
        return features;

    // Qt 6 uses versionless imports
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_PREFIX, 6, -1));
    features.unite(versionedIds(Constants::FEATURE_QT_QUICK_CONTROLS_2_PREFIX, 6, -1));

    return features;
}

Tasks QtVersion::validateKit(const Kit *k)
{
    Tasks result;

    QtVersion *version = QtKitAspect::qtVersion(k);
    Q_ASSERT(version == this);

    const Abis qtAbis = version->qtAbis();
    if (qtAbis.isEmpty()) // No need to test if Qt does not know anyway...
        return result;

    const Id dt = DeviceTypeKitAspect::deviceTypeId(k);
    if (dt != "DockerDeviceType") {
        const QSet<Id> tdt = targetDeviceTypes();
        if (!tdt.isEmpty() && !tdt.contains(dt))
            result << BuildSystemTask(Task::Warning, tr("Device type is not supported by Qt version."));
    }

    if (ToolChain *tc = ToolChainKitAspect::cxxToolChain(k)) {
        Abi targetAbi = tc->targetAbi();
        Abis supportedAbis = tc->supportedAbis();
        bool fuzzyMatch = false;
        bool fullMatch = false;

        QString qtAbiString;
        for (const Abi &qtAbi : qtAbis) {
            if (!qtAbiString.isEmpty())
                qtAbiString.append(' ');
            qtAbiString.append(qtAbi.toString());

            if (!fullMatch) {
                fullMatch = supportedAbis.contains(qtAbi)
                            && qtAbi.wordWidth() == targetAbi.wordWidth()
                            && qtAbi.architecture() == targetAbi.architecture();
            }
            if (!fuzzyMatch && !fullMatch) {
                fuzzyMatch = Utils::anyOf(supportedAbis, [&](const Abi &abi) {
                    return qtAbi.isCompatibleWith(abi);
                });
            }
        }

        QString message;
        if (!fullMatch) {
            if (!fuzzyMatch)
                message = tr("The compiler \"%1\" (%2) cannot produce code for the Qt version \"%3\" (%4).");
            else
                message = tr("The compiler \"%1\" (%2) may not produce code compatible with the Qt version \"%3\" (%4).");
            message = message.arg(tc->displayName(), targetAbi.toString(),
                                  version->displayName(), qtAbiString);
            result << BuildSystemTask(fuzzyMatch ? Task::Warning : Task::Error, message);
        }
    } else if (ToolChainKitAspect::cToolChain(k)) {
        const QString message = tr("The kit has a Qt version, but no C++ compiler.");
        result << BuildSystemTask(Task::Warning, message);
    }
    return result;
}

FilePath QtVersion::prefix() const // QT_INSTALL_PREFIX
{
    d->updateVersionInfo();
    return d->m_data.prefix;
}

FilePath QtVersion::binPath() const // QT_INSTALL_BINS
{
    d->updateVersionInfo();
    return d->m_data.binPath;
}

FilePath QtVersion::libExecPath() const // QT_INSTALL_LIBEXECS
{
    d->updateVersionInfo();
    return d->m_data.libExecPath;
}
FilePath QtVersion::configurationPath() const // QT_INSTALL_CONFIGURATION
{
    d->updateVersionInfo();
    return d->m_data.configurationPath;
}

FilePath QtVersion::headerPath() const // QT_INSTALL_HEADERS
{
    d->updateVersionInfo();
    return d->m_data.headerPath;
}

FilePath QtVersion::dataPath() const // QT_INSTALL_DATA
{
    d->updateVersionInfo();
    return d->m_data.dataPath;
}

FilePath QtVersion::docsPath() const // QT_INSTALL_DOCS
{
    d->updateVersionInfo();
    return d->m_data.docsPath;
}

FilePath QtVersion::importsPath() const // QT_INSTALL_IMPORTS
{
    d->updateVersionInfo();
    return d->m_data.importsPath;
}

FilePath QtVersion::libraryPath() const // QT_INSTALL_LIBS
{
    d->updateVersionInfo();
    return d->m_data.libraryPath;
}

FilePath QtVersion::pluginPath() const // QT_INSTALL_PLUGINS
{
    d->updateVersionInfo();
    return d->m_data.pluginPath;
}

FilePath QtVersion::qmlPath() const // QT_INSTALL_QML
{
    d->updateVersionInfo();
    return d->m_data.qmlPath;
}

FilePath QtVersion::translationsPath() const // QT_INSTALL_TRANSLATIONS
{
    d->updateVersionInfo();
    return d->m_data.translationsPath;
}

FilePath QtVersion::hostBinPath() const // QT_HOST_BINS
{
    d->updateVersionInfo();
    return d->m_data.hostBinPath;
}

FilePath QtVersion::hostLibexecPath() const // QT_HOST_LIBEXECS
{
    d->updateVersionInfo();
    return d->m_data.hostLibexecPath;
}

FilePath QtVersion::hostDataPath() const // QT_HOST_DATA
{
    d->updateVersionInfo();
    return d->m_data.hostDataPath;
}

FilePath QtVersion::hostPrefixPath() const  // QT_HOST_PREFIX
{
    d->updateVersionInfo();
    return d->m_data.hostPrefixPath;
}

FilePath QtVersion::mkspecsPath() const
{
    const FilePath result = hostDataPath();
    if (result.isEmpty())
        return FilePath::fromUserInput(QtVersionPrivate::qmakeProperty(
                                           d->m_versionInfo, "QMAKE_MKSPECS"));
    return result.pathAppended("mkspecs");
}

FilePath QtVersion::librarySearchPath() const
{
    return HostOsInfo::isWindowsHost() ? binPath() : libraryPath();
}

FilePaths QtVersion::directoriesToIgnoreInProjectTree() const
{
    FilePaths result;
    const FilePath mkspecPathGet = mkspecsPath();
    result.append(mkspecPathGet);

    FilePath mkspecPathSrc = FilePath::fromUserInput(
        d->qmakeProperty("QT_HOST_DATA", QtVersionPrivate::PropertyVariantSrc));
    if (!mkspecPathSrc.isEmpty()) {
        mkspecPathSrc = mkspecPathSrc.pathAppended("mkspecs");
        if (mkspecPathSrc != mkspecPathGet)
            result.append(mkspecPathSrc);
    }

    return result;
}

QString QtVersion::qtNamespace() const
{
    ensureMkSpecParsed();
    return d->m_mkspecValues.value(MKSPEC_VALUE_NAMESPACE);
}

QString QtVersion::qtLibInfix() const
{
    ensureMkSpecParsed();
    return d->m_mkspecValues.value(MKSPEC_VALUE_LIBINFIX);
}

bool QtVersion::isFrameworkBuild() const
{
    ensureMkSpecParsed();
    return d->m_frameworkBuild;
}

bool QtVersion::hasDebugBuild() const
{
    return d->m_defaultConfigIsDebug || d->m_defaultConfigIsDebugAndRelease;
}

bool QtVersion::hasReleaseBuild() const
{
    return !d->m_defaultConfigIsDebug || d->m_defaultConfigIsDebugAndRelease;
}

void QtVersion::fromMap(const QVariantMap &map)
{
    d->m_id = map.value(Constants::QTVERSIONID).toInt();
    if (d->m_id == -1) // this happens on adding from installer, see updateFromInstaller => get a new unique id
        d->m_id = QtVersionManager::getUniqueId();
    d->m_data.unexpandedDisplayName.fromMap(map, Constants::QTVERSIONNAME);
    d->m_isAutodetected = map.value(QTVERSIONAUTODETECTED).toBool();
    d->m_detectionSource = map.value(QTVERSIONDETECTIONSOURCE).toString();
    d->m_overrideFeatures = Utils::Id::fromStringList(map.value(QTVERSION_OVERRIDE_FEATURES).toStringList());
    d->m_qmakeCommand = FilePath::fromVariant(map.value(QTVERSIONQMAKEPATH));

    FilePath qmake = d->m_qmakeCommand;
    // FIXME: Check this is still needed or whether ProcessArgs::splitArg handles it.
    QString string = d->m_qmakeCommand.path();
    if (string.startsWith('~'))
        string.remove(0, 1).prepend(QDir::homePath());
    qmake.setPath(string);
    if (!d->m_qmakeCommand.needsDevice()) {
        if (BuildableHelperLibrary::isQtChooser(qmake)) {
            // we don't want to treat qtchooser as a normal qmake
            // see e.g. QTCREATORBUG-9841, also this lead to users changing what
            // qtchooser forwards too behind our backs, which will inadvertly lead to bugs
            d->m_qmakeCommand = BuildableHelperLibrary::qtChooserToQmakePath(qmake);
        }
    }

    d->m_data.qtSources = FilePath::fromVariant(map.value(QTVERSIONSOURCEPATH));

    // Handle ABIs provided by the SDKTool:
    // Note: Creator does not write these settings itself, so it has to come from the SDKTool!
    d->m_data.qtAbis = Utils::transform<Abis>(map.value(QTVERSION_ABIS).toStringList(),
                                              &Abi::fromString);
    d->m_data.qtAbis = Utils::filtered(d->m_data.qtAbis, &Abi::isValid);
    d->m_data.hasQtAbis = !d->m_data.qtAbis.isEmpty();

    updateDefaultDisplayName();

    // Clear the cached qmlscene command, it might not match the restored path anymore.
    d->m_qmlRuntimePath.clear();
}

QVariantMap QtVersion::toMap() const
{
    QVariantMap result;
    result.insert(Constants::QTVERSIONID, uniqueId());
    d->m_data.unexpandedDisplayName.toMap(result, Constants::QTVERSIONNAME);

    result.insert(QTVERSIONAUTODETECTED, isAutodetected());
    result.insert(QTVERSIONDETECTIONSOURCE, detectionSource());
    if (!d->m_overrideFeatures.isEmpty())
        result.insert(QTVERSION_OVERRIDE_FEATURES, Utils::Id::toStringList(d->m_overrideFeatures));

    result.insert(QTVERSIONQMAKEPATH, qmakeFilePath().toVariant());
    return result;
}

bool QtVersion::isValid() const
{
    if (uniqueId() == -1 || displayName().isEmpty())
        return false;
    d->updateVersionInfo();
    d->updateMkspec();

    return !qmakeFilePath().isEmpty() && d->m_data.installed && !binPath().isEmpty()
           && !d->m_mkspecFullPath.isEmpty() && d->m_qmakeIsExecutable;
}

QtVersion::Predicate QtVersion::isValidPredicate(const QtVersion::Predicate &predicate)
{
    if (predicate)
        return [predicate](const QtVersion *v) { return v->isValid() && predicate(v); };
    return [](const QtVersion *v) { return v->isValid(); };
}

QString QtVersion::invalidReason() const
{
    if (displayName().isEmpty())
        return QCoreApplication::translate("QtVersion", "Qt version has no name");
    if (qmakeFilePath().isEmpty())
        return QCoreApplication::translate("QtVersion", "No qmake path set");
    if (!d->m_qmakeIsExecutable)
        return QCoreApplication::translate("QtVersion", "qmake does not exist or is not executable");
    if (!d->m_data.installed)
        return QCoreApplication::translate("QtVersion", "Qt version is not properly installed, please run make install");
    if (binPath().isEmpty())
        return QCoreApplication::translate("QtVersion",
                                           "Could not determine the path to the binaries of the Qt installation, maybe the qmake path is wrong?");
    if (d->m_mkspecUpToDate && d->m_mkspecFullPath.isEmpty())
        return QCoreApplication::translate("QtVersion", "The default mkspec symlink is broken.");
    return QString();
}

QStringList QtVersion::warningReason() const
{
    QStringList ret;
    if (qtAbis().isEmpty())
        ret << QCoreApplication::translate("QtVersion", "ABI detection failed: Make sure to use a matching compiler when building.");
    if (d->m_versionInfo.value(ProKey("QT_INSTALL_PREFIX/get"))
            != d->m_versionInfo.value(ProKey("QT_INSTALL_PREFIX"))) {
        ret << QCoreApplication::translate("QtVersion", "Non-installed -prefix build - for internal development only.");
    }
    return ret;
}

FilePath QtVersion::qmakeFilePath() const
{
    return d->m_qmakeCommand;
}

Abis QtVersion::qtAbis() const
{
    if (!d->m_data.hasQtAbis) {
        d->m_data.qtAbis = detectQtAbis();
        d->m_data.hasQtAbis = true;
    }
    return d->m_data.qtAbis;
}

Abis QtVersion::detectQtAbis() const
{
    return qtAbisFromLibrary(d->qtCorePaths());
}

bool QtVersion::hasAbi(ProjectExplorer::Abi::OS os, ProjectExplorer::Abi::OSFlavor flavor) const
{
    const Abis abis = qtAbis();
    return Utils::anyOf(abis, [&](const Abi &abi) {
        if (abi.os() != os)
            return false;

        if (flavor == Abi::UnknownFlavor)
            return true;

        return abi.osFlavor() == flavor;
    });
}

bool QtVersion::equals(QtVersion *other)
{
    if (d->m_qmakeCommand != other->d->m_qmakeCommand)
        return false;
    if (type() != other->type())
        return false;
    if (uniqueId() != other->uniqueId())
        return false;
    if (displayName() != other->displayName())
        return false;
    if (isValid() != other->isValid())
        return false;

    return true;
}

int QtVersion::uniqueId() const
{
    return d->m_id;
}

QString QtVersion::type() const
{
    return d->m_type;
}

bool QtVersion::isAutodetected() const
{
    return d->m_isAutodetected;
}

QString QtVersion::detectionSource() const
{
    return d->m_detectionSource;
}

QString QtVersion::displayName() const
{
    return macroExpander()->expand(unexpandedDisplayName());
}

QString QtVersion::unexpandedDisplayName() const
{
    return d->m_data.unexpandedDisplayName.value();
}

void QtVersion::setUnexpandedDisplayName(const QString &name)
{
    d->m_data.unexpandedDisplayName.setValue(name);
}

void QtVersion::updateDefaultDisplayName()
{
    d->m_data.unexpandedDisplayName.setDefaultValue(defaultUnexpandedDisplayName());
}

QString QtVersion::toHtml(bool verbose) const
{
    QString rc;
    QTextStream str(&rc);
    str << "<html><body><table>";
    str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "Name:")
        << "</b></td><td>" << displayName() << "</td></tr>";
    if (!isValid()) {
        str << "<tr><td colspan=2><b>"
            << QCoreApplication::translate("QtVersion", "Invalid Qt version")
            << "</b></td></tr>";
    } else {
        str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "ABI:")
            << "</b></td>";
        const Abis abis = qtAbis();
        if (abis.isEmpty()) {
            str << "<td>" << Abi().toString() << "</td></tr>";
        } else {
            for (int i = 0; i < abis.size(); ++i) {
                if (i)
                    str << "<tr><td></td>";
                str << "<td>" << abis.at(i).toString() << "</td></tr>";
            }
        }
        const OsType osType = d->m_qmakeCommand.osType();
        str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "Source:")
            << "</b></td><td>" << sourcePath().toUserOutput() << "</td></tr>";
        str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "mkspec:")
            << "</b></td><td>" << QDir::toNativeSeparators(mkspec()) << "</td></tr>";
        str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "qmake:")
            << "</b></td><td>" << d->m_qmakeCommand.toUserOutput() << "</td></tr>";
        ensureMkSpecParsed();
        if (!mkspecPath().isEmpty()) {
            if (d->m_defaultConfigIsDebug || d->m_defaultConfigIsDebugAndRelease) {
                str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "Default:") << "</b></td><td>"
                    << (d->m_defaultConfigIsDebug ? "debug" : "release");
                if (d->m_defaultConfigIsDebugAndRelease)
                    str << " debug_and_release";
                str << "</td></tr>";
            } // default config.
        }
        str << "<tr><td><b>" << QCoreApplication::translate("QtVersion", "Version:")
            << "</b></td><td>" << qtVersionString() << "</td></tr>";
        if (verbose) {
            const QHash<ProKey, ProString> vInfo = d->versionInfo();
            if (!vInfo.isEmpty()) {
                QList<ProKey> keys = vInfo.keys();
                Utils::sort(keys);
                foreach (const ProKey &key, keys) {
                    const QString &value = vInfo.value(key).toQString();
                    QString variableName = key.toQString();
                    if (variableName != "QMAKE_MKSPECS"
                        && !variableName.endsWith("/raw")) {
                        bool isPath = false;
                        if (variableName.contains("_HOST_")
                            || variableName.contains("_INSTALL_")) {
                            if (!variableName.endsWith("/get"))
                                continue;
                            variableName.chop(4);
                            isPath = true;
                        } else if (variableName == "QT_SYSROOT") {
                            isPath = true;
                        }
                        str << "<tr><td><pre>" << variableName <<  "</pre></td><td>";
                        if (value.isEmpty())
                            isPath = false;
                        if (isPath) {
                            str << "<a href=\"" << QUrl::fromLocalFile(value).toString()
                                << "\">" << OsSpecificAspects::pathWithNativeSeparators(osType, value) << "</a>";
                        } else {
                            str << value;
                        }
                        str << "</td></tr>";
                    }
                }
            }
        }
    }
    str << "</table></body></html>";
    return rc;
}

FilePath QtVersion::sourcePath() const
{
    if (d->m_data.sourcePath.isEmpty()) {
        d->updateVersionInfo();
        d->m_data.sourcePath = QtVersionPrivate::sourcePath(d->m_versionInfo);
    }
    return d->m_data.sourcePath;
}

FilePath QtVersion::qtPackageSourcePath() const
{
    return d->m_data.qtSources;
}

FilePath QtVersion::designerFilePath() const
{
    if (!isValid())
        return {};
    if (d->m_designerPath.isEmpty())
        d->m_designerPath = d->findHostBinary(Designer);
    return d->m_designerPath;
}

FilePath QtVersion::linguistFilePath() const
{
    if (!isValid())
        return {};
    if (d->m_linguistPath.isEmpty())
        d->m_linguistPath = d->findHostBinary(Linguist);
    return d->m_linguistPath;
}

FilePath QtVersion::qscxmlcFilePath() const
{
    if (!isValid())
        return {};

    if (d->m_qscxmlcPath.isEmpty())
        d->m_qscxmlcPath = d->findHostBinary(QScxmlc);
    return d->m_qscxmlcPath;
}

FilePath QtVersion::qmlRuntimeFilePath() const
{
    if (!isValid())
        return {};

    if (!d->m_qmlRuntimePath.isEmpty())
        return d->m_qmlRuntimePath;

    FilePath path = binPath();
    if (qtVersion() >= QtVersionNumber(6, 2, 0))
        path = path.pathAppended("qml").withExecutableSuffix();
    else
        path = path.pathAppended("qmlscene").withExecutableSuffix();

    d->m_qmlRuntimePath = path.isExecutableFile() ? path : FilePath();

    return d->m_qmlRuntimePath;
}

FilePath QtVersion::qmlplugindumpFilePath() const
{
    if (!isValid())
        return {};

    if (!d->m_qmlplugindumpPath.isEmpty())
        return d->m_qmlplugindumpPath;

    const FilePath path = binPath().pathAppended("qmlplugindump").withExecutableSuffix();
    d->m_qmlplugindumpPath = path.isExecutableFile() ? path : FilePath();

    return d->m_qmlplugindumpPath;
}

FilePath QtVersionPrivate::findHostBinary(HostBinaries binary) const
{
    FilePath baseDir;
    if (q->qtVersion() < QtVersionNumber(5, 0, 0)) {
        baseDir = q->binPath();
    } else {
        switch (binary) {
        case Designer:
        case Linguist:
        case QScxmlc:
            baseDir = q->hostBinPath();
            break;
        case Rcc:
        case Uic:
            if (q->qtVersion() >= QtVersionNumber(6, 1))
                baseDir = q->hostLibexecPath();
            else
                baseDir = q->hostBinPath();
            break;
        default:
            // Can't happen
            Q_ASSERT(false);
        }
    }

    if (baseDir.isEmpty())
        return {};

    QStringList possibleCommands;
    switch (binary) {
    case Designer:
        if (HostOsInfo::isMacHost())
            possibleCommands << "Designer.app/Contents/MacOS/Designer";
        else
            possibleCommands << HostOsInfo::withExecutableSuffix("designer");
        break;
    case Linguist:
        if (HostOsInfo::isMacHost())
            possibleCommands << "Linguist.app/Contents/MacOS/Linguist";
        else
            possibleCommands << HostOsInfo::withExecutableSuffix("linguist");
        break;
    case Rcc:
        if (HostOsInfo::isWindowsHost()) {
            possibleCommands << "rcc.exe";
        } else {
            const QString majorString = QString::number(q->qtVersion().majorVersion);
            possibleCommands << ("rcc-qt" + majorString) << ("rcc" + majorString) << "rcc";
        }
        break;
    case Uic:
        if (HostOsInfo::isWindowsHost()) {
            possibleCommands << "uic.exe";
        } else {
            const QString majorString = QString::number(q->qtVersion().majorVersion);
            possibleCommands << ("uic-qt" + majorString) << ("uic" + majorString) << "uic";
        }
        break;
    case QScxmlc:
        possibleCommands << HostOsInfo::withExecutableSuffix("qscxmlc");
        break;
    default:
        Q_ASSERT(false);
    }
    for (const QString &possibleCommand : qAsConst(possibleCommands)) {
        const FilePath fullPath = baseDir / possibleCommand;
        if (fullPath.isExecutableFile())
            return fullPath;
    }
    return {};
}

FilePath QtVersion::rccFilePath() const
{
    if (!isValid())
        return {};
    if (!d->m_rccPath.isEmpty())
        return d->m_rccPath;
    d->m_rccPath = d->findHostBinary(Rcc);
    return d->m_rccPath;
}

FilePath QtVersion::uicFilePath() const
{
    if (!isValid())
        return {};
    if (!d->m_uicPath.isEmpty())
        return d->m_uicPath;
    d->m_uicPath = d->findHostBinary(Uic);
    return d->m_uicPath;
}

void QtVersionPrivate::updateMkspec()
{
    if (m_id == -1 || m_mkspecUpToDate)
        return;

    m_mkspecUpToDate = true;
    m_mkspecFullPath = mkspecFromVersionInfo(versionInfo(), m_qmakeCommand);

    m_mkspec = m_mkspecFullPath;
    if (m_mkspecFullPath.isEmpty())
        return;

    FilePath baseMkspecDir = mkspecDirectoryFromVersionInfo(versionInfo(), m_qmakeCommand);

    if (m_mkspec.isChildOf(baseMkspecDir)) {
        m_mkspec = m_mkspec.relativeChildPath(baseMkspecDir);
//        qDebug() << "Setting mkspec to"<<mkspec;
    } else {
        const FilePath sourceMkSpecPath = q->sourcePath().pathAppended("mkspecs");
        if (m_mkspec.isChildOf(sourceMkSpecPath)) {
            m_mkspec = m_mkspec.relativeChildPath(sourceMkSpecPath);
        } else {
            // Do nothing
        }
    }
}

void QtVersion::ensureMkSpecParsed() const
{
    if (d->m_mkspecReadUpToDate)
        return;
    d->m_mkspecReadUpToDate = true;

    if (mkspecPath().isEmpty())
        return;

    QMakeVfs vfs;
    QMakeGlobals option;
    applyProperties(&option);
    Environment env = d->m_qmakeCommand.deviceEnvironment();
    setupQmakeRunEnvironment(env);
    option.environment = env.toProcessEnvironment();
    ProMessageHandler msgHandler(true);
    ProFileCacheManager::instance()->incRefCount();
    QMakeParser parser(ProFileCacheManager::instance()->cache(), &vfs, &msgHandler);
    ProFileEvaluator evaluator(&option, &parser, &vfs, &msgHandler);
    // FIXME: toString() would be better, but the pro parser Q_ASSERTs on anything
    // non-local.
    evaluator.loadNamedSpec(mkspecPath().path(), false);

    parseMkSpec(&evaluator);

    ProFileCacheManager::instance()->decRefCount();
}

void QtVersion::parseMkSpec(ProFileEvaluator *evaluator) const
{
    const QStringList configValues = evaluator->values("CONFIG");
    d->m_defaultConfigIsDebugAndRelease = false;
    d->m_frameworkBuild = false;
    for (const QString &value : configValues) {
        if (value == "debug")
            d->m_defaultConfigIsDebug = true;
        else if (value == "release")
            d->m_defaultConfigIsDebug = false;
        else if (value == "build_all")
            d->m_defaultConfigIsDebugAndRelease = true;
        else if (value == "qt_framework")
            d->m_frameworkBuild = true;
    }
    const QString libinfix = MKSPEC_VALUE_LIBINFIX;
    const QString ns = MKSPEC_VALUE_NAMESPACE;
    d->m_mkspecValues.insert(libinfix, evaluator->value(libinfix));
    d->m_mkspecValues.insert(ns, evaluator->value(ns));
}

void QtVersion::setId(int id)
{
    d->m_id = id;
}

QString QtVersion::mkspec() const
{
    d->updateMkspec();
    return d->m_mkspec.toString();
}

QString QtVersion::mkspecFor(ToolChain *tc) const
{
    QString versionSpec = mkspec();
    if (!tc)
        return versionSpec;

    const QStringList tcSpecList = tc->suggestedMkspecList();
    if (tcSpecList.contains(versionSpec))
        return versionSpec;

    for (const QString &tcSpec : tcSpecList) {
        if (hasMkspec(tcSpec))
            return tcSpec;
    }

    return versionSpec;
}

FilePath QtVersion::mkspecPath() const
{
    d->updateMkspec();
    return d->m_mkspecFullPath;
}

bool QtVersion::hasMkspec(const QString &spec) const
{
    if (spec.isEmpty())
        return true; // default spec of a Qt version

    QDir mkspecDir = QDir(hostDataPath().toString() + "/mkspecs/");
    const QString absSpec = mkspecDir.absoluteFilePath(spec);
    if (QFileInfo(absSpec).isDir() && QFileInfo(absSpec + "/qmake.conf").isFile())
        return true;
    mkspecDir.setPath(sourcePath().toString() + "/mkspecs/");
    const QString absSrcSpec = mkspecDir.absoluteFilePath(spec);
    return absSrcSpec != absSpec
            && QFileInfo(absSrcSpec).isDir()
            && QFileInfo(absSrcSpec + "/qmake.conf").isFile();
}

QtVersion::QmakeBuildConfigs QtVersion::defaultBuildConfig() const
{
    ensureMkSpecParsed();
    QtVersion::QmakeBuildConfigs result = QtVersion::QmakeBuildConfig(0);

    if (d->m_defaultConfigIsDebugAndRelease)
        result = QtVersion::BuildAll;
    if (d->m_defaultConfigIsDebug)
        result = result | QtVersion::DebugBuild;
    return result;
}

QString QtVersion::qtVersionString() const
{
    d->updateVersionInfo();
    return d->m_data.qtVersionString;
}

QtVersionNumber QtVersion::qtVersion() const
{
    return QtVersionNumber(qtVersionString());
}

void QtVersionPrivate::updateVersionInfo()
{
    if (m_versionInfoUpToDate || !m_qmakeIsExecutable || m_isUpdating)
        return;

    m_isUpdating = true;

    // extract data from qmake executable
    m_versionInfo.clear();
    m_data.installed = true;
    m_data.hasExamples = false;
    m_data.hasDocumentation = false;

    QString error;
    if (!queryQMakeVariables(m_qmakeCommand, q->qmakeRunEnvironment(), &m_versionInfo, &error)) {
        m_qmakeIsExecutable = false;
        qWarning("Cannot update Qt version information from %s: %s.",
                 qPrintable(m_qmakeCommand.displayName()), qPrintable(error));
        return;
    }
    m_qmakeIsExecutable = true;

    auto fileProperty = [this](const QByteArray &name) {
        return FilePath::fromUserInput(qmakeProperty(name)).onDevice(m_qmakeCommand);
    };

    m_data.prefix = fileProperty("QT_INSTALL_PREFIX");
    m_data.binPath = fileProperty("QT_INSTALL_BINS");
    m_data.libExecPath = fileProperty("QT_INSTALL_LIBEXECS");
    m_data.configurationPath = fileProperty("QT_INSTALL_CONFIGURATION");
    m_data.dataPath = fileProperty("QT_INSTALL_DATA");
    m_data.demosPath = fileProperty("QT_INSTALL_DEMOS");
    m_data.docsPath = fileProperty("QT_INSTALL_DOCS");
    m_data.examplesPath = fileProperty("QT_INSTALL_EXAMPLES");
    m_data.headerPath = fileProperty("QT_INSTALL_HEADERS");
    m_data.importsPath = fileProperty("QT_INSTALL_IMPORTS");
    m_data.libraryPath = fileProperty("QT_INSTALL_LIBS");
    m_data.pluginPath = fileProperty("QT_INSTALL_PLUGINS");
    m_data.qmlPath = fileProperty("QT_INSTALL_QML");
    m_data.translationsPath = fileProperty("QT_INSTALL_TRANSLATIONS");
    m_data.hostBinPath = fileProperty("QT_HOST_BINS");
    m_data.hostLibexecPath = fileProperty("QT_HOST_LIBEXECS");
    m_data.hostDataPath = fileProperty("QT_HOST_DATA");
    m_data.hostPrefixPath = fileProperty("QT_HOST_PREFIX");

    // Now check for a qt that is configured with a prefix but not installed
    if (!m_data.hostBinPath.isReadableDir())
        m_data.installed = false;

    // Framework builds for Qt 4.8 don't use QT_INSTALL_HEADERS
    // so we don't check on mac
    if (!HostOsInfo::isMacHost()) {
        if (!m_data.headerPath.isReadableDir())
            m_data.installed = false;
    }

    if (m_data.docsPath.isReadableDir())
        m_data.hasDocumentation = true;

    if (m_data.examplesPath.isReadableDir())
        m_data.hasExamples = true;

    if (m_data.demosPath.isReadableDir())
        m_data.hasDemos = true;

    m_data.qtVersionString = qmakeProperty("QT_VERSION");

    m_isUpdating = false;
    m_versionInfoUpToDate = true;
}

QHash<ProKey,ProString> QtVersionPrivate::versionInfo()
{
    updateVersionInfo();
    return m_versionInfo;
}

QString QtVersionPrivate::qmakeProperty(const QHash<ProKey, ProString> &versionInfo,
                                            const QByteArray &name,
                                            PropertyVariant variant)
{
    QString val = versionInfo
                      .value(ProKey(QString::fromLatin1(
                          name
                          + (variant == PropertyVariantDev
                                 ? "/dev"
                                 : variant == PropertyVariantGet ? "/get" : "/src"))))
                      .toQString();
    if (!val.isNull())
        return val;
    return versionInfo.value(ProKey(name)).toQString();
}

void QtVersion::applyProperties(QMakeGlobals *qmakeGlobals) const
{
    qmakeGlobals->setProperties(d->versionInfo());
}

bool QtVersion::hasDocs() const
{
    d->updateVersionInfo();
    return d->m_data.hasDocumentation;
}

bool QtVersion::hasDemos() const
{
    d->updateVersionInfo();
    return d->m_data.hasDemos;
}

FilePath QtVersion::demosPath() const
{
    return d->m_data.demosPath;
}

FilePath QtVersion::frameworkPath() const
{
    if (HostOsInfo::isMacHost())
        return libraryPath();
    return {};
}

bool QtVersion::hasExamples() const
{
    d->updateVersionInfo();
    return d->m_data.hasExamples;
}

FilePath QtVersion::examplesPath() const // QT_INSTALL_EXAMPLES
{
    return d->m_data.examplesPath;
}

QStringList QtVersion::qtSoPaths() const
{
    const FilePaths qtPaths = {libraryPath(), pluginPath(), qmlPath(), importsPath()};
    QSet<QString> paths;
    for (const FilePath &p : qtPaths) {
        QString path = p.toString();
        if (path.isEmpty())
            continue;
        QDirIterator it(path, QStringList("*.so"), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            paths.insert(it.fileInfo().absolutePath());
        }
    }
    return Utils::toList(paths);
}

MacroExpander *QtVersion::macroExpander() const
{
    return d->m_expander.macroExpander(this);
}

std::unique_ptr<MacroExpander>
QtVersion::createMacroExpander(const std::function<const QtVersion *()> &qtVersion)
{
    const auto versionProperty =
        [qtVersion](const std::function<QString(const QtVersion *)> &property) {
            return [property, qtVersion]() -> QString {
                const QtVersion *version = qtVersion();
                return version ? property(version) : QString();
            };
        };
    std::unique_ptr<MacroExpander> expander(new MacroExpander);
    expander->setDisplayName(QtKitAspect::tr("Qt version"));

    expander->registerVariable("Qt:Version",
                               QtKitAspect::tr("The version string of the current Qt version."),
                               versionProperty([](const QtVersion *version) {
                                   return version->qtVersionString();
                               }));

    expander->registerVariable(
        "Qt:Type",
        QtKitAspect::tr("The type of the current Qt version."),
        versionProperty([](const QtVersion *version) {
            return version->type();
        }));

    expander->registerVariable(
        "Qt:Mkspec",
        QtKitAspect::tr("The mkspec of the current Qt version."),
        versionProperty([](const QtVersion *version) {
            return QDir::toNativeSeparators(version->mkspec());
        }));

    expander->registerVariable("Qt:QT_INSTALL_PREFIX",
                               QtKitAspect::tr(
                                   "The installation prefix of the current Qt version."),
                               versionProperty([](const QtVersion *version) {
                                   return version->prefix().path();
                               }));

    expander->registerVariable("Qt:QT_INSTALL_DATA",
                               QtKitAspect::tr(
                                   "The installation location of the current Qt version's data."),
                               versionProperty([](const QtVersion *version) {
                                   return version->dataPath().path();
                               }));

    expander->registerVariable("Qt:QT_HOST_PREFIX",
                               QtKitAspect::tr(
                                   "The host location of the current Qt version."),
                               versionProperty([](const QtVersion *version) {
                                   return version->hostPrefixPath().path();
                               }));

    expander->registerVariable("Qt:QT_HOST_LIBEXECS",
                               QtKitAspect::tr("The installation location of the current Qt "
                                               "version's internal host executable files."),
                               versionProperty([](const QtVersion *version) {
                                   return version->hostLibexecPath().path();
                               }));

    expander->registerVariable(
        "Qt:QT_INSTALL_HEADERS",
        QtKitAspect::tr("The installation location of the current Qt version's header files."),
        versionProperty(
            [](const QtVersion *version) { return version->headerPath().path(); }));

    expander->registerVariable(
        "Qt:QT_INSTALL_LIBS",
        QtKitAspect::tr("The installation location of the current Qt version's library files."),
        versionProperty(
            [](const QtVersion *version) { return version->libraryPath().path(); }));

    expander->registerVariable(
        "Qt:QT_INSTALL_DOCS",
        QtKitAspect::tr(
            "The installation location of the current Qt version's documentation files."),
        versionProperty(
            [](const QtVersion *version) { return version->docsPath().path(); }));

    expander->registerVariable(
        "Qt:QT_INSTALL_BINS",
        QtKitAspect::tr("The installation location of the current Qt version's executable files."),
        versionProperty([](const QtVersion *version) { return version->binPath().path(); }));

    expander->registerVariable(
        "Qt:QT_INSTALL_LIBEXECS",
        QtKitAspect::tr(
            "The installation location of the current Qt version's internal executable files."),
        versionProperty(
            [](const QtVersion *version) { return version->libExecPath().path(); }));

    expander
        ->registerVariable("Qt:QT_INSTALL_PLUGINS",
                           QtKitAspect::tr(
                               "The installation location of the current Qt version's plugins."),
                           versionProperty([](const QtVersion *version) {
                               return version->pluginPath().path();
                           }));

    expander
        ->registerVariable("Qt:QT_INSTALL_QML",
                           QtKitAspect::tr(
                               "The installation location of the current Qt version's QML files."),
                           versionProperty([](const QtVersion *version) {
                               return version->qmlPath().path();
                           }));

    expander
        ->registerVariable("Qt:QT_INSTALL_IMPORTS",
                           QtKitAspect::tr(
                               "The installation location of the current Qt version's imports."),
                           versionProperty([](const QtVersion *version) {
                               return version->importsPath().path();
                           }));

    expander->registerVariable(
        "Qt:QT_INSTALL_TRANSLATIONS",
        QtKitAspect::tr("The installation location of the current Qt version's translation files."),
        versionProperty(
            [](const QtVersion *version) { return version->translationsPath().path(); }));

    expander->registerVariable(
        "Qt:QT_INSTALL_CONFIGURATION",
        QtKitAspect::tr("The installation location of the current Qt version's translation files."),
        versionProperty(
            [](const QtVersion *version) { return version->configurationPath().path(); }));

    expander
        ->registerVariable("Qt:QT_INSTALL_EXAMPLES",
                           QtKitAspect::tr(
                               "The installation location of the current Qt version's examples."),
                           versionProperty([](const QtVersion *version) {
                               return version->examplesPath().path();
                           }));

    expander->registerVariable("Qt:QT_INSTALL_DEMOS",
                               QtKitAspect::tr(
                                   "The installation location of the current Qt version's demos."),
                               versionProperty([](const QtVersion *version) {
                                   return version->demosPath().path();
                               }));

    expander->registerVariable("Qt:QMAKE_MKSPECS",
                               QtKitAspect::tr("The current Qt version's default mkspecs (Qt 4)."),
                               versionProperty([](const QtVersion *version) {
                                   return version->d->qmakeProperty("QMAKE_MKSPECS");
                               }));

    expander->registerVariable("Qt:QMAKE_SPEC",
                               QtKitAspect::tr(
                                   "The current Qt version's default mkspec (Qt 5; host system)."),
                               versionProperty([](const QtVersion *version) {
                                   return version->d->qmakeProperty("QMAKE_SPEC");
                               }));

    expander
        ->registerVariable("Qt:QMAKE_XSPEC",
                           QtKitAspect::tr(
                               "The current Qt version's default mkspec (Qt 5; target system)."),
                           versionProperty([](const QtVersion *version) {
                               return version->d->qmakeProperty("QMAKE_XSPEC");
                           }));

    expander->registerVariable("Qt:QMAKE_VERSION",
                               QtKitAspect::tr("The current Qt's qmake version."),
                               versionProperty([](const QtVersion *version) {
                                   return version->d->qmakeProperty("QMAKE_VERSION");
                               }));

    //    FIXME: Re-enable once we can detect expansion loops.
    //    expander->registerVariable("Qt:Name",
    //        QtKitAspect::tr("The display name of the current Qt version."),
    //        versionProperty([](QtVersion *version) {
    //            return version->displayName();
    //        }));

    return expander;
}

void QtVersion::populateQmlFileFinder(FileInProjectFinder *finder, const Target *target)
{
    // If target given, then use the project associated with that ...
    const Project *startupProject = target ? target->project() : nullptr;

    // ... else try the session manager's global startup project ...
    if (!startupProject)
        startupProject = SessionManager::startupProject();

    // ... and if that is null, use the first project available.
    const QList<Project *> projects = SessionManager::projects();
    QTC_CHECK(projects.isEmpty() || startupProject);

    FilePath projectDirectory;
    FilePaths sourceFiles;

    // Sort files from startupProject to the front of the list ...
    if (startupProject) {
        projectDirectory = startupProject->projectDirectory();
        sourceFiles.append(startupProject->files(Project::SourceFiles));
    }

    // ... then add all the other projects' files.
    for (const Project *project : projects) {
        if (project != startupProject)
            sourceFiles.append(project->files(Project::SourceFiles));
    }

    // If no target was given, but we've found a startupProject, then try to deduce a
    // target from that.
    if (!target && startupProject)
        target = startupProject->activeTarget();

    // ... and find the sysroot and qml directory if we have any target at all.
    const Kit *kit = target ? target->kit() : nullptr;
    const FilePath activeSysroot = SysRootKitAspect::sysRoot(kit);
    const QtVersion *qtVersion = QtVersionManager::isLoaded()
            ? QtKitAspect::qtVersion(kit) : nullptr;
    FilePaths additionalSearchDirectories = qtVersion
            ? FilePaths({qtVersion->qmlPath()}) : FilePaths();

    if (target) {
        for (const DeployableFile &file : target->deploymentData().allFiles())
            finder->addMappedPath(file.localFilePath(), file.remoteFilePath());
    }

    // Add resource paths to the mapping
    if (startupProject) {
        if (ProjectNode *rootNode = startupProject->rootProjectNode()) {
            rootNode->forEachNode([&](FileNode *node) {
                if (auto resourceNode = dynamic_cast<ProjectExplorer::ResourceFileNode *>(node))
                    finder->addMappedPath(node->filePath(), ":" + resourceNode->qrcPath());
            });
        } else {
            // Can there be projects without root node?
        }
    }

    // Finally, do populate m_projectFinder
    finder->setProjectDirectory(projectDirectory);
    finder->setProjectFiles(sourceFiles);
    finder->setSysroot(activeSysroot);
    finder->setAdditionalSearchDirectories(additionalSearchDirectories);
}

QSet<Id> QtVersion::features() const
{
    if (d->m_overrideFeatures.isEmpty())
        return availableFeatures();
    return d->m_overrideFeatures;
}

void QtVersion::addToEnvironment(const Kit *k, Environment &env) const
{
    Q_UNUSED(k)
    env.set("QTDIR", hostDataPath().toUserOutput());
}

// Some Qt versions may require environment settings for qmake to work
//
// One such example is Blackberry which for some reason decided to always use the same
// qmake and use environment variables embedded in their mkspecs to make that point to
// the different Qt installations.

Environment QtVersion::qmakeRunEnvironment() const
{
    Environment env = d->m_qmakeCommand.deviceEnvironment();
    setupQmakeRunEnvironment(env);
    return env;
}

void QtVersion::setupQmakeRunEnvironment(Environment &env) const
{
    Q_UNUSED(env);
}

bool QtVersion::hasQmlDumpWithRelocatableFlag() const
{
    return ((qtVersion() > QtVersionNumber(4, 8, 4) && qtVersion() < QtVersionNumber(5, 0, 0))
            || qtVersion() >= QtVersionNumber(5, 1, 0));
}

Tasks QtVersion::reportIssuesImpl(const QString &proFile, const QString &buildDir) const
{
    Q_UNUSED(proFile)
    Q_UNUSED(buildDir)
    Tasks results;

    if (!isValid()) {
        //: %1: Reason for being invalid
        const QString msg = QCoreApplication::translate("QmakeProjectManager::QtVersion", "The Qt version is invalid: %1").arg(invalidReason());
        results.append(BuildSystemTask(Task::Error, msg));
    }

    FilePath qmake = qmakeFilePath();
    if (!qmake.isExecutableFile()) {
        //: %1: Path to qmake executable
        const QString msg = QCoreApplication::translate("QmakeProjectManager::QtVersion",
                    "The qmake command \"%1\" was not found or is not executable.").arg(qmake.displayName());
        results.append(BuildSystemTask(Task::Error, msg));
    }

    return results;
}

bool QtVersion::supportsMultipleQtAbis() const
{
    return false;
}

Tasks QtVersion::reportIssues(const QString &proFile, const QString &buildDir) const
{
    Tasks results = reportIssuesImpl(proFile, buildDir);
    Utils::sort(results);
    return results;
}

QtConfigWidget *QtVersion::createConfigurationWidget() const
{
    return nullptr;
}

static QByteArray runQmakeQuery(const FilePath &binary, const Environment &env, QString *error)
{
    QTC_ASSERT(error, return QByteArray());

    const int timeOutMS = 30000; // Might be slow on some machines.

    // Prevent e.g. qmake 4.x on MinGW to show annoying errors about missing dll's.
    WindowsCrashDialogBlocker crashDialogBlocker;

    QtcProcess process;
    process.setEnvironment(env);
    process.setCommand({binary, {"-query"}});
    process.start();

    if (!process.waitForStarted()) {
        *error = QCoreApplication::translate("QtVersion", "Cannot start \"%1\": %2")
                .arg(binary.displayName()).arg(process.errorString());
        return {};
    }
    if (!process.waitForFinished(timeOutMS)) {
        *error = QCoreApplication::translate("QtVersion", "Timeout running \"%1\" (%2 ms).")
                .arg(binary.displayName()).arg(timeOutMS);
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        *error = QCoreApplication::translate("QtVersion", "\"%1\" crashed.")
                .arg(binary.displayName());
        return {};
    }

    const QByteArray out = process.readAllStandardOutput();
    if (out.isEmpty()) {
        *error = QCoreApplication::translate("QtVersion", "\"%1\" produced no output: %2.")
                .arg(binary.displayName(), process.cleanedStdErr());
        return {};
    }

    error->clear();
    return out;
}

bool QtVersionPrivate::queryQMakeVariables(const FilePath &binary, const Environment &env,
                                               QHash<ProKey, ProString> *versionInfo, QString *error)
{
    QString tmp;
    if (!error)
        error = &tmp;

    if (!binary.isExecutableFile()) {
        *error = QCoreApplication::translate("QtVersion", "qmake \"%1\" is not an executable.")
                .arg(binary.displayName());
        return false;
    }

    QByteArray output;
    output = runQmakeQuery(binary, env, error);

    if (!output.contains("QMAKE_VERSION:")) {
        // Some setups pass error messages via stdout, fooling the logic below.
        // Example with docker/qemu/arm "OCI runtime exec failed: exec failed: container_linux.go:367:
        // starting container process caused: exec: "/bin/qmake": stat /bin/qmake: no such file or directory"
        // Since we have a rough idea on what the output looks like we can work around this.
        // Output does not always start with QT_SYSROOT, see QTCREATORBUG-26123.
        *error += QString::fromUtf8(output);
        return false;
    }

    if (output.isNull() && !error->isEmpty()) {
        // Note: Don't rerun if we were able to execute the binary before.

        // Try running qmake with all kinds of tool chains set up in the environment.
        // This is required to make non-static qmakes work on windows where every tool chain
        // tries to be incompatible with any other.
        const Abis abiList = Abi::abisOfBinary(binary);
        const Toolchains tcList = ToolChainManager::toolchains([&abiList](const ToolChain *t) {
            return abiList.contains(t->targetAbi());
        });
        for (ToolChain *tc : tcList) {
            Environment realEnv = env;
            tc->addToEnvironment(realEnv);
            output = runQmakeQuery(binary, realEnv, error);
            if (error->isEmpty())
                break;
        }
    }

    if (output.isNull())
        return false;

    QMakeGlobals::parseProperties(output, *versionInfo);

    return true;
}

QString QtVersionPrivate::qmakeProperty(const QByteArray &name,
                                            QtVersionPrivate::PropertyVariant variant)
{
    updateVersionInfo();
    return qmakeProperty(m_versionInfo, name, variant);
}

FilePath QtVersionPrivate::mkspecDirectoryFromVersionInfo(const QHash<ProKey, ProString> &versionInfo,
                                                              const FilePath &qmakeCommand)
{
    QString dataDir = qmakeProperty(versionInfo, "QT_HOST_DATA", PropertyVariantSrc);
    if (dataDir.isEmpty())
        return FilePath();
    return FilePath::fromUserInput(dataDir + "/mkspecs").onDevice(qmakeCommand);
}

FilePath QtVersionPrivate::mkspecFromVersionInfo(const QHash<ProKey, ProString> &versionInfo,
                                                     const FilePath &qmakeCommand)
{
    FilePath baseMkspecDir = mkspecDirectoryFromVersionInfo(versionInfo, qmakeCommand);
    if (baseMkspecDir.isEmpty())
        return FilePath();

    bool qt5 = false;
    QString theSpec = qmakeProperty(versionInfo, "QMAKE_XSPEC");
    if (theSpec.isEmpty())
        theSpec = "default";
    else
        qt5 = true;

    FilePath mkspecFullPath = baseMkspecDir.pathAppended(theSpec);

    // qDebug() << "default mkspec is located at" << mkspecFullPath;

    OsType osInfo = mkspecFullPath.osType();
    if (osInfo == OsTypeWindows) {
        if (!qt5) {
            QFile f2(mkspecFullPath.toString() + "/qmake.conf");
            if (f2.exists() && f2.open(QIODevice::ReadOnly)) {
                while (!f2.atEnd()) {
                    QByteArray line = f2.readLine();
                    if (line.startsWith("QMAKESPEC_ORIGINAL")) {
                        const QList<QByteArray> &temp = line.split('=');
                        if (temp.size() == 2) {
                            QString possibleFullPath = QString::fromLocal8Bit(temp.at(1).trimmed().constData());
                            if (possibleFullPath.contains('$')) { // QTBUG-28792
                                const QRegularExpression rex("\\binclude\\(([^)]+)/qmake\\.conf\\)");
                                const QRegularExpressionMatch match = rex.match(QString::fromLocal8Bit(f2.readAll()));
                                if (match.hasMatch()) {
                                    possibleFullPath = mkspecFullPath.toString() + '/'
                                            + match.captured(1);
                                }
                            }
                            // We sometimes get a mix of different slash styles here...
                            possibleFullPath = possibleFullPath.replace('\\', '/');
                            if (QFileInfo::exists(possibleFullPath)) // Only if the path exists
                                mkspecFullPath = FilePath::fromUserInput(possibleFullPath);
                        }
                        break;
                    }
                }
                f2.close();
            }
        }
    } else {
        if (osInfo == OsTypeMac) {
            QFile f2(mkspecFullPath.toString() + "/qmake.conf");
            if (f2.exists() && f2.open(QIODevice::ReadOnly)) {
                while (!f2.atEnd()) {
                    QByteArray line = f2.readLine();
                    if (line.startsWith("MAKEFILE_GENERATOR")) {
                        const QList<QByteArray> &temp = line.split('=');
                        if (temp.size() == 2) {
                            const QByteArray &value = temp.at(1);
                            if (value.contains("XCODE")) {
                                // we don't want to generate xcode projects...
                                // qDebug() << "default mkspec is xcode, falling back to g++";
                                return baseMkspecDir.pathAppended("macx-g++");
                            }
                        }
                        break;
                    }
                }
                f2.close();
            }
        }
        if (!qt5) {
            //resolve mkspec link
            QString rspec = mkspecFullPath.toFileInfo().symLinkTarget();
            if (!rspec.isEmpty())
                mkspecFullPath = FilePath::fromUserInput(
                            QDir(baseMkspecDir.toString()).absoluteFilePath(rspec));
        }
    }
    return mkspecFullPath;
}

FilePath QtVersionPrivate::sourcePath(const QHash<ProKey, ProString> &versionInfo)
{
    const QString qt5Source = qmakeProperty(versionInfo, "QT_INSTALL_PREFIX/src");
    if (!qt5Source.isEmpty())
        return FilePath::fromString(QFileInfo(qt5Source).canonicalFilePath());

    const QString installData = qmakeProperty(versionInfo, "QT_INSTALL_PREFIX");
    QString sourcePath = installData;
    QFile qmakeCache(installData + "/.qmake.cache");
    if (qmakeCache.exists() && qmakeCache.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&qmakeCache);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith("QT_SOURCE_TREE")) {
                sourcePath = line.split('=').at(1).trimmed();
                if (sourcePath.startsWith("$$quote(")) {
                    sourcePath.remove(0, 8);
                    sourcePath.chop(1);
                }
                break;
            }
        }
    }
    return FilePath::fromUserInput(QFileInfo(sourcePath).canonicalFilePath());
}

bool QtVersion::isInQtSourceDirectory(const FilePath &filePath) const
{
    FilePath source = sourcePath();
    if (source.isEmpty())
        return false;
    if (source.fileName() == "qtbase")
        source = source.parentDir();
    return filePath.isChildOf(source);
}

bool QtVersion::isQtSubProject(const FilePath &filePath) const
{
    FilePath source = sourcePath();
    if (!source.isEmpty()) {
        if (source.fileName() == "qtbase")
            source = source.parentDir();
        if (filePath.isChildOf(source))
            return true;
    }

    const FilePath examples = examplesPath();
    if (!examples.isEmpty() && filePath.isChildOf(examples))
        return true;

    const FilePath demos = demosPath();
    if (!demos.isEmpty() && filePath.isChildOf(demos))
        return true;

    return false;
}

bool QtVersion::isQmlDebuggingSupported(const Kit *k, QString *reason)
{
    QTC_ASSERT(k, return false);
    QtVersion *version = QtKitAspect::qtVersion(k);
    if (!version) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "No Qt version.");
        return false;
    }
    return version->isQmlDebuggingSupported(reason);
}

bool QtVersion::isQmlDebuggingSupported(QString *reason) const
{
    if (!isValid()) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "Invalid Qt version.");
        return false;
    }

    if (qtVersion() < QtVersionNumber(5, 0, 0)) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "Requires Qt 5.0.0 or newer.");
        return false;
    }

    return true;
}

bool QtVersion::isQtQuickCompilerSupported(const Kit *k, QString *reason)
{
    QTC_ASSERT(k, return false);
    QtVersion *version = QtKitAspect::qtVersion(k);
    if (!version) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "No Qt version.");
        return false;
    }
    return version->isQtQuickCompilerSupported(reason);
}

bool QtVersion::isQtQuickCompilerSupported(QString *reason) const
{
    if (!isValid()) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "Invalid Qt version.");
        return false;
    }

    if (qtVersion() < QtVersionNumber(5, 3, 0)) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "Requires Qt 5.3.0 or newer.");
        return false;
    }

    const QString qtQuickCompilerPrf = mkspecsPath().toString() + "/features/qtquickcompiler.prf";
    if (!QFileInfo::exists(qtQuickCompilerPrf)) {
        if (reason)
            *reason = QCoreApplication::translate("QtVersion", "This Qt Version does not contain Qt Quick Compiler.");
        return false;
    }

    return true;
}

FilePaths QtVersionPrivate::qtCorePaths()
{
    updateVersionInfo();
    const QString versionString = m_data.qtVersionString;

    const QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

    const FilePaths entries = m_data.libraryPath.dirEntries(filters)
                            + m_data.binPath.dirEntries(filters);

    FilePaths staticLibs;
    FilePaths dynamicLibs;
    for (const FilePath &entry : entries) {
        const QString file = entry.fileName();
        if (file.startsWith("QtCore") && file.endsWith(".framework") && entry.isReadableDir()) {
            // handle Framework
            dynamicLibs.append(entry.pathAppended(file.left(file.lastIndexOf('.'))));
        } else if (file.startsWith("libQtCore") || file.startsWith("QtCore")
                || file.startsWith("libQt5Core") || file.startsWith("Qt5Core")
                || file.startsWith("libQt6Core") || file.startsWith("Qt6Core")) {
            if (entry.isReadableFile()) {
                if (file.endsWith(".a") || file.endsWith(".lib"))
                    staticLibs.append(entry);
                else if (file.endsWith(".dll")
                         || file.endsWith(QString::fromLatin1(".so.") + versionString)
                         || file.endsWith(".so")
#if defined(Q_OS_OPENBSD)
                         || file.contains(QRegularExpression("\\.so\\.[0-9]+\\.[0-9]+$")) // QTCREATORBUG-23818
#endif
                         || file.endsWith(QLatin1Char('.') + versionString + ".dylib"))
                    dynamicLibs.append(entry);
            }
        }
    }
    // Only handle static libs if we cannot find dynamic ones:
    if (dynamicLibs.isEmpty())
        return staticLibs;
    return dynamicLibs;
}

static QByteArray scanQtBinaryForBuildString(const FilePath &library)
{
    QFile lib(library.toString());
    QByteArray buildString;

    if (lib.open(QIODevice::ReadOnly)) {
        const QByteArray startNeedle = "Qt ";
        const QByteArray buildNeedle = " build; by ";
        const size_t oneMiB = 1024 * 1024;
        const size_t keepSpace = 4096;
        const size_t bufferSize = oneMiB + keepSpace;
        QByteArray buffer(bufferSize, Qt::Uninitialized);

        char *const readStart = buffer.data() + keepSpace;
        auto readStartIt = buffer.begin() + keepSpace;
        const auto copyStartIt = readStartIt + (oneMiB - keepSpace);

        while (!lib.atEnd()) {
            const int read = lib.read(readStart, static_cast<int>(oneMiB));
            const auto readEndIt = readStart + read;
            auto currentIt = readStartIt;

            forever {
                const auto qtFoundIt = std::search(currentIt, readEndIt,
                                                   startNeedle.begin(), startNeedle.end());
                if (qtFoundIt == readEndIt)
                    break;

                currentIt = qtFoundIt + 1;

                // Found "Qt ", now find the next '\0'.
                const auto nullFoundIt = std::find(qtFoundIt, readEndIt, '\0');
                if (nullFoundIt == readEndIt)
                    break;

                // String much too long?
                const size_t len = std::distance(qtFoundIt, nullFoundIt);
                if (len > keepSpace)
                    continue;

                // Does it contain " build; by "?
                const auto buildByFoundIt = std::search(qtFoundIt, nullFoundIt,
                                                        buildNeedle.begin(), buildNeedle.end());
                if (buildByFoundIt == nullFoundIt)
                    continue;

                buildString = QByteArray(qtFoundIt, static_cast<int>(len));
                break;
            }

            if (!buildString.isEmpty() || readEndIt != buffer.constEnd())
                break;

            std::move(copyStartIt, readEndIt, buffer.begin()); // Copy last section to front.
        }
    }
    return buildString;
}

static QStringList extractFieldsFromBuildString(const QByteArray &buildString)
{
    if (buildString.isEmpty()
            || buildString.count() > 4096)
        return QStringList();

    const QRegularExpression buildStringMatcher("^Qt "
                                                "([\\d\\.a-zA-Z]*) " // Qt version
                                                "\\("
                                                "([\\w_-]+) "       // Abi information
                                                "(shared|static) (?:\\(dynamic\\) )?"
                                                "(debug|release)"
                                                " build; by "
                                                "(.*)"               // compiler with extra info
                                                "\\)$");

    QTC_ASSERT(buildStringMatcher.isValid(), qWarning() << buildStringMatcher.errorString());

    const QRegularExpressionMatch match = buildStringMatcher.match(QString::fromUtf8(buildString));
    if (!match.hasMatch())
        return QStringList();

    QStringList result;
    result.append(match.captured(1)); // qtVersion

    // Abi info string:
    QStringList abiInfo = match.captured(2).split('-', Qt::SkipEmptyParts);

    result.append(abiInfo.takeFirst()); // cpu

    const QString endian = abiInfo.takeFirst();
    QTC_ASSERT(endian.endsWith("_endian"), return QStringList());
    result.append(endian.left(endian.count() - 7)); // without the "_endian"

    result.append(abiInfo.takeFirst()); // pointer

    if (abiInfo.isEmpty()) {
        // no extra info whatsoever:
        result.append(""); // qreal is unset
        result.append(""); // extra info is unset
    } else {
        const QString next = abiInfo.at(0);
        if (next.startsWith("qreal_")) {
            abiInfo.takeFirst();
            result.append(next.mid(6)); // qreal: without the "qreal_" part;
        } else {
            result.append(""); // qreal is unset!
        }

        result.append(abiInfo.join('-')); // extra abi strings
    }

    result.append(match.captured(3)); // linkage
    result.append(match.captured(4)); // buildType
    result.append(match.captured(5)); // compiler

    return result;
}

static Abi refineAbiFromBuildString(const QByteArray &buildString, const Abi &probableAbi)
{
    QStringList buildStringData = extractFieldsFromBuildString(buildString);
    if (buildStringData.count() != 9)
        return probableAbi;

    const QString compiler = buildStringData.at(8);

    Abi::Architecture arch = probableAbi.architecture();
    Abi::OS os = probableAbi.os();
    Abi::OSFlavor flavor = probableAbi.osFlavor();
    Abi::BinaryFormat format = probableAbi.binaryFormat();
    unsigned char wordWidth = probableAbi.wordWidth();

    if (compiler.startsWith("GCC ") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMSysFlavor;
    } else if (compiler.startsWith("MSVC 2005")  && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2005Flavor;
    } else if (compiler.startsWith("MSVC 2008") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2008Flavor;
    } else if (compiler.startsWith("MSVC 2010") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2010Flavor;
    } else if (compiler.startsWith("MSVC 2012") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2012Flavor;
    } else if (compiler.startsWith("MSVC 2015") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2015Flavor;
    } else if (compiler.startsWith("MSVC 2017") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2017Flavor;
    } else if (compiler.startsWith("MSVC 2019") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2019Flavor;
    } else if (compiler.startsWith("MSVC 2022") && os == Abi::WindowsOS) {
        flavor = Abi::WindowsMsvc2022Flavor;
    }

    return Abi(arch, os, flavor, format, wordWidth);
}

static Abi scanQtBinaryForBuildStringAndRefineAbi(const FilePath &library,
                                                   const Abi &probableAbi)
{
    static QHash<FilePath, Abi> results;

    if (!results.contains(library)) {
        const QByteArray buildString = scanQtBinaryForBuildString(library);
        results.insert(library, refineAbiFromBuildString(buildString, probableAbi));
    }
    return results.value(library);
}

Abis QtVersion::qtAbisFromLibrary(const FilePaths &coreLibraries)
{
    Abis res;
    for (const FilePath &library : coreLibraries) {
        for (const Abi &abi : Abi::abisOfBinary(library)) {
            Abi tmp = abi;
            if (abi.osFlavor() == Abi::UnknownFlavor)
                tmp = scanQtBinaryForBuildStringAndRefineAbi(library, abi);
            if (!res.contains(tmp))
                res.append(tmp);
        }
    }
    return res;
}

void QtVersion::resetCache() const
{
    d->m_data.hasQtAbis = false;
    d->m_mkspecReadUpToDate = false;
}

// QtVersionFactory

static QList<QtVersionFactory *> g_qtVersionFactories;

QtVersion *QtVersionFactory::createQtVersionFromQMakePath
    (const FilePath &qmakePath, bool isAutoDetected, const QString &detectionSource, QString *error)
{
    QHash<ProKey, ProString> versionInfo;
    const Environment env = qmakePath.deviceEnvironment();
    if (!QtVersionPrivate::queryQMakeVariables(qmakePath, env, &versionInfo, error))
        return nullptr;
    FilePath mkspec = QtVersionPrivate::mkspecFromVersionInfo(versionInfo, qmakePath);

    QMakeVfs vfs;
    QMakeGlobals globals;
    globals.setProperties(versionInfo);
    ProMessageHandler msgHandler(false);
    ProFileCacheManager::instance()->incRefCount();
    QMakeParser parser(ProFileCacheManager::instance()->cache(), &vfs, &msgHandler);
    ProFileEvaluator evaluator(&globals, &parser, &vfs, &msgHandler);
    evaluator.loadNamedSpec(mkspec.path(), false);

    QList<QtVersionFactory *> factories = g_qtVersionFactories;
    Utils::sort(factories, [](const QtVersionFactory *l, const QtVersionFactory *r) {
        return l->m_priority > r->m_priority;
    });

    if (!qmakePath.isExecutableFile())
        return nullptr;

    QtVersionFactory::SetupData setup;
    setup.config = evaluator.values("CONFIG");
    setup.platforms = evaluator.values("QMAKE_PLATFORM"); // It's a list in general.
    setup.isQnx = !evaluator.value("QNX_CPUDIR").isEmpty();

    foreach (QtVersionFactory *factory, factories) {
        if (!factory->m_restrictionChecker || factory->m_restrictionChecker(setup)) {
            QtVersion *ver = factory->create();
            QTC_ASSERT(ver, continue);
            ver->d->m_id = QtVersionManager::getUniqueId();
            QTC_CHECK(ver->d->m_qmakeCommand.isEmpty()); // Should only be used once.
            ver->d->m_qmakeCommand = qmakePath;
            ver->d->m_detectionSource = detectionSource;
            ver->d->m_isAutodetected = isAutoDetected;
            ver->updateDefaultDisplayName();
            ProFileCacheManager::instance()->decRefCount();
            return ver;
        }
    }
    ProFileCacheManager::instance()->decRefCount();
    if (error) {
        *error = QCoreApplication::translate("QtSupport::QtVersionFactory",
                    "No factory found for qmake: \"%1\"").arg(qmakePath.displayName());
    }
    return nullptr;
}

QtVersionFactory::QtVersionFactory()
{
    g_qtVersionFactories.append(this);
}

QtVersionFactory::~QtVersionFactory()
{
    g_qtVersionFactories.removeOne(this);
}

const QList<QtVersionFactory *> QtVersionFactory::allQtVersionFactories()
{
    return g_qtVersionFactories;
}

bool QtVersionFactory::canRestore(const QString &type)
{
    return type == m_supportedType;
}

QtVersion *QtVersionFactory::restore(const QString &type, const QVariantMap &data)
{
    QTC_ASSERT(canRestore(type), return nullptr);
    QTC_ASSERT(m_creator, return nullptr);
    QtVersion *version = create();
    version->fromMap(data);
    return version;
}

QtVersion *QtVersionFactory::create() const
{
    QTC_ASSERT(m_creator, return nullptr);
    QtVersion *version = m_creator();
    version->d->m_type = m_supportedType;
    return version;
}

QtVersion *QtVersion::clone() const
{
    for (QtVersionFactory *factory : qAsConst(g_qtVersionFactories)) {
        if (factory->m_supportedType == d->m_type) {
            QtVersion *version = factory->create();
            QTC_ASSERT(version, return nullptr);
            version->fromMap(toMap());
            return version;
        }
    }
    QTC_CHECK(false);
    return nullptr;
}

void QtVersionFactory::setQtVersionCreator(const std::function<QtVersion *()> &creator)
{
    m_creator = creator;
}

void QtVersionFactory::setRestrictionChecker(const std::function<bool(const SetupData &)> &checker)
{
    m_restrictionChecker = checker;
}

void QtVersionFactory::setSupportedType(const QString &type)
{
    m_supportedType = type;
}

void QtVersionFactory::setPriority(int priority)
{
    m_priority = priority;
}

} // QtSupport

#if defined(WITH_TESTS)

#include <QTest>

#include "qtsupportplugin.h"

namespace QtSupport {
namespace Internal {

void QtSupportPlugin::testQtBuildStringParsing_data()
{
    QTest::addColumn<QByteArray>("buildString");
    QTest::addColumn<QString>("expected");

    QTest::newRow("invalid build string")
            << QByteArray("Qt with invalid buildstring") << QString();
    QTest::newRow("empty build string")
            << QByteArray("") << QString();
    QTest::newRow("huge build string")
            << QByteArray(8192, 'x') << QString();

    QTest::newRow("valid build string")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;;;shared;release;GCC 6.2.1 20160830";

    QTest::newRow("with qreal")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64-qreal___fp16 shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;__fp16;;shared;release;GCC 6.2.1 20160830";
    QTest::newRow("with qreal and abi")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64-qreal___fp16-eabi shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;__fp16;eabi;shared;release;GCC 6.2.1 20160830";
    QTest::newRow("with qreal, eabi and softfloat")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64-qreal___fp16-eabi-softfloat shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;__fp16;eabi-softfloat;shared;release;GCC 6.2.1 20160830";
    QTest::newRow("with eabi")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64-eabi shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;;eabi;shared;release;GCC 6.2.1 20160830";
    QTest::newRow("with eabi and softfloat")
            << QByteArray("Qt 5.7.1 (x86_64-little_endian-lp64-eabi-softfloat shared (dynamic) release build; by GCC 6.2.1 20160830)")
            << "5.7.1;x86_64;little;lp64;;eabi-softfloat;shared;release;GCC 6.2.1 20160830";
}

void QtSupportPlugin::testQtBuildStringParsing()
{
    QFETCH(QByteArray, buildString);
    QFETCH(QString, expected);

    QStringList expectedList;
    if (!expected.isEmpty())
        expectedList = expected.split(';');

    QStringList actual = extractFieldsFromBuildString(buildString);
    QCOMPARE(expectedList, actual);
}

} // Internal
} // QtSupport

#endif // WITH_TESTS
