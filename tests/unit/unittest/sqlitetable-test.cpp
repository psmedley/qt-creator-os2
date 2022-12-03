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

#include "googletest.h"
#include "spydummy.h"

#include <sqlitecolumn.h>
#include <sqlitedatabasemock.h>
#include <sqlitetable.h>

namespace {

using Sqlite::ColumnType;
using Sqlite::ConstraintType;
using Sqlite::Database;
using Sqlite::Enforment;
using Sqlite::ForeignKey;
using Sqlite::ForeignKeyAction;
using Sqlite::JournalMode;
using Sqlite::OpenMode;
using Sqlite::StrictColumnType;

class SqliteTable : public ::testing::Test
{
protected:
    using Column = Sqlite::Column;

    NiceMock<SqliteDatabaseMock> databaseMock;
    Sqlite::Table table;
    Utils::SmallString tableName = "testTable";
};


TEST_F(SqliteTable, ColumnIsAddedToTable)
{
    table.setUseWithoutRowId(true);

    ASSERT_TRUE(table.useWithoutRowId());
}

TEST_F(SqliteTable, SetTableName)
{
    table.setName(tableName.clone());

    ASSERT_THAT(table.name(), tableName);
}

TEST_F(SqliteTable, SetUseWithoutRowid)
{
    table.setUseWithoutRowId(true);

    ASSERT_TRUE(table.useWithoutRowId());
}

TEST_F(SqliteTable, AddIndex)
{
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");

    auto index = table.addIndex({column, column2});

    ASSERT_THAT(Utils::SmallStringView(index.sqlStatement()),
                Eq("CREATE INDEX IF NOT EXISTS index_testTable_name_value ON testTable(name, value)"));
}

TEST_F(SqliteTable, InitializeTable)
{
    table.setName(tableName.clone());
    table.setUseIfNotExists(true);
    table.setUseTemporaryTable(true);
    table.setUseWithoutRowId(true);
    table.addColumn("name");
    table.addColumn("value");

    EXPECT_CALL(databaseMock,
                execute(Eq(
                    "CREATE TEMPORARY TABLE IF NOT EXISTS testTable(name, value) WITHOUT ROWID")));

    table.initialize(databaseMock);
}

TEST_F(SqliteTable, InitializeTableWithIndex)
{
    InSequence sequence;
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");
    table.addIndex({column});
    table.addIndex({column2}, "value IS NOT NULL");

    EXPECT_CALL(databaseMock, execute(Eq("CREATE TABLE testTable(name, value)")));
    EXPECT_CALL(databaseMock, execute(Eq("CREATE INDEX IF NOT EXISTS index_testTable_name ON testTable(name)")));
    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE INDEX IF NOT EXISTS index_testTable_value ON testTable(value) "
                           "WHERE value IS NOT NULL")));

    table.initialize(databaseMock);
}

TEST_F(SqliteTable, InitializeTableWithUniqueIndex)
{
    InSequence sequence;
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");
    table.addUniqueIndex({column});
    table.addUniqueIndex({column2}, "value IS NOT NULL");

    EXPECT_CALL(databaseMock, execute(Eq("CREATE TABLE testTable(name, value)")));
    EXPECT_CALL(databaseMock,
                execute(Eq(
                    "CREATE UNIQUE INDEX IF NOT EXISTS index_testTable_name ON testTable(name)")));
    EXPECT_CALL(databaseMock,
                execute(Eq(
                    "CREATE UNIQUE INDEX IF NOT EXISTS index_testTable_value ON testTable(value) "
                    "WHERE value IS NOT NULL")));

    table.initialize(databaseMock);
}

TEST_F(SqliteTable, AddForeignKeyColumnWithTableCalls)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");
    table.setName(tableName);
    table.addForeignKeyColumn("name",
                              foreignTable,
                              ForeignKeyAction::SetNull,
                              ForeignKeyAction::Cascade,
                              Enforment::Deferred);

    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE TABLE testTable(name INTEGER REFERENCES foreignTable ON UPDATE "
                           "SET NULL ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED)")));

    table.initialize(databaseMock);
}

TEST_F(SqliteTable, AddForeignKeyColumnWithColumnCalls)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn", ColumnType::Text, {Sqlite::Unique{}});
    table.setName(tableName);
    table.addForeignKeyColumn("name",
                              foreignColumn,
                              ForeignKeyAction::SetDefault,
                              ForeignKeyAction::Restrict,
                              Enforment::Deferred);

    EXPECT_CALL(
        databaseMock,
        execute(
            Eq("CREATE TABLE testTable(name TEXT REFERENCES foreignTable(foreignColumn) ON UPDATE "
               "SET DEFAULT ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED)")));

    table.initialize(databaseMock);
}

TEST_F(SqliteTable, AddColumn)
{
    table.setName(tableName);

    auto &column = table.addColumn("name", ColumnType::Text, {Sqlite::Unique{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, ColumnType::Text),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<Sqlite::Unique>(Eq(Sqlite::Unique{}))))));
}

TEST_F(SqliteTable, AddForeignKeyColumnWithTable)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");

    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignTable,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred);

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, ColumnType::Integer),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<ForeignKey>(
                                AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                      Field(&ForeignKey::column, IsEmpty()),
                                      Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                      Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                      Field(&ForeignKey::enforcement, Enforment::Deferred)))))));
}

TEST_F(SqliteTable, AddForeignKeyColumnWithColumn)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn", ColumnType::Text, {Sqlite::Unique{}});
    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignColumn,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred);

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, ColumnType::Text),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<ForeignKey>(
                                AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                      Field(&ForeignKey::column, Eq("foreignColumn")),
                                      Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                      Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                      Field(&ForeignKey::enforcement, Enforment::Deferred)))))));
}

TEST_F(SqliteTable, AddForeignKeyWhichIsNotUniqueThrowsAnExceptions)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn", ColumnType::Text);
    table.setName(tableName);

    ASSERT_THROW(table.addForeignKeyColumn("name",
                                           foreignColumn,
                                           ForeignKeyAction::SetNull,
                                           ForeignKeyAction::Cascade,
                                           Enforment::Deferred),
                 Sqlite::ForeignKeyColumnIsNotUnique);
}

TEST_F(SqliteTable, AddForeignKeyColumnWithTableAndNotNull)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");

    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignTable,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred,
                                             {Sqlite::NotNull{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, ColumnType::Integer),
                      Field(&Column::constraints,
                            UnorderedElementsAre(
                                VariantWith<ForeignKey>(
                                    AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                          Field(&ForeignKey::column, IsEmpty()),
                                          Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                          Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                          Field(&ForeignKey::enforcement, Enforment::Deferred))),
                                VariantWith<Sqlite::NotNull>(Eq(Sqlite::NotNull{}))))));
}

TEST_F(SqliteTable, AddForeignKeyColumnWithColumnAndNotNull)
{
    Sqlite::Table foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn", ColumnType::Text, {Sqlite::Unique{}});
    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignColumn,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred,
                                             {Sqlite::NotNull{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, ColumnType::Text),
                      Field(&Column::constraints,
                            UnorderedElementsAre(
                                VariantWith<ForeignKey>(
                                    AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                          Field(&ForeignKey::column, Eq("foreignColumn")),
                                          Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                          Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                          Field(&ForeignKey::enforcement, Enforment::Deferred))),
                                VariantWith<Sqlite::NotNull>(Eq(Sqlite::NotNull{}))))));
}

TEST_F(SqliteTable, AddPrimaryTableContraint)
{
    table.setName(tableName.clone());
    const auto &idColumn = table.addColumn("id");
    const auto &nameColumn = table.addColumn("name");
    table.addPrimaryKeyContraint({idColumn, nameColumn});

    EXPECT_CALL(databaseMock, execute(Eq("CREATE TABLE testTable(id, name, PRIMARY KEY(id, name))")));

    table.initialize(databaseMock);
}

class StrictSqliteTable : public ::testing::Test
{
protected:
    using Column = Sqlite::StrictColumn;

    NiceMock<SqliteDatabaseMock> databaseMock;
    Sqlite::StrictTable table;
    Utils::SmallString tableName = "testTable";
};

TEST_F(StrictSqliteTable, ColumnIsAddedToTable)
{
    table.setUseWithoutRowId(true);

    ASSERT_TRUE(table.useWithoutRowId());
}

TEST_F(StrictSqliteTable, SetTableName)
{
    table.setName(tableName.clone());

    ASSERT_THAT(table.name(), tableName);
}

TEST_F(StrictSqliteTable, SetUseWithoutRowid)
{
    table.setUseWithoutRowId(true);

    ASSERT_TRUE(table.useWithoutRowId());
}

TEST_F(StrictSqliteTable, AddIndex)
{
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");

    auto index = table.addIndex({column, column2});

    ASSERT_THAT(Utils::SmallStringView(index.sqlStatement()),
                Eq("CREATE INDEX IF NOT EXISTS index_testTable_name_value ON testTable(name, "
                   "value)"));
}

TEST_F(StrictSqliteTable, InitializeTable)
{
    table.setName(tableName.clone());
    table.setUseIfNotExists(true);
    table.setUseTemporaryTable(true);
    table.setUseWithoutRowId(true);
    table.addColumn("name");
    table.addColumn("value");

    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE TEMPORARY TABLE IF NOT EXISTS testTable(name ANY, value ANY) "
                           "WITHOUT ROWID STRICT")));

    table.initialize(databaseMock);
}

TEST_F(StrictSqliteTable, InitializeTableWithIndex)
{
    InSequence sequence;
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");
    table.addIndex({column});
    table.addIndex({column2}, "value IS NOT NULL");

    EXPECT_CALL(databaseMock, execute(Eq("CREATE TABLE testTable(name ANY, value ANY) STRICT")));
    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE INDEX IF NOT EXISTS index_testTable_name ON testTable(name)")));
    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE INDEX IF NOT EXISTS index_testTable_value ON testTable(value) "
                           "WHERE value IS NOT NULL")));

    table.initialize(databaseMock);
}

TEST_F(StrictSqliteTable, InitializeTableWithUniqueIndex)
{
    InSequence sequence;
    table.setName(tableName.clone());
    auto &column = table.addColumn("name");
    auto &column2 = table.addColumn("value");
    table.addUniqueIndex({column});
    table.addUniqueIndex({column2}, "value IS NOT NULL");

    EXPECT_CALL(databaseMock, execute(Eq("CREATE TABLE testTable(name ANY, value ANY) STRICT")));
    EXPECT_CALL(databaseMock,
                execute(Eq(
                    "CREATE UNIQUE INDEX IF NOT EXISTS index_testTable_name ON testTable(name)")));
    EXPECT_CALL(databaseMock,
                execute(Eq(
                    "CREATE UNIQUE INDEX IF NOT EXISTS index_testTable_value ON testTable(value) "
                    "WHERE value IS NOT NULL")));

    table.initialize(databaseMock);
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithTableCalls)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");
    table.setName(tableName);
    table.addForeignKeyColumn("name",
                              foreignTable,
                              ForeignKeyAction::SetNull,
                              ForeignKeyAction::Cascade,
                              Enforment::Deferred);

    EXPECT_CALL(databaseMock,
                execute(Eq("CREATE TABLE testTable(name INTEGER REFERENCES foreignTable ON UPDATE "
                           "SET NULL ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED) STRICT")));

    table.initialize(databaseMock);
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithColumnCalls)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn",
                                                 StrictColumnType::Text,
                                                 {Sqlite::Unique{}});
    table.setName(tableName);
    table.addForeignKeyColumn("name",
                              foreignColumn,
                              ForeignKeyAction::SetDefault,
                              ForeignKeyAction::Restrict,
                              Enforment::Deferred);

    EXPECT_CALL(
        databaseMock,
        execute(
            Eq("CREATE TABLE testTable(name TEXT REFERENCES foreignTable(foreignColumn) ON UPDATE "
               "SET DEFAULT ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED) STRICT")));

    table.initialize(databaseMock);
}

TEST_F(StrictSqliteTable, AddColumn)
{
    table.setName(tableName);

    auto &column = table.addColumn("name", StrictColumnType::Text, {Sqlite::Unique{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, StrictColumnType::Text),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<Sqlite::Unique>(Eq(Sqlite::Unique{}))))));
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithTable)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");

    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignTable,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred);

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, StrictColumnType::Integer),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<ForeignKey>(
                                AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                      Field(&ForeignKey::column, IsEmpty()),
                                      Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                      Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                      Field(&ForeignKey::enforcement, Enforment::Deferred)))))));
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithColumn)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn",
                                                 StrictColumnType::Text,
                                                 {Sqlite::Unique{}});
    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignColumn,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred);

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, StrictColumnType::Text),
                      Field(&Column::constraints,
                            ElementsAre(VariantWith<ForeignKey>(
                                AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                      Field(&ForeignKey::column, Eq("foreignColumn")),
                                      Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                      Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                      Field(&ForeignKey::enforcement, Enforment::Deferred)))))));
}

TEST_F(StrictSqliteTable, AddForeignKeyWhichIsNotUniqueThrowsAnExceptions)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn", StrictColumnType::Text);
    table.setName(tableName);

    ASSERT_THROW(table.addForeignKeyColumn("name",
                                           foreignColumn,
                                           ForeignKeyAction::SetNull,
                                           ForeignKeyAction::Cascade,
                                           Enforment::Deferred),
                 Sqlite::ForeignKeyColumnIsNotUnique);
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithTableAndNotNull)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");

    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignTable,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred,
                                             {Sqlite::NotNull{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, StrictColumnType::Integer),
                      Field(&Column::constraints,
                            UnorderedElementsAre(
                                VariantWith<ForeignKey>(
                                    AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                          Field(&ForeignKey::column, IsEmpty()),
                                          Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                          Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                          Field(&ForeignKey::enforcement, Enforment::Deferred))),
                                VariantWith<Sqlite::NotNull>(Eq(Sqlite::NotNull{}))))));
}

TEST_F(StrictSqliteTable, AddForeignKeyColumnWithColumnAndNotNull)
{
    Sqlite::StrictTable foreignTable;
    foreignTable.setName("foreignTable");
    auto &foreignColumn = foreignTable.addColumn("foreignColumn",
                                                 StrictColumnType::Text,
                                                 {Sqlite::Unique{}});
    table.setName(tableName);

    auto &column = table.addForeignKeyColumn("name",
                                             foreignColumn,
                                             ForeignKeyAction::SetNull,
                                             ForeignKeyAction::Cascade,
                                             Enforment::Deferred,
                                             {Sqlite::NotNull{}});

    ASSERT_THAT(column,
                AllOf(Field(&Column::name, Eq("name")),
                      Field(&Column::tableName, Eq(tableName)),
                      Field(&Column::type, StrictColumnType::Text),
                      Field(&Column::constraints,
                            UnorderedElementsAre(
                                VariantWith<ForeignKey>(
                                    AllOf(Field(&ForeignKey::table, Eq("foreignTable")),
                                          Field(&ForeignKey::column, Eq("foreignColumn")),
                                          Field(&ForeignKey::updateAction, ForeignKeyAction::SetNull),
                                          Field(&ForeignKey::deleteAction, ForeignKeyAction::Cascade),
                                          Field(&ForeignKey::enforcement, Enforment::Deferred))),
                                VariantWith<Sqlite::NotNull>(Eq(Sqlite::NotNull{}))))));
}

TEST_F(StrictSqliteTable, AddPrimaryTableContraint)
{
    table.setName(tableName.clone());
    const auto &idColumn = table.addColumn("id");
    const auto &nameColumn = table.addColumn("name");
    table.addPrimaryKeyContraint({idColumn, nameColumn});

    EXPECT_CALL(databaseMock,
                execute(
                    Eq("CREATE TABLE testTable(id ANY, name ANY, PRIMARY KEY(id, name)) STRICT")));

    table.initialize(databaseMock);
}
} // namespace
