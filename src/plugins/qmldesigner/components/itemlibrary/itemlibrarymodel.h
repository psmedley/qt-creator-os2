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

#pragma once

#include <QAbstractListModel>
#include <QtQml/qqml.h>
#include <import.h>

QT_FORWARD_DECLARE_CLASS(QMimeData)

namespace QmlDesigner {

class ItemLibraryCategory;
class ItemLibraryEntry;
class ItemLibraryImport;
class ItemLibraryInfo;
class Model;

class ItemLibraryModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isAnyCategoryHidden READ isAnyCategoryHidden WRITE setIsAnyCategoryHidden NOTIFY isAnyCategoryHiddenChanged FINAL)
    Q_PROPERTY(QObject *itemsModel READ itemsModel WRITE setItemsModel NOTIFY itemsModelChanged)
    Q_PROPERTY(bool importUnimportedSelected READ importUnimportedSelected WRITE setImportUnimportedSelected NOTIFY importUnimportedSelectedChanged)

public:
    explicit ItemLibraryModel(QObject *parent = nullptr);
    ~ItemLibraryModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const;
    ItemLibraryImport *importByUrl(const QString &importName) const;

    void update(ItemLibraryInfo *itemLibraryInfo, Model *model);
    void updateUsedImports(const QList<Import> &usedImports);

    QMimeData *getMimeData(const ItemLibraryEntry &itemLibraryEntry);

    void setSearchText(const QString &searchText);
    void setFlowMode(bool);

    bool isAnyCategoryHidden() const;
    void setIsAnyCategoryHidden(bool state);

    bool importUnimportedSelected() const;
    void setImportUnimportedSelected(bool state);

    QObject *itemsModel() const;
    void setItemsModel(QObject *model);

    static void registerQmlTypes();
    static void saveExpandedState(bool expanded, const QString &sectionName);
    static bool loadExpandedState(const QString &sectionName);
    static void saveCategoryVisibleState(bool isVisible, const QString &categoryName, const QString
                                         &importName);
    static bool loadCategoryVisibleState(const QString &categoryName, const QString &importName);
    void selectImportFirstVisibleCategory();

    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void hideCategory(const QString &importUrl, const QString &categoryName);
    Q_INVOKABLE void showImportHiddenCategories(const QString &importUrl);
    Q_INVOKABLE void showAllHiddenCategories();
    Q_INVOKABLE void selectImportCategory(const QString &importUrl, int categoryIndex);

    Import entryToImport(const ItemLibraryEntry &entry);

    inline static QHash<QString, QString> categorySortingHash;

signals:
    void isAnyCategoryHiddenChanged();
    void importUnimportedSelectedChanged();
    void selectedCategoryChanged();
    void selectedImportUrlChanged();
    void itemsModelChanged();

private:
    void updateVisibility(bool *changed);
    void addRoleNames();
    void sortSections();
    void clearSections();
    void updateSelection();
    void clearSelectedCategory();

    QList<QPointer<ItemLibraryImport>> m_importList;
    QHash<int, QByteArray> m_roleNames;

    QString m_searchText;
    bool m_flowMode = false;
    bool m_isAnyCategoryHidden = false;
    bool m_importUnimportedSelected = false;
    QString m_selectedImportUrl;
    int m_selectedCategoryIndex = -1;
    QObject *m_itemsModel = nullptr; // items model for the horizontal layout

    inline static QHash<QString, bool> expandedStateHash;
    inline static QHash<QString, bool> categoryVisibleStateHash;
};

} // namespace QmlDesigner
