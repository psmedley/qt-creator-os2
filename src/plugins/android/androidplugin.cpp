/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
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

#include "androidplugin.h"

#include "androidconfigurations.h"
#include "androidconstants.h"
#include "androiddebugsupport.h"
#include "androiddeployqtstep.h"
#include "androiddevice.h"
#include "androidmanager.h"
#include "androidmanifesteditorfactory.h"
#include "androidpackageinstallationstep.h"
#include "androidpotentialkit.h"
#include "androidqmlpreviewworker.h"
#include "androidqmltoolingsupport.h"
#include "androidqtversion.h"
#include "androidrunconfiguration.h"
#include "androidruncontrol.h"
#include "androidsettingswidget.h"
#include "androidtoolchain.h"
#include "javaeditor.h"
#include "javalanguageserver.h"

#ifdef HAVE_QBS
#  include "androidqbspropertyprovider.h"
#endif

#include <coreplugin/icore.h>
#include <utils/checkablemessagebox.h>
#include <utils/infobar.h>

#include <languageclient/languageclientsettings.h>

#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/deployconfiguration.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>

#include <qtsupport/qtversionmanager.h>

using namespace ProjectExplorer;
using namespace ProjectExplorer::Constants;

const char kSetupAndroidSetting[] = "ConfigureAndroid";

namespace Android {
namespace Internal {

class AndroidDeployConfigurationFactory : public DeployConfigurationFactory
{
public:
    AndroidDeployConfigurationFactory()
    {
        setConfigBaseId("Qt4ProjectManager.AndroidDeployConfiguration2");
        addSupportedTargetDeviceType(Constants::ANDROID_DEVICE_TYPE);
        setDefaultDisplayName(QCoreApplication::translate("Android::Internal",
                                                          "Deploy to Android Device"));
        addInitialStep(Constants::ANDROID_DEPLOY_QT_ID);
    }
};

class AndroidRunConfigurationFactory : public RunConfigurationFactory
{
public:
    AndroidRunConfigurationFactory()
    {
        registerRunConfiguration<Android::AndroidRunConfiguration>
                ("Qt4ProjectManager.AndroidRunConfiguration:");
        addSupportedTargetDeviceType(Android::Constants::ANDROID_DEVICE_TYPE);
    }
};

class AndroidPluginPrivate : public QObject
{
public:
    AndroidConfigurations androidConfiguration;
    AndroidSettingsPage settingsPage;
    AndroidDeployQtStepFactory deployQtStepFactory;
    AndroidQtVersionFactory qtVersionFactory;
    AndroidToolChainFactory toolChainFactory;
    AndroidDeployConfigurationFactory deployConfigurationFactory;
    AndroidDeviceFactory deviceFactory;
    AndroidPotentialKit potentialKit;
    JavaEditorFactory javaEditorFactory;
    AndroidPackageInstallationFactory packackeInstallationFactory;
    AndroidManifestEditorFactory manifestEditorFactory;
    AndroidRunConfigurationFactory runConfigFactory;

    RunWorkerFactory runWorkerFactory{
        RunWorkerFactory::make<AndroidRunSupport>(),
        {NORMAL_RUN_MODE},
        {runConfigFactory.runConfigurationId()}
    };
    RunWorkerFactory debugWorkerFactory{
        RunWorkerFactory::make<AndroidDebugSupport>(),
        {DEBUG_RUN_MODE},
        {runConfigFactory.runConfigurationId()}
    };
    RunWorkerFactory profilerWorkerFactory{
        RunWorkerFactory::make<AndroidQmlToolingSupport>(),
        {QML_PROFILER_RUN_MODE},
        {runConfigFactory.runConfigurationId()}
    };
    RunWorkerFactory qmlPreviewWorkerFactory{
        RunWorkerFactory::make<AndroidQmlPreviewWorker>(),
        {QML_PREVIEW_RUN_MODE},
        {"QmlProjectManager.QmlRunConfiguration.Qml", runConfigFactory.runConfigurationId()},
        {Android::Constants::ANDROID_DEVICE_TYPE}
    };

    AndroidBuildApkStepFactory buildApkStepFactory;
    AndroidDeviceManager deviceManager;
};

AndroidPlugin::~AndroidPlugin()
{
    delete d;
}

bool AndroidPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    d = new AndroidPluginPrivate;

    connect(KitManager::instance(), &KitManager::kitsLoaded,
            this, &AndroidPlugin::kitsRestored);

    LanguageClient::LanguageClientSettings::registerClientType({Android::Constants::JLS_SETTINGS_ID,
                                                                tr("Java Language Server"),
                                                                []() { return new JLSSettings; }});

    return true;
}

void AndroidPlugin::kitsRestored()
{
    const bool qtForAndroidInstalled
        = !QtSupport::QtVersionManager::versions([](const QtSupport::QtVersion *v) {
               return v->targetDeviceTypes().contains(Android::Constants::ANDROID_DEVICE_TYPE);
           }).isEmpty();

    if (!AndroidConfigurations::currentConfig().sdkFullyConfigured() && qtForAndroidInstalled) {
        connect(Core::ICore::instance(), &Core::ICore::coreOpened, this,
                &AndroidPlugin::askUserAboutAndroidSetup, Qt::QueuedConnection);
    }

    AndroidConfigurations::registerNewToolChains();
    AndroidConfigurations::updateAutomaticKitList();
    connect(QtSupport::QtVersionManager::instance(), &QtSupport::QtVersionManager::qtVersionsChanged,
            AndroidConfigurations::instance(), []() {
        AndroidConfigurations::registerNewToolChains();
        AndroidConfigurations::updateAutomaticKitList();
    });
    disconnect(KitManager::instance(), &KitManager::kitsLoaded,
               this, &AndroidPlugin::kitsRestored);
}

void AndroidPlugin::askUserAboutAndroidSetup()
{
    if (!Utils::CheckableMessageBox::shouldAskAgain(Core::ICore::settings(), kSetupAndroidSetting)
        || !Core::ICore::infoBar()->canInfoBeAdded(kSetupAndroidSetting))
        return;

    Utils::InfoBarEntry
        info(kSetupAndroidSetting,
             tr("Would you like to configure Android options? This will ensure "
                "Android kits can be usable and all essential packages are installed. "
                "To do it later, select Edit > Preferences > Devices > Android."),
             Utils::InfoBarEntry::GlobalSuppression::Enabled);
    info.addCustomButton(tr("Configure Android"), [this] {
        Core::ICore::infoBar()->removeInfo(kSetupAndroidSetting);
        Core::ICore::infoBar()->globallySuppressInfo(kSetupAndroidSetting);
        QTimer::singleShot(0, this, [this]() { d->potentialKit.executeFromMenu(); });
    });
    Core::ICore::infoBar()->addInfo(info);
}

} // namespace Internal
} // namespace Android
