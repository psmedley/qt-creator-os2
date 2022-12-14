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

#include "generatedcodemodelsupport.h"
#include "cppmodelmanager.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace CPlusPlus;

namespace CppEditor {

class QObjectCache
{
public:
    bool contains(QObject *object) const
    {
        return m_cache.contains(object);
    }

    void insert(QObject *object)
    {
        QObject::connect(object, &QObject::destroyed, [this](QObject *dead) {
            m_cache.remove(dead);
        });
        m_cache.insert(object);
    }

private:
    QSet<QObject *> m_cache;
};

GeneratedCodeModelSupport::GeneratedCodeModelSupport(CppModelManager *modelmanager,
                                                     ProjectExplorer::ExtraCompiler *generator,
                                                     const Utils::FilePath &generatedFile) :
    AbstractEditorSupport(modelmanager, generator), m_generatedFileName(generatedFile),
    m_generator(generator)
{
    QLoggingCategory log("qtc.cppeditor.generatedcodemodelsupport", QtWarningMsg);
    qCDebug(log) << "ctor GeneratedCodeModelSupport for" << m_generator->source()
                 << generatedFile;

    connect(m_generator, &ProjectExplorer::ExtraCompiler::contentsChanged,
            this, &GeneratedCodeModelSupport::onContentsChanged, Qt::QueuedConnection);
    onContentsChanged(generatedFile);
}

GeneratedCodeModelSupport::~GeneratedCodeModelSupport()
{
    CppModelManager::instance()->emitAbstractEditorSupportRemoved(
                m_generatedFileName.toString());
    QLoggingCategory log("qtc.cppeditor.generatedcodemodelsupport", QtWarningMsg);
    qCDebug(log) << "dtor ~generatedcodemodelsupport for" << m_generatedFileName;
}

void GeneratedCodeModelSupport::onContentsChanged(const Utils::FilePath &file)
{
    if (file == m_generatedFileName) {
        notifyAboutUpdatedContents();
        updateDocument();
    }
}

QByteArray GeneratedCodeModelSupport::contents() const
{
    return m_generator->content(m_generatedFileName);
}

QString GeneratedCodeModelSupport::fileName() const
{
    return m_generatedFileName.toString();
}

QString GeneratedCodeModelSupport::sourceFileName() const
{
    return m_generator->source().toString();
}

void GeneratedCodeModelSupport::update(const QList<ProjectExplorer::ExtraCompiler *> &generators)
{
    static QObjectCache extraCompilerCache;

    CppModelManager * const mm = CppModelManager::instance();

    for (ExtraCompiler *generator : generators) {
        if (extraCompilerCache.contains(generator))
            continue;

        extraCompilerCache.insert(generator);
        generator->forEachTarget([mm, generator](const Utils::FilePath &generatedFile) {
            new GeneratedCodeModelSupport(mm, generator, generatedFile);
        });
    }
}

} // namespace CppEditor
