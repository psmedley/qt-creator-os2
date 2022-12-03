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

#include "baseprojectwizarddialog.h"

#include <coreplugin/documentmanager.h>
#include <utils/projectintropage.h>

#include <QDir>

/*!
    \class ProjectExplorer::BaseProjectWizardDialog

    \brief The BaseProjectWizardDialog class is the base class for project
    wizards.

    Presents the introductory page and takes care of setting the folder chosen
    as default projects' folder should the user wish to do that.
*/

using namespace Utils;

namespace ProjectExplorer {

struct BaseProjectWizardDialogPrivate
{
    explicit BaseProjectWizardDialogPrivate(ProjectIntroPage *page, int id = -1)
        : desiredIntroPageId(id), introPage(page)
    {}

    const int desiredIntroPageId;
    ProjectIntroPage *introPage;
    int introPageId = -1;
    Id selectedPlatform;
    QSet<Id> requiredFeatureSet;
};


BaseProjectWizardDialog::BaseProjectWizardDialog(const Core::BaseFileWizardFactory *factory,
                                                 QWidget *parent,
                                                 const Core::WizardDialogParameters &parameters) :
    Core::BaseFileWizard(factory, parameters.extraValues(), parent),
    d(std::make_unique<BaseProjectWizardDialogPrivate>(new ProjectIntroPage))
{
    setFilePath(parameters.defaultPath());
    setSelectedPlatform(parameters.selectedPlatform());
    setRequiredFeatures(parameters.requiredFeatures());
    init();
}

BaseProjectWizardDialog::BaseProjectWizardDialog(const Core::BaseFileWizardFactory *factory,
                                                 ProjectIntroPage *introPage, int introId,
                                                 QWidget *parent,
                                                 const Core::WizardDialogParameters &parameters) :
    Core::BaseFileWizard(factory, parameters.extraValues(), parent),
    d(std::make_unique<BaseProjectWizardDialogPrivate>(introPage, introId))
{
    setFilePath(parameters.defaultPath());
    setSelectedPlatform(parameters.selectedPlatform());
    setRequiredFeatures(parameters.requiredFeatures());
    init();
}

void BaseProjectWizardDialog::init()
{
    if (d->introPageId == -1) {
        d->introPageId = addPage(d->introPage);
    } else {
        d->introPageId = d->desiredIntroPageId;
        setPage(d->desiredIntroPageId, d->introPage);
    }
    connect(this, &QDialog::accepted, this, &BaseProjectWizardDialog::slotAccepted);
}

BaseProjectWizardDialog::~BaseProjectWizardDialog() = default;

QString BaseProjectWizardDialog::projectName() const
{
    return d->introPage->projectName();
}

FilePath BaseProjectWizardDialog::filePath() const
{
    return d->introPage->filePath();
}

void BaseProjectWizardDialog::setIntroDescription(const QString &des)
{
    d->introPage->setDescription(des);
}

void BaseProjectWizardDialog::setFilePath(const FilePath &path)
{
    d->introPage->setFilePath(path);
}

void BaseProjectWizardDialog::setProjectName(const QString &name)
{
    d->introPage->setProjectName(name);
}

void BaseProjectWizardDialog::setProjectList(const QStringList &projectList)
{
    d->introPage->setProjectList(projectList);
}

void BaseProjectWizardDialog::setProjectDirectories(const FilePaths &directories)
{
    d->introPage->setProjectDirectories(directories);
}

void BaseProjectWizardDialog::setForceSubProject(bool force)
{
    introPage()->setForceSubProject(force);
}

void BaseProjectWizardDialog::slotAccepted()
{
    if (d->introPage->useAsDefaultPath()) {
        // Store the path as default path for new projects if desired.
        Core::DocumentManager::setProjectsDirectory(filePath());
        Core::DocumentManager::setUseProjectsDirectory(true);
    }
}

bool BaseProjectWizardDialog::validateCurrentPage()
{
    if (currentId() == d->introPageId)
        emit projectParametersChanged(d->introPage->projectName(), d->introPage->filePath().toString());
    return Core::BaseFileWizard::validateCurrentPage();
}

ProjectIntroPage *BaseProjectWizardDialog::introPage() const
{
    return d->introPage;
}

QString BaseProjectWizardDialog::uniqueProjectName(const FilePath &path)
{
    const QDir pathDir(path.toString());
    //: File path suggestion for a new project. If you choose
    //: to translate it, make sure it is a valid path name without blanks
    //: and using only ascii chars.
    const QString prefix = tr("untitled");
    for (unsigned i = 0; ; ++i) {
        QString name = prefix;
        if (i)
            name += QString::number(i);
        if (!pathDir.exists(name))
            return name;
    }
    return prefix;
}

void BaseProjectWizardDialog::addExtensionPages(const QList<QWizardPage *> &wizardPageList)
{
    for (QWizardPage *p : wizardPageList)
        addPage(p);
}

Id BaseProjectWizardDialog::selectedPlatform() const
{
    return d->selectedPlatform;
}

void BaseProjectWizardDialog::setSelectedPlatform(Id platform)
{
    d->selectedPlatform = platform;
}

QSet<Id> BaseProjectWizardDialog::requiredFeatures() const
{
    return d->requiredFeatureSet;
}

void BaseProjectWizardDialog::setRequiredFeatures(const QSet<Id> &featureSet)
{
    d->requiredFeatureSet = featureSet;
}

} // namespace ProjectExplorer
