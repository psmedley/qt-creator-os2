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

#include "cpplocatordata.h"

#include "stringtable.h"

namespace CppEditor {

enum { MaxPendingDocuments = 10 };

CppLocatorData::CppLocatorData()
{
    m_search.setSymbolsToSearchFor(SymbolSearcher::Enums |
                                   SymbolSearcher::Classes |
                                   SymbolSearcher::Functions |
                                   SymbolSearcher::TypeAliases);
    m_pendingDocuments.reserve(MaxPendingDocuments);
}

void CppLocatorData::onDocumentUpdated(const CPlusPlus::Document::Ptr &document)
{
    QMutexLocker locker(&m_pendingDocumentsMutex);

    bool isPending = false;
    for (int i = 0, ei = m_pendingDocuments.size(); i < ei; ++i) {
        const CPlusPlus::Document::Ptr &doc = m_pendingDocuments.at(i);
        if (doc->fileName() == document->fileName()) {
            isPending = true;
            if (document->revision() >= doc->revision())
                m_pendingDocuments[i] = document;
            break;
        }
    }

    if (!isPending && QFileInfo(document->fileName()).suffix() != "moc")
        m_pendingDocuments.append(document);

    flushPendingDocument(false);
}

void CppLocatorData::onAboutToRemoveFiles(const QStringList &files)
{
    if (files.isEmpty())
        return;

    QMutexLocker locker(&m_pendingDocumentsMutex);

    for (const QString &file : files) {
        m_infosByFile.remove(file);

        for (int i = 0; i < m_pendingDocuments.size(); ++i) {
            if (m_pendingDocuments.at(i)->fileName() == file) {
                m_pendingDocuments.remove(i);
                break;
            }
        }
    }

    Internal::StringTable::scheduleGC();
    flushPendingDocument(false);
}

void CppLocatorData::flushPendingDocument(bool force) const
{
    // TODO: move this off the UI thread and into a future.
    if (!force && m_pendingDocuments.size() < MaxPendingDocuments)
        return;
    if (m_pendingDocuments.isEmpty())
        return;

    for (CPlusPlus::Document::Ptr doc : qAsConst(m_pendingDocuments))
        m_infosByFile.insert(Internal::StringTable::insert(doc->fileName()), m_search(doc));

    m_pendingDocuments.clear();
    m_pendingDocuments.reserve(MaxPendingDocuments);
}

} // namespace CppEditor
