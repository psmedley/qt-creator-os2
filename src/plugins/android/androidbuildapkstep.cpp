/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
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

#include "androidbuildapkstep.h"

#include "androidconfigurations.h"
#include "androidconstants.h"
#include "androidcreatekeystorecertificate.h"
#include "androidextralibrarylistmodel.h"
#include "androidmanager.h"
#include "androidqtversion.h"
#include "androidsdkmanager.h"
#include "certificatesmodel.h"
#include "createandroidmanifestwizard.h"

#include "javaparser.h"

#include <coreplugin/fileutils.h>
#include <coreplugin/icore.h>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/buildsystem.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/target.h>
#include <projectexplorer/taskhub.h>

#include <qtsupport/qtkitinformation.h>

#include <utils/algorithm.h>
#include <utils/fancylineedit.h>
#include <utils/infolabel.h>
#include <utils/pathchooser.h>
#include <utils/qtcprocess.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;

namespace Android {
namespace Internal {

static Q_LOGGING_CATEGORY(buildapkstepLog, "qtc.android.build.androidbuildapkstep", QtWarningMsg)

const char KeystoreLocationKey[] = "KeystoreLocation";
const char BuildTargetSdkKey[] = "BuildTargetSdk";
const char VerboseOutputKey[] = "VerboseOutput";

class PasswordInputDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(Android::Internal::AndroidBuildApkStep)

public:
    enum Context{
      KeystorePassword = 1,
      CertificatePassword
    };

    PasswordInputDialog(Context context, std::function<bool (const QString &)> callback,
                        const QString &extraContextStr, QWidget *parent = nullptr);

    static QString getPassword(Context context, std::function<bool (const QString &)> callback,
                               const QString &extraContextStr, bool *ok = nullptr,
                               QWidget *parent = nullptr);

private:
    std::function<bool (const QString &)> verifyCallback = [](const QString &) { return true; };
    QLabel *inputContextlabel = new QLabel(this);
    QLineEdit *inputEdit = new QLineEdit(this);
    Utils::InfoLabel *warningLabel = new Utils::InfoLabel(tr("Incorrect password."),
                                                          Utils::InfoLabel::Warning, this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                       this);
};

// AndroidBuildApkWidget

class AndroidBuildApkWidget : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(Android::Internal::AndroidBuildApkStep)

public:
    explicit AndroidBuildApkWidget(AndroidBuildApkStep *step);

private:
    void setCertificates();
    void updateSigningWarning();
    void signPackageCheckBoxToggled(bool checked);
    void onOpenSslCheckBoxChanged();
    bool isOpenSslLibsIncluded();
    FilePath appProjectFilePath() const;
    QString openSslIncludeFileContent(const FilePath &projectPath);

    QWidget *createApplicationGroup();
    QWidget *createSignPackageGroup();
    QWidget *createAdvancedGroup();
    QWidget *createAdditionalLibrariesGroup();

private:
    AndroidBuildApkStep *m_step = nullptr;
    QCheckBox *m_signPackageCheckBox = nullptr;
    InfoLabel *m_signingDebugWarningLabel = nullptr;
    QComboBox *m_certificatesAliasComboBox = nullptr;
    QCheckBox *m_addDebuggerCheckBox = nullptr;
    QCheckBox *m_openSslCheckBox = nullptr;
};

AndroidBuildApkWidget::AndroidBuildApkWidget(AndroidBuildApkStep *step)
    : m_step(step)
{
    auto vbox = new QVBoxLayout(this);
    vbox->addWidget(createSignPackageGroup());
    vbox->addWidget(createApplicationGroup());
    vbox->addWidget(createAdvancedGroup());
    vbox->addWidget(createAdditionalLibrariesGroup());

    connect(m_step->buildConfiguration(), &BuildConfiguration::buildTypeChanged,
            this, &AndroidBuildApkWidget::updateSigningWarning);

    connect(m_signPackageCheckBox, &QAbstractButton::clicked,
            m_addDebuggerCheckBox, &QWidget::setEnabled);

    signPackageCheckBoxToggled(m_step->signPackage());
    updateSigningWarning();
}

QWidget *AndroidBuildApkWidget::createApplicationGroup()
{
    QtSupport::QtVersion *qt = QtSupport::QtKitAspect::qtVersion(m_step->target()->kit());
    const int minApiSupported = AndroidManager::defaultMinimumSDK(qt);
    QStringList targets = AndroidConfig::apiLevelNamesFor(AndroidConfigurations::sdkManager()->
                                                          filteredSdkPlatforms(minApiSupported));
    targets.removeDuplicates();

    auto group = new QGroupBox(tr("Application"), this);

    auto targetSDKComboBox = new QComboBox();
    targetSDKComboBox->addItems(targets);
    targetSDKComboBox->setCurrentIndex(targets.indexOf(m_step->buildTargetSdk()));

    const auto cbActivated = QOverload<int>::of(&QComboBox::activated);
    connect(targetSDKComboBox, cbActivated, this, [this, targetSDKComboBox](int idx) {
       const QString sdk = targetSDKComboBox->itemText(idx);
       m_step->setBuildTargetSdk(sdk);
       AndroidManager::updateGradleProperties(m_step->target(), QString()); // FIXME: Use real key.
   });

    auto formLayout = new QFormLayout(group);
    formLayout->addRow(tr("Android build platform SDK:"), targetSDKComboBox);

    auto createAndroidTemplatesButton = new QPushButton(tr("Create Templates"));
    createAndroidTemplatesButton->setToolTip(
        tr("Create an Android package for Custom Java code, assets, and Gradle configurations."));
    connect(createAndroidTemplatesButton, &QAbstractButton::clicked, this, [this] {
        CreateAndroidManifestWizard wizard(m_step->buildSystem());
        wizard.exec();
    });

    formLayout->addRow(tr("Android customization:"), createAndroidTemplatesButton);

    return group;
}

QWidget *AndroidBuildApkWidget::createSignPackageGroup()
{
    QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    auto group = new QGroupBox(tr("Application Signature"), this);

    auto keystoreLocationLabel = new QLabel(tr("Keystore:"), group);
    keystoreLocationLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    auto keystoreLocationChooser = new PathChooser(group);
    keystoreLocationChooser->setExpectedKind(PathChooser::File);
    keystoreLocationChooser->lineEdit()->setReadOnly(true);
    keystoreLocationChooser->setFilePath(m_step->keystorePath());
    keystoreLocationChooser->setInitialBrowsePathBackup(FileUtils::homePath());
    keystoreLocationChooser->setPromptDialogFilter(tr("Keystore files (*.keystore *.jks)"));
    keystoreLocationChooser->setPromptDialogTitle(tr("Select Keystore File"));
    connect(keystoreLocationChooser, &PathChooser::filePathChanged, this, [this](const FilePath &file) {
        m_step->setKeystorePath(file);
        m_signPackageCheckBox->setChecked(!file.isEmpty());
        if (!file.isEmpty())
            setCertificates();
    });

    auto keystoreCreateButton = new QPushButton(tr("Create..."), group);
    connect(keystoreCreateButton, &QAbstractButton::clicked, this, [this, keystoreLocationChooser] {
        AndroidCreateKeystoreCertificate d;
        if (d.exec() != QDialog::Accepted)
            return;
        keystoreLocationChooser->setFilePath(d.keystoreFilePath());
        m_step->setKeystorePath(d.keystoreFilePath());
        m_step->setKeystorePassword(d.keystorePassword());
        m_step->setCertificateAlias(d.certificateAlias());
        m_step->setCertificatePassword(d.certificatePassword());
        setCertificates();
    });

    m_signPackageCheckBox = new QCheckBox(tr("Sign package"), group);
    m_signPackageCheckBox->setChecked(m_step->signPackage());

    m_signingDebugWarningLabel = new Utils::InfoLabel(tr("Signing a debug package"),
                                                      Utils::InfoLabel::Warning, group);
    m_signingDebugWarningLabel->hide();

    auto certificateAliasLabel = new QLabel(tr("Certificate alias:"), group);
    certificateAliasLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    m_certificatesAliasComboBox = new QComboBox(group);
    m_certificatesAliasComboBox->setEnabled(false);
    QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    m_certificatesAliasComboBox->setSizePolicy(sizePolicy2);
    m_certificatesAliasComboBox->setMinimumSize(QSize(300, 0));

    auto horizontalLayout_2 = new QHBoxLayout;
    horizontalLayout_2->addWidget(keystoreLocationLabel);
    horizontalLayout_2->addWidget(keystoreLocationChooser);
    horizontalLayout_2->addWidget(keystoreCreateButton);

    auto horizontalLayout_3 = new QHBoxLayout;
    horizontalLayout_3->addWidget(m_signingDebugWarningLabel);
    horizontalLayout_3->addWidget(certificateAliasLabel);
    horizontalLayout_3->addWidget(m_certificatesAliasComboBox);

    auto vbox = new QVBoxLayout(group);
    vbox->addLayout(horizontalLayout_2);
    vbox->addWidget(m_signPackageCheckBox);
    vbox->addLayout(horizontalLayout_3);

    connect(m_signPackageCheckBox, &QAbstractButton::toggled,
            this, &AndroidBuildApkWidget::signPackageCheckBoxToggled);

    auto updateAlias = [this](int idx) {
        QString alias = m_certificatesAliasComboBox->itemText(idx);
        if (!alias.isEmpty())
            m_step->setCertificateAlias(alias);
    };

    const auto cbActivated = QOverload<int>::of(&QComboBox::activated);
    const auto cbCurrentIndexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);

    connect(m_certificatesAliasComboBox, cbActivated, this, updateAlias);
    connect(m_certificatesAliasComboBox, cbCurrentIndexChanged, this, updateAlias);

    return group;
}

QWidget *AndroidBuildApkWidget::createAdvancedGroup()
{
    auto group = new QGroupBox(tr("Advanced Actions"), this);

    auto openPackageLocationCheckBox = new QCheckBox(tr("Open package location after build"), group);
    openPackageLocationCheckBox->setChecked(m_step->openPackageLocation());
    connect(openPackageLocationCheckBox, &QAbstractButton::toggled,
            this, [this](bool checked) { m_step->setOpenPackageLocation(checked); });

    m_addDebuggerCheckBox = new QCheckBox(tr("Add debug server"), group);
    m_addDebuggerCheckBox->setEnabled(false);
    m_addDebuggerCheckBox->setToolTip(tr("Packages debug server with "
           "the APK to enable debugging. For the signed APK this option is unchecked by default."));
    m_addDebuggerCheckBox->setChecked(m_step->addDebugger());
    connect(m_addDebuggerCheckBox, &QAbstractButton::toggled,
            m_step, &AndroidBuildApkStep::setAddDebugger);

    auto verboseOutputCheckBox = new QCheckBox(tr("Verbose output"), group);
    verboseOutputCheckBox->setChecked(m_step->verboseOutput());

    auto vbox = new QVBoxLayout(group);
    QtSupport::QtVersion *version = QtSupport::QtKitAspect::qtVersion(m_step->kit());
    if (version && version->qtVersion() >= QtSupport::QtVersionNumber{5, 14}) {
        auto buildAAB = new QCheckBox(tr("Build Android App Bundle (*.aab)"), group);
        buildAAB->setChecked(m_step->buildAAB());
        connect(buildAAB, &QAbstractButton::toggled, m_step, &AndroidBuildApkStep::setBuildAAB);
        vbox->addWidget(buildAAB);
    }
    vbox->addWidget(openPackageLocationCheckBox);
    vbox->addWidget(verboseOutputCheckBox);
    vbox->addWidget(m_addDebuggerCheckBox);

    connect(verboseOutputCheckBox, &QAbstractButton::toggled,
            this, [this](bool checked) { m_step->setVerboseOutput(checked); });

    return group;
}

QWidget *AndroidBuildApkWidget::createAdditionalLibrariesGroup()
{
    auto group = new QGroupBox(tr("Additional Libraries"));
    group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto libsModel = new AndroidExtraLibraryListModel(m_step->buildSystem(), this);
    connect(libsModel, &AndroidExtraLibraryListModel::enabledChanged, this,
            [this, group](const bool enabled) {
                group->setEnabled(enabled);
                m_openSslCheckBox->setChecked(isOpenSslLibsIncluded());
    });

    auto libsView = new QListView;
    libsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    libsView->setToolTip(tr("List of extra libraries to include in Android package and load on startup."));
    libsView->setModel(libsModel);

    auto addLibButton = new QToolButton;
    addLibButton->setText(tr("Add..."));
    addLibButton->setToolTip(tr("Select library to include in package."));
    addLibButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    addLibButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(addLibButton, &QAbstractButton::clicked, this, [this, libsModel] {
        QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                              tr("Select additional libraries"),
                                                              QDir::homePath(),
                                                              tr("Libraries (*.so)"));
        if (!fileNames.isEmpty())
            libsModel->addEntries(fileNames);
    });

    auto removeLibButton = new QToolButton;
    removeLibButton->setText(tr("Remove"));
    removeLibButton->setToolTip(tr("Remove currently selected library from list."));
    connect(removeLibButton, &QAbstractButton::clicked, this, [libsModel, libsView] {
        QModelIndexList removeList = libsView->selectionModel()->selectedIndexes();
        libsModel->removeEntries(removeList);
    });

    auto libsButtonLayout = new QVBoxLayout;
    libsButtonLayout->addWidget(addLibButton);
    libsButtonLayout->addWidget(removeLibButton);
    libsButtonLayout->addStretch(1);

    m_openSslCheckBox = new QCheckBox(tr("Include prebuilt OpenSSL libraries"));
    m_openSslCheckBox->setToolTip(tr("This is useful for apps that use SSL operations. The path "
                                     "can be defined in Edit > Preferences > Devices > Android."));
    connect(m_openSslCheckBox, &QAbstractButton::clicked, this,
            &AndroidBuildApkWidget::onOpenSslCheckBoxChanged);

    auto grid = new QGridLayout(group);
    grid->addWidget(m_openSslCheckBox, 0, 0);
    grid->addWidget(libsView, 1, 0);
    grid->addLayout(libsButtonLayout, 1, 1);

    QItemSelectionModel *libSelection = libsView->selectionModel();
    connect(libSelection, &QItemSelectionModel::selectionChanged, this, [libSelection, removeLibButton] {
        removeLibButton->setEnabled(libSelection->hasSelection());
    });

    Target *target = m_step->target();
    const QString buildKey = target->activeBuildKey();
    const ProjectNode *node = target->project()->findNodeForBuildKey(buildKey);
    group->setEnabled(node && !node->parseInProgress());

    return group;
}

void AndroidBuildApkWidget::signPackageCheckBoxToggled(bool checked)
{
    m_certificatesAliasComboBox->setEnabled(checked);
    m_step->setSignPackage(checked);
    m_addDebuggerCheckBox->setChecked(!checked);
    updateSigningWarning();
    if (!checked)
        return;
    if (!m_step->keystorePath().isEmpty())
        setCertificates();
}

void AndroidBuildApkWidget::onOpenSslCheckBoxChanged()
{
    Utils::FilePath projectPath = appProjectFilePath();
    QFile projectFile(projectPath.toString());
    if (!projectFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qWarning() << "Cannot open project file to add OpenSSL extra libs: " << projectPath;
        return;
    }

    const QString searchStr = openSslIncludeFileContent(projectPath);
    QTextStream textStream(&projectFile);

    QString fileContent = textStream.readAll();
    if (!m_openSslCheckBox->isChecked()) {
        fileContent.remove("\n" + searchStr);
    } else if (!fileContent.contains(searchStr, Qt::CaseSensitive)) {
        fileContent.append(searchStr + "\n");
    }

    projectFile.resize(0);
    textStream << fileContent;
    projectFile.close();
}

FilePath AndroidBuildApkWidget::appProjectFilePath() const
{
    const FilePath topLevelFile = m_step->buildConfiguration()->buildSystem()->projectFilePath();
    if (topLevelFile.fileName() == "CMakeLists.txt")
        return topLevelFile;
    static const auto isApp = [](Node *n) { return n->asProjectNode()
                && n->asProjectNode()->productType() == ProductType::App; };
    Node * const appNode = m_step->buildConfiguration()->project()->rootProjectNode()
            ->findNode(isApp);
    return appNode ? appNode ->filePath() : topLevelFile;
}

bool AndroidBuildApkWidget::isOpenSslLibsIncluded()
{
    Utils::FilePath projectPath = appProjectFilePath();
    const QString searchStr = openSslIncludeFileContent(projectPath);
    QFile projectFile(projectPath.toString());
    projectFile.open(QIODevice::ReadOnly);
    QTextStream textStream(&projectFile);
    QString fileContent = textStream.readAll();
    projectFile.close();
    return fileContent.contains(searchStr, Qt::CaseSensitive);
}

QString AndroidBuildApkWidget::openSslIncludeFileContent(const FilePath &projectPath)
{
    QString openSslPath = AndroidConfigurations::currentConfig().openSslLocation().toString();
    if (projectPath.endsWith(".pro"))
        return "android: include(" + openSslPath + "/openssl.pri)";
    if (projectPath.endsWith("CMakeLists.txt"))
        return "if (ANDROID)\n    include(" + openSslPath + "/CMakeLists.txt)\nendif()";

    return QString();
}

void AndroidBuildApkWidget::setCertificates()
{
    QAbstractItemModel *certificates = m_step->keystoreCertificates();
    if (certificates) {
        m_signPackageCheckBox->setChecked(certificates);
        m_certificatesAliasComboBox->setModel(certificates);
    }
}

void AndroidBuildApkWidget::updateSigningWarning()
{
    bool nonRelease = m_step->buildType() != BuildConfiguration::Release;
    bool visible = m_step->signPackage() && nonRelease;
    m_signingDebugWarningLabel->setVisible(visible);
}

// AndroidBuildApkStep

AndroidBuildApkStep::AndroidBuildApkStep(BuildStepList *parent, Utils::Id id)
    : AbstractProcessStep(parent, id),
      m_buildTargetSdk(AndroidConfig::apiLevelNameFor(AndroidConfigurations::
                                         sdkManager()->latestAndroidSdkPlatform()))
{
    setImmutable(true);
    setDisplayName(tr("Build Android APK"));
}

bool AndroidBuildApkStep::init()
{
    if (!AbstractProcessStep::init()) {
        reportWarningOrError(tr("\"%1\" step failed initialization.").arg(displayName()),
                             Task::Error);
        return false;
    }

    if (m_signPackage) {
        qCDebug(buildapkstepLog) << "Signing enabled";
        // check keystore and certificate passwords
        if (!verifyKeystorePassword() || !verifyCertificatePassword()) {
            reportWarningOrError(tr("Keystore/Certificate password verification failed."),
                                 Task::Error);
            return false;
        }

        if (buildType() != BuildConfiguration::Release)
            reportWarningOrError(tr("Warning: Signing a debug or profile package."), Task::Warning);
    }

    QtSupport::QtVersion *version = QtSupport::QtKitAspect::qtVersion(kit());
    if (!version) {
        reportWarningOrError(tr("The Qt version for kit %1 is invalid.").arg(kit()->displayName()),
                             Task::Error);
        return false;
    }

    const QVersionNumber sdkToolsVersion = AndroidConfigurations::currentConfig().sdkToolsVersion();
    if (sdkToolsVersion >= QVersionNumber(25, 3, 0)
            && AndroidConfigurations::currentConfig().preCmdlineSdkToolsInstalled()) {
        if (!version->sourcePath().pathAppended("src/3rdparty/gradle").exists()) {
            const QString error
                = tr("The installed SDK tools version (%1) does not include Gradle "
                     "scripts. The minimum Qt version required for Gradle build to work "
                     "is %2")
                      .arg(sdkToolsVersion.toString())
                      .arg("5.9.0/5.6.3");
            reportWarningOrError(error, Task::Error);
            return false;
        }
    } else if (version->qtVersion() < QtSupport::QtVersionNumber(5, 4, 0)) {
        const QString error = tr("The minimum Qt version required for Gradle build to work is %1. "
                                 "It is recommended to install the latest Qt version.")
                                  .arg("5.4.0");
        reportWarningOrError(error, Task::Error);
        return false;
    }

    const int minSDKForKit = AndroidManager::minimumSDK(kit());
    if (AndroidManager::minimumSDK(target()) < minSDKForKit) {
        const QString error
            = tr("The API level set for the APK is less than the minimum required by the kit."
                 "\nThe minimum API level required by the kit is %1.")
                  .arg(minSDKForKit);
        reportWarningOrError(error, Task::Error);
        return false;
    }

    m_openPackageLocationForRun = m_openPackageLocation;
    const FilePath outputDir = AndroidManager::androidBuildDirectory(target());

    if (m_buildAAB) {
        const QString bt = buildType() == BuildConfiguration::Release ? QLatin1String("release")
                                                                      : QLatin1String("debug");
        m_packagePath = outputDir.pathAppended(
            QString("build/outputs/bundle/%1/android-build-%1.aab").arg(bt));
    } else {
        m_packagePath = AndroidManager::apkPath(target());
    }

    qCDebug(buildapkstepLog) << "APK or AAB path:" << m_packagePath;

    FilePath command = version->hostBinPath().pathAppended("androiddeployqt").withExecutableSuffix();

    m_inputFile = AndroidQtVersion::androidDeploymentSettings(target());
    if (m_inputFile.isEmpty()) {
        m_skipBuilding = true;
        reportWarningOrError(tr("No valid input file for \"%1\".").arg(target()->activeBuildKey()),
                             Task::Warning);
        return true;
    }
    m_skipBuilding = false;

    if (m_buildTargetSdk.isEmpty()) {
        reportWarningOrError(tr("Android build SDK version is not defined. Check Android settings.")
                             , Task::Error);
        return false;
    }

    QStringList arguments = {"--input", m_inputFile.toString(),
                             "--output", outputDir.toString(),
                             "--android-platform", m_buildTargetSdk,
                             "--jdk", AndroidConfigurations::currentConfig().openJDKLocation().toString()};

    if (m_verbose)
        arguments << "--verbose";

    arguments << "--gradle";

    if (m_buildAAB)
        arguments << "--aab" <<  "--jarsigner";

    QStringList argumentsPasswordConcealed = arguments;

    if (m_signPackage) {
        arguments << "--release"
                  << "--sign" << m_keystorePath.toString() << m_certificateAlias
                  << "--storepass" << m_keystorePasswd;
        argumentsPasswordConcealed << "--release"
                                   << "--sign" << "******"
                                   << "--storepass" << "******";
        if (!m_certificatePasswd.isEmpty()) {
            arguments << "--keypass" << m_certificatePasswd;
            argumentsPasswordConcealed << "--keypass" << "******";
        }

    }

    // Must be the last option, otherwise androiddeployqt might use the other
    // params (e.g. --sign) to choose not to add gdbserver
    if (version->qtVersion() >= QtSupport::QtVersionNumber(5, 6, 0)) {
        if (m_addDebugger || buildType() == ProjectExplorer::BuildConfiguration::Debug)
            arguments << "--gdbserver";
        else
            arguments << "--no-gdbserver";
    }

    processParameters()->setCommandLine({command, arguments});

    // Generate arguments with keystore password concealed
    ProjectExplorer::ProcessParameters pp2;
    setupProcessParameters(&pp2);
    pp2.setCommandLine({command, argumentsPasswordConcealed});
    m_command = pp2.effectiveCommand();
    m_argumentsPasswordConcealed = pp2.prettyArguments();

    return true;
}

void AndroidBuildApkStep::setupOutputFormatter(OutputFormatter *formatter)
{
    const auto parser = new JavaParser;
    parser->setProjectFileList(project()->files(Project::AllFiles));

    const QString buildKey = target()->activeBuildKey();
    const ProjectNode *node = project()->findNodeForBuildKey(buildKey);
    FilePath sourceDirPath;
    if (node)
        sourceDirPath = FilePath::fromVariant(node->data(Constants::AndroidPackageSourceDir));
    parser->setSourceDirectory(sourceDirPath.canonicalPath());
    parser->setBuildDirectory(AndroidManager::androidBuildDirectory(target()));
    formatter->addLineParser(parser);
    AbstractProcessStep::setupOutputFormatter(formatter);
}

void AndroidBuildApkStep::showInGraphicalShell()
{
    Core::FileUtils::showInGraphicalShell(Core::ICore::dialogParent(), m_packagePath);
}

QWidget *AndroidBuildApkStep::createConfigWidget()
{
    return new AndroidBuildApkWidget(this);
}

void AndroidBuildApkStep::processFinished(int exitCode, QProcess::ExitStatus status)
{
    AbstractProcessStep::processFinished(exitCode, status);
    if (m_openPackageLocationForRun && status == QProcess::NormalExit && exitCode == 0)
        QTimer::singleShot(0, this, &AndroidBuildApkStep::showInGraphicalShell);
}

bool AndroidBuildApkStep::verifyKeystorePassword()
{
    if (!m_keystorePath.exists()) {
        reportWarningOrError(tr("Cannot sign the package. Invalid keystore path (%1).")
                             .arg(m_keystorePath.toString()), Task::Error);
        return false;
    }

    if (AndroidManager::checkKeystorePassword(m_keystorePath.toString(), m_keystorePasswd))
        return true;

    bool success = false;
    auto verifyCallback = std::bind(&AndroidManager::checkKeystorePassword,
                                    m_keystorePath.toString(), std::placeholders::_1);
    m_keystorePasswd = PasswordInputDialog::getPassword(PasswordInputDialog::KeystorePassword,
                                                        verifyCallback, "", &success);
    return success;
}

bool AndroidBuildApkStep::verifyCertificatePassword()
{
    if (!AndroidManager::checkCertificateExists(m_keystorePath.toString(), m_keystorePasswd,
                                                 m_certificateAlias)) {
        reportWarningOrError(tr("Cannot sign the package. Certificate alias %1 does not exist.")
                             .arg(m_certificateAlias), Task::Error);
        return false;
    }

    if (AndroidManager::checkCertificatePassword(m_keystorePath.toString(), m_keystorePasswd,
                                                 m_certificateAlias, m_certificatePasswd)) {
        return true;
    }

    bool success = false;
    auto verifyCallback = std::bind(&AndroidManager::checkCertificatePassword,
                                    m_keystorePath.toString(), m_keystorePasswd,
                                    m_certificateAlias, std::placeholders::_1);

    m_certificatePasswd = PasswordInputDialog::getPassword(PasswordInputDialog::CertificatePassword,
                                                           verifyCallback, m_certificateAlias,
                                                           &success);
    return success;
}


static bool copyFileIfNewer(const FilePath &sourceFilePath,
                            const FilePath &destinationFilePath)
{
    if (sourceFilePath == destinationFilePath)
        return true;
    if (destinationFilePath.exists()) {
        if (sourceFilePath.lastModified() <= destinationFilePath.lastModified())
            return true;
        if (!destinationFilePath.removeFile())
            return false;
    }

    if (!destinationFilePath.parentDir().ensureWritableDir())
        return false;
    return sourceFilePath.copyFile(destinationFilePath);
}

void AndroidBuildApkStep::doRun()
{
    if (m_skipBuilding) {
        reportWarningOrError(tr("Android deploy settings file not found, not building an APK."),
                             Task::Error);
        emit finished(true);
        return;
    }

    auto setup = [this] {
        const auto androidAbis = AndroidManager::applicationAbis(target());
        const QString buildKey = target()->activeBuildKey();

        QtSupport::QtVersion *version = QtSupport::QtKitAspect::qtVersion(kit());
        if (!version) {
            reportWarningOrError(tr("The Qt version for kit %1 is invalid.")
                                 .arg(kit()->displayName()), Task::Error);
            return false;
        }

        const FilePath buildDir = buildDirectory();
        const FilePath androidBuildDir = AndroidManager::androidBuildDirectory(target());
        for (const auto &abi : androidAbis) {
            FilePath androidLibsDir = androidBuildDir / "libs" / abi;
            if (!androidLibsDir.exists()) {
                if (!androidLibsDir.ensureWritableDir()) {
                    reportWarningOrError(tr("The Android build folder %1 was not found and could "
                                            "not be created.").arg(androidLibsDir.toUserOutput()),
                                         Task::Error);
                    return false;
                } else if (version->qtVersion() >= QtSupport::QtVersionNumber{6, 0, 0}
                           && version->qtVersion() <= QtSupport::QtVersionNumber{6, 1, 1}) {
                    // 6.0.x <= Qt <= 6.1.1 used to need a manaul call to _prepare_apk_dir target,
                    // and now it's made directly with ALL target, so this code below ensures
                    // these versions are not broken.
                    const QString fileName = QString("lib%1_%2.so").arg(buildKey, abi);
                    const FilePath from = buildDir / fileName;
                    const FilePath to = androidLibsDir / fileName;
                    if (!from.exists() || to.exists())
                        continue;

                    if (!from.copyFile(to)) {
                        reportWarningOrError(tr("Cannot copy the target's lib file %1 to the "
                                                "Android build folder %2.")
                                             .arg(fileName, androidLibsDir.toUserOutput()),
                                             Task::Error);
                        return false;
                    }
                }
            }

        }

        bool inputExists = m_inputFile.exists();
        if (inputExists && !AndroidManager::isQtCreatorGenerated(m_inputFile))
            return true; // use the generated file if it was not generated by qtcreator

        BuildSystem *bs = buildSystem();
        const FilePaths targets = Utils::transform(
                    bs->extraData(buildKey, Android::Constants::AndroidTargets).toStringList(),
                    &FilePath::fromUserInput);
        if (targets.isEmpty())
            return inputExists; // qmake does this job for us

        QJsonObject deploySettings = Android::AndroidManager::deploymentSettings(target());
        QString applicationBinary;
        if (!version->supportsMultipleQtAbis()) {
            QTC_ASSERT(androidAbis.size() == 1, return false);
            applicationBinary = buildSystem()->buildTarget(buildKey).targetFilePath.toString();
            FilePath androidLibsDir = androidBuildDir / "libs" / androidAbis.first();
            for (const FilePath &target : targets) {
                if (!copyFileIfNewer(target, androidLibsDir.pathAppended(target.fileName()))) {
                    reportWarningOrError(
                                tr("Cannot copy file \"%1\" to Android build libs folder \"%2\".")
                                .arg(target.toUserOutput()).arg(androidLibsDir.toUserOutput()),
                                Task::Error);
                    return false;
                }
            }
            deploySettings["target-architecture"] = androidAbis.first();
        } else {
            applicationBinary = buildSystem()->buildTarget(buildKey).targetFilePath.fileName();
            QJsonObject architectures;

            // Copy targets to android build folder
            for (const auto &abi : androidAbis) {
                QString targetSuffix = QString{"_%1.so"}.arg(abi);
                if (applicationBinary.endsWith(targetSuffix)) {
                    // Keep only TargetName from "lib[TargetName]_abi.so"
                    applicationBinary.remove(0, 3).chop(targetSuffix.size());
                }

                FilePath androidLibsDir = androidBuildDir / "libs" / abi;
                for (const FilePath &target : targets) {
                    if (target.endsWith(targetSuffix)) {
                        const FilePath destination = androidLibsDir.pathAppended(target.fileName());
                        if (!copyFileIfNewer(target, destination)) {
                            reportWarningOrError(
                                tr("Cannot copy file \"%1\" to Android build libs folder \"%2\".")
                                    .arg(target.toUserOutput()).arg(androidLibsDir.toUserOutput()),
                                Task::Error);
                            return false;
                        }
                        architectures[abi] = AndroidManager::archTriplet(abi);
                    }
                }
            }
            deploySettings["architectures"] = architectures;
        }
        deploySettings["application-binary"] = applicationBinary;

        QString extraLibs = bs->extraData(buildKey, Android::Constants::AndroidExtraLibs).toString();
        if (!extraLibs.isEmpty())
            deploySettings["android-extra-libs"] = extraLibs;

        QString androidSrcs = bs->extraData(buildKey, Android::Constants::AndroidPackageSourceDir).toString();
        if (!androidSrcs.isEmpty())
            deploySettings["android-package-source-directory"] = androidSrcs;

        QString qmlImportPath = bs->extraData(buildKey, "QML_IMPORT_PATH").toString();
        if (!qmlImportPath.isEmpty())
            deploySettings["qml-import-paths"] = qmlImportPath;

        QString qmlRootPath = bs->extraData(buildKey, "QML_ROOT_PATH").toString();
        if (qmlRootPath.isEmpty())
            qmlRootPath = target()->project()->rootProjectDirectory().toString();
         deploySettings["qml-root-path"] = qmlRootPath;

        QFile f{m_inputFile.toString()};
        if (!f.open(QIODevice::WriteOnly)) {
            reportWarningOrError(tr("Cannot open androiddeployqt input file \"%1\" for writing.")
                                 .arg(m_inputFile.toUserOutput()), Task::Error);
            return false;
        }
        f.write(QJsonDocument{deploySettings}.toJson());
        return true;
    };

    if (!setup()) {
        reportWarningOrError(tr("Cannot set up \"%1\", not building an APK.").arg(displayName()),
                             Task::Error);
        emit finished(false);
        return;
    }

    AbstractProcessStep::doRun();
}

void AndroidBuildApkStep::reportWarningOrError(const QString &message, Task::TaskType type)
{
    qCDebug(buildapkstepLog) << message;
    emit addOutput(message, OutputFormat::ErrorMessage);
    TaskHub::addTask(BuildSystemTask(type, message));
}

void AndroidBuildApkStep::processStarted()
{
    emit addOutput(tr("Starting: \"%1\" %2")
                   .arg(m_command.toUserOutput(), m_argumentsPasswordConcealed),
                   BuildStep::OutputFormat::NormalMessage);
}

bool AndroidBuildApkStep::fromMap(const QVariantMap &map)
{
    m_keystorePath = FilePath::fromVariant(map.value(KeystoreLocationKey));
    m_signPackage = false; // don't restore this
    m_buildTargetSdk = map.value(BuildTargetSdkKey).toString();
    if (m_buildTargetSdk.isEmpty()) {
        m_buildTargetSdk = AndroidConfig::apiLevelNameFor(AndroidConfigurations::
                                                          sdkManager()->latestAndroidSdkPlatform());
    }
    m_verbose = map.value(VerboseOutputKey).toBool();
    return ProjectExplorer::BuildStep::fromMap(map);
}

QVariantMap AndroidBuildApkStep::toMap() const
{
    QVariantMap map = ProjectExplorer::AbstractProcessStep::toMap();
    map.insert(KeystoreLocationKey, m_keystorePath.toVariant());
    map.insert(BuildTargetSdkKey, m_buildTargetSdk);
    map.insert(VerboseOutputKey, m_verbose);
    return map;
}

Utils::FilePath AndroidBuildApkStep::keystorePath()
{
    return m_keystorePath;
}

QString AndroidBuildApkStep::buildTargetSdk() const
{
    return m_buildTargetSdk;
}

void AndroidBuildApkStep::setBuildTargetSdk(const QString &sdk)
{
    m_buildTargetSdk = sdk;
}

void AndroidBuildApkStep::stdError(const QString &output)
{
    AbstractProcessStep::stdError(output);

    QString newOutput = output;
    newOutput.remove(QRegularExpression("^(\\n)+"));

    if (newOutput.isEmpty())
        return;

    if (newOutput.startsWith("warning", Qt::CaseInsensitive)
        || newOutput.startsWith("note", Qt::CaseInsensitive))
        TaskHub::addTask(BuildSystemTask(Task::Warning, newOutput));
    else
        TaskHub::addTask(BuildSystemTask(Task::Error, newOutput));
}

QVariant AndroidBuildApkStep::data(Utils::Id id) const
{
    if (id == Constants::AndroidNdkPlatform) {
        if (auto qtVersion = QtKitAspect::qtVersion(kit()))
            return AndroidConfigurations::currentConfig()
                .bestNdkPlatformMatch(AndroidManager::minimumSDK(target()), qtVersion).mid(8);
        return {};
    }
    if (id == Constants::NdkLocation) {
        if (auto qtVersion = QtKitAspect::qtVersion(kit()))
            return QVariant::fromValue(AndroidConfigurations::currentConfig().ndkLocation(qtVersion));
        return {};
    }
    if (id == Constants::SdkLocation)
        return QVariant::fromValue(AndroidConfigurations::currentConfig().sdkLocation());

    if (id == Constants::AndroidMkSpecAbis)
        return AndroidManager::applicationAbis(target());

    return AbstractProcessStep::data(id);
}

void AndroidBuildApkStep::setKeystorePath(const Utils::FilePath &path)
{
    m_keystorePath = path;
    m_certificatePasswd.clear();
    m_keystorePasswd.clear();
}

void AndroidBuildApkStep::setKeystorePassword(const QString &pwd)
{
    m_keystorePasswd = pwd;
}

void AndroidBuildApkStep::setCertificateAlias(const QString &alias)
{
    m_certificateAlias = alias;
}

void AndroidBuildApkStep::setCertificatePassword(const QString &pwd)
{
    m_certificatePasswd = pwd;
}

bool AndroidBuildApkStep::signPackage() const
{
    return m_signPackage;
}

void AndroidBuildApkStep::setSignPackage(bool b)
{
    m_signPackage = b;
}

bool AndroidBuildApkStep::buildAAB() const
{
    return m_buildAAB;
}

void AndroidBuildApkStep::setBuildAAB(bool aab)
{
    m_buildAAB = aab;
}

bool AndroidBuildApkStep::openPackageLocation() const
{
    return m_openPackageLocation;
}

void AndroidBuildApkStep::setOpenPackageLocation(bool open)
{
    m_openPackageLocation = open;
}

void AndroidBuildApkStep::setVerboseOutput(bool verbose)
{
    m_verbose = verbose;
}

bool AndroidBuildApkStep::addDebugger() const
{
    return m_addDebugger;
}

void AndroidBuildApkStep::setAddDebugger(bool debug)
{
    m_addDebugger = debug;
}

bool AndroidBuildApkStep::verboseOutput() const
{
    return m_verbose;
}

QAbstractItemModel *AndroidBuildApkStep::keystoreCertificates()
{
    // check keystore passwords
    if (!verifyKeystorePassword())
        return nullptr;

    CertificatesModel *model = nullptr;
    const QStringList params = {"-list", "-v", "-keystore", m_keystorePath.toUserOutput(),
        "-storepass", m_keystorePasswd, "-J-Duser.language=en"};

    QtcProcess keytoolProc;
    keytoolProc.setTimeoutS(30);
    keytoolProc.setCommand({AndroidConfigurations::currentConfig().keytoolPath(), params});
    keytoolProc.runBlocking(EventLoopMode::On);
    if (keytoolProc.result() > ProcessResult::FinishedWithError)
        QMessageBox::critical(nullptr, tr("Error"), tr("Failed to run keytool."));
    else
        model = new CertificatesModel(keytoolProc.cleanedStdOut(), this);

    return model;
}

PasswordInputDialog::PasswordInputDialog(PasswordInputDialog::Context context,
                                         std::function<bool (const QString &)> callback,
                                         const QString &extraContextStr,
                                         QWidget *parent) :
    QDialog(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint),
    verifyCallback(callback)

{
    inputEdit->setEchoMode(QLineEdit::Password);

    warningLabel->hide();

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(inputContextlabel);
    mainLayout->addWidget(inputEdit);
    mainLayout->addWidget(warningLabel);
    mainLayout->addWidget(buttonBox);

    connect(inputEdit, &QLineEdit::textChanged,[this](const QString &text) {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
    });

    connect(buttonBox, &QDialogButtonBox::accepted, [this]() {
        if (verifyCallback(inputEdit->text())) {
            accept(); // Dialog accepted.
        } else {
            warningLabel->show();
            inputEdit->clear();
            adjustSize();
        }
    });

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setWindowTitle(context == KeystorePassword ? tr("Keystore") : tr("Certificate"));

    QString contextStr;
    if (context == KeystorePassword)
        contextStr = tr("Enter keystore password");
    else
        contextStr = tr("Enter certificate password");

    contextStr += extraContextStr.isEmpty() ? QStringLiteral(":") :
                                              QStringLiteral(" (%1):").arg(extraContextStr);
    inputContextlabel->setText(contextStr);
}

QString PasswordInputDialog::getPassword(Context context, std::function<bool (const QString &)> callback,
                                         const QString &extraContextStr, bool *ok, QWidget *parent)
{
    std::unique_ptr<PasswordInputDialog> dlg(new PasswordInputDialog(context, callback,
                                                                     extraContextStr, parent));
    bool isAccepted = dlg->exec() == QDialog::Accepted;
    if (ok)
        *ok = isAccepted;
    return isAccepted ? dlg->inputEdit->text() : "";
}


// AndroidBuildApkStepFactory

AndroidBuildApkStepFactory::AndroidBuildApkStepFactory()
{
    registerStep<AndroidBuildApkStep>(Constants::ANDROID_BUILD_APK_ID);
    setSupportedDeviceType(Constants::ANDROID_DEVICE_TYPE);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
    setDisplayName(AndroidBuildApkStep::tr("Build Android APK"));
    setRepeatable(false);
}

} // namespace Internal
} // namespace Android
