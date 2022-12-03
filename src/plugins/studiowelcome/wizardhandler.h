/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#pragma once

#include "presetmodel.h"

#include <utils/filepath.h>
#include <utils/infolabel.h>

QT_BEGIN_NAMESPACE
class QWizard;
class QWizardPage;
class QStandardItemModel;
QT_END_NAMESPACE

namespace ProjectExplorer {
class JsonFieldPage;
}

namespace StudioWelcome {

class WizardHandler: public QObject
{
    Q_OBJECT

public:
    void reset(const std::shared_ptr<PresetItem> &presetInfo, int presetSelection);
    void setScreenSizeIndex(int index);
    int screenSizeIndex(const QString &sizeName) const;
    QString screenSizeName(int index) const;
    int screenSizeIndex() const;
    int targetQtVersionIndex() const;
    int targetQtVersionIndex(const QString &qtVersionName) const;
    void setTargetQtVersionIndex(int index);
    bool haveTargetQtVersion() const;
    QString targetQtVersionName(int index) const;

    void setStyleIndex(int index);
    int styleIndex() const;
    int styleIndex(const QString &styleName) const;
    QString styleName(int index) const;
    bool haveStyleModel() const;

    void destroyWizard();

    void setUseVirtualKeyboard(bool value);
    bool haveVirtualKeyboard() const;

    void setProjectName(const QString &name);
    void setProjectLocation(const Utils::FilePath &location);

    void run(const std::function<void (QWizardPage *)> &processPage);

    std::shared_ptr<PresetItem> preset() const { return m_preset; }

signals:
    void deletingWizard();
    void wizardCreated(QStandardItemModel *screenSizeModel, QStandardItemModel *styleModel);
    void statusMessageChanged(Utils::InfoLabel::InfoType type, const QString &message);
    void projectCanBeCreated(bool value);
    void wizardCreationFailed();

private:
    void setupWizard();
    void initializeProjectPage(QWizardPage *page);
    void initializeFieldsPage(QWizardPage *page);

    QStandardItemModel *getScreenFactorModel(ProjectExplorer::JsonFieldPage *page);
    QStandardItemModel *getStyleModel(ProjectExplorer::JsonFieldPage *page);

private slots:
    void onWizardResetting();
    void onProjectIntroCompleteChanged();

private:
    Utils::Wizard *m_wizard = nullptr;
    ProjectExplorer::JsonFieldPage *m_detailsPage = nullptr;

    int m_selectedPreset = -1;

    std::shared_ptr<PresetItem> m_preset;
    Utils::FilePath m_projectLocation;
};

} // StudioWelcome
