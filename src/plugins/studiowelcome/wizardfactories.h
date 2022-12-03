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

#include <utils/id.h>

namespace Core {
class IWizardFactory;
}

namespace ProjectExplorer {
class JsonWizardFactory;
}

using ProjectExplorer::JsonWizardFactory;

namespace StudioWelcome {

class WizardFactories
{
public:
    using GetIconUnicodeFunc = QString (*)(const QString &);

public:
    WizardFactories(const QList<Core::IWizardFactory *> &factories, QWidget *wizardParent,
                    const Utils::Id &platform);

    const Core::IWizardFactory *front() const;
    const std::map<QString, WizardCategory> &presetsGroupedByCategory() const
    { return m_presetItems; }

    bool empty() const { return m_factories.empty(); }
    static GetIconUnicodeFunc setIconUnicodeCallback(GetIconUnicodeFunc cb)
    {
        return std::exchange(m_getIconUnicode, cb);
    }

private:
    void sortByCategoryAndId();
    void filter();

    std::shared_ptr<PresetItem> makePresetItem(JsonWizardFactory *f, QWidget *parent, const Utils::Id &platform);
    std::map<QString, WizardCategory> makePresetItemsGroupedByCategory();

private:
    QWidget *m_wizardParent;
    Utils::Id m_platform; // filter wizards to only those supported by this platform.

    QList<JsonWizardFactory *> m_factories;
    std::map<QString, WizardCategory> m_presetItems;

    static GetIconUnicodeFunc m_getIconUnicode;
};

} // namespace StudioWelcome
