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

#include "dockerplugin.h"

#include "dockerconstants.h"

#include "dockerapi.h"
#include "dockerdevice.h"
#include "dockersettings.h"

#include <projectexplorer/projectexplorerconstants.h>

#include <utils/qtcassert.h>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace Docker {
namespace Internal {

class DockerPluginPrivate
{
public:
    // DockerSettings settings;
    // DockerOptionsPage optionsPage{&settings};

    DockerDeviceFactory deviceFactory;

    // DockerBuildStepFactory buildStepFactory;
    Utils::optional<bool> daemonRunning;

    DockerApi dockerApi;
};

static DockerPlugin *s_instance = nullptr;

DockerPlugin::DockerPlugin()
{
    s_instance = this;
}

DockerApi *DockerPlugin::dockerApi()
{
    QTC_ASSERT(s_instance, return nullptr);
    return &s_instance->d->dockerApi;
}

DockerPlugin::~DockerPlugin()
{
    s_instance = nullptr;
    delete d;
}

bool DockerPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    d = new DockerPluginPrivate;

    return true;
}

} // namespace Internal
} // namespace Docker
