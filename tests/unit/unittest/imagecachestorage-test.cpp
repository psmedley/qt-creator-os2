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

#include "sqlitedatabasemock.h"

#include <imagecachestorage.h>
#include <sqlitedatabase.h>

namespace {

MATCHER_P(IsIcon, icon, std::string(negation ? "is't" : "is") + PrintToString(icon))
{
    return arg.availableSizes() == icon.availableSizes();
}
class ImageCacheStorageTest : public testing::Test
{
protected:
    template<int ResultCount, int BindParameterCount = 0>
    using ReadStatement = QmlDesigner::ImageCacheStorage<
        SqliteDatabaseMock>::ReadStatement<ResultCount, BindParameterCount>;
    template<int BindParameterCount>
    using WriteStatement = QmlDesigner::ImageCacheStorage<SqliteDatabaseMock>::WriteStatement<BindParameterCount>;

    NiceMock<SqliteDatabaseMock> databaseMock;
    QmlDesigner::ImageCacheStorage<SqliteDatabaseMock> storage{databaseMock};
    ReadStatement<1, 2> &selectImageStatement = storage.selectImageStatement;
    ReadStatement<1, 2> &selectSmallImageStatement = storage.selectSmallImageStatement;
    ReadStatement<1, 2> &selectIconStatement = storage.selectIconStatement;
    WriteStatement<4> &upsertImageStatement = storage.upsertImageStatement;
    WriteStatement<3> &upsertIconStatement = storage.upsertIconStatement;
    QImage image1{10, 10, QImage::Format_ARGB32};
    QImage smallImage1{10, 10, QImage::Format_ARGB32};
    QIcon icon1{QPixmap::fromImage(image1)};
};

TEST_F(ImageCacheStorageTest, FetchImageCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchImage("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, FetchImageCallsIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)))
        .WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, rollback());
    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchImage("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, FetchSmallImageCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectSmallImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchSmallImage("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, FetchSmallImageCallsIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectSmallImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)))
        .WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, rollback());
    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectSmallImageStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchSmallImage("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, FetchIconCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectIconStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchIcon("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, FetchIconCallsIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectIconStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)))
        .WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, rollback());
    EXPECT_CALL(databaseMock, deferredBegin());
    EXPECT_CALL(selectIconStatement,
                valueReturnBlob(TypedEq<Utils::SmallStringView>("/path/to/component"),
                                TypedEq<long long>(123)));
    EXPECT_CALL(databaseMock, commit());

    storage.fetchIcon("/path/to/component", {123});
}

TEST_F(ImageCacheStorageTest, StoreImageCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertImageStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      Not(IsEmpty()),
                      Not(IsEmpty())));
    EXPECT_CALL(databaseMock, commit());

    storage.storeImage("/path/to/component", {123}, image1, smallImage1);
}

TEST_F(ImageCacheStorageTest, StoreEmptyImageCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertImageStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      IsEmpty(),
                      IsEmpty()));
    EXPECT_CALL(databaseMock, commit());

    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});
}

TEST_F(ImageCacheStorageTest, StoreImageCallsIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin()).WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertImageStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      IsEmpty(),
                      IsEmpty()));
    EXPECT_CALL(databaseMock, commit());

    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});
}

TEST_F(ImageCacheStorageTest, StoreIconCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertIconStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      A<Sqlite::BlobView>()));
    EXPECT_CALL(databaseMock, commit());

    storage.storeIcon("/path/to/component", {123}, icon1);
}

TEST_F(ImageCacheStorageTest, StoreEmptyIconCalls)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertIconStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      IsEmpty()));
    EXPECT_CALL(databaseMock, commit());

    storage.storeIcon("/path/to/component", {123}, QIcon{});
}

TEST_F(ImageCacheStorageTest, StoreIconCallsIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, immediateBegin()).WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, immediateBegin());
    EXPECT_CALL(upsertIconStatement,
                write(TypedEq<Utils::SmallStringView>("/path/to/component"),
                      TypedEq<long long>(123),
                      IsEmpty()));
    EXPECT_CALL(databaseMock, commit());

    storage.storeIcon("/path/to/component", {123}, QIcon{});
}

TEST_F(ImageCacheStorageTest, CallWalCheckointFull)
{
    EXPECT_CALL(databaseMock, walCheckpointFull());

    storage.walCheckpointFull();
}

TEST_F(ImageCacheStorageTest, CallWalCheckointFullIsBusy)
{
    InSequence s;

    EXPECT_CALL(databaseMock, walCheckpointFull()).WillOnce(Throw(Sqlite::StatementIsBusy("busy")));
    EXPECT_CALL(databaseMock, walCheckpointFull());

    storage.walCheckpointFull();
}

class ImageCacheStorageSlowTest : public testing::Test
{
protected:
    QImage createImage()
    {
        QImage image{150, 150, QImage::Format_ARGB32};
        image.fill(QColor{128, 64, 0, 11});
        image.setPixelColor(1, 1, QColor{1, 255, 33, 196});

        return image;
    }

protected:
    Sqlite::Database database{":memory:", Sqlite::JournalMode::Memory};
    QmlDesigner::ImageCacheStorage<Sqlite::Database> storage{database};
    QImage image1{createImage()};
    QImage smallImage1{image1.scaled(96, 96)};
    QIcon icon1{QPixmap::fromImage(image1)};
};

TEST_F(ImageCacheStorageSlowTest, StoreImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    ASSERT_THAT(storage.fetchImage("/path/to/component", {123}), Optional(image1));
}

TEST_F(ImageCacheStorageSlowTest, StoreEmptyImageAfterEntry)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});

    ASSERT_THAT(storage.fetchImage("/path/to/component", {123}), Optional(QImage{}));
}

TEST_F(ImageCacheStorageSlowTest, StoreEmptyEntry)
{
    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});

    ASSERT_THAT(storage.fetchImage("/path/to/component", {123}), Optional(QImage{}));
}

TEST_F(ImageCacheStorageSlowTest, FetchNonExistingImageIsEmpty)
{
    auto image = storage.fetchImage("/path/to/component", {123});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchSameTimeImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchImage("/path/to/component", {123});

    ASSERT_THAT(image, Optional(image1));
}

TEST_F(ImageCacheStorageSlowTest, DoNotFetchOlderImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchImage("/path/to/component", {124});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchNewerImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchImage("/path/to/component", {122});

    ASSERT_THAT(image, Optional(image1));
}

TEST_F(ImageCacheStorageSlowTest, FetchNonExistingSmallImageIsEmpty)
{
    auto image = storage.fetchSmallImage("/path/to/component", {123});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchSameTimeSmallImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchSmallImage("/path/to/component", {123});

    ASSERT_THAT(image, Optional(smallImage1));
}

TEST_F(ImageCacheStorageSlowTest, DoNotFetchOlderSmallImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchSmallImage("/path/to/component", {124});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchNewerSmallImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto image = storage.fetchSmallImage("/path/to/component", {122});

    ASSERT_THAT(image, Optional(smallImage1));
}

TEST_F(ImageCacheStorageSlowTest, StoreIcon)
{
    storage.storeIcon("/path/to/component", {123}, icon1);

    ASSERT_THAT(storage.fetchIcon("/path/to/component", {123}), Optional(IsIcon(icon1)));
}

TEST_F(ImageCacheStorageSlowTest, StoreEmptyIconAfterEntry)
{
    storage.storeIcon("/path/to/component", {123}, icon1);

    storage.storeIcon("/path/to/component", {123}, QIcon{});

    ASSERT_THAT(storage.fetchIcon("/path/to/component", {123}), Optional(IsIcon(QIcon{})));
}

TEST_F(ImageCacheStorageSlowTest, StoreEmptyIconEntry)
{
    storage.storeIcon("/path/to/component", {123}, QIcon{});

    ASSERT_THAT(storage.fetchIcon("/path/to/component", {123}), Optional(IsIcon(QIcon{})));
}

TEST_F(ImageCacheStorageSlowTest, FetchNonExistingIconIsEmpty)
{
    auto image = storage.fetchIcon("/path/to/component", {123});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchSameTimeIcon)
{
    storage.storeIcon("/path/to/component", {123}, icon1);

    auto image = storage.fetchIcon("/path/to/component", {123});

    ASSERT_THAT(image, Optional(IsIcon(icon1)));
}

TEST_F(ImageCacheStorageSlowTest, DoNotFetchOlderIcon)
{
    storage.storeIcon("/path/to/component", {123}, icon1);

    auto image = storage.fetchIcon("/path/to/component", {124});

    ASSERT_THAT(image, Eq(Utils::nullopt));
}

TEST_F(ImageCacheStorageSlowTest, FetchNewerIcon)
{
    storage.storeIcon("/path/to/component", {123}, icon1);

    auto image = storage.fetchIcon("/path/to/component", {122});

    ASSERT_THAT(image, Optional(IsIcon(icon1)));
}

TEST_F(ImageCacheStorageSlowTest, FetchModifiedImageTime)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto timeStamp = storage.fetchModifiedImageTime("/path/to/component");

    ASSERT_THAT(timeStamp, Eq(Sqlite::TimeStamp{123}));
}

TEST_F(ImageCacheStorageSlowTest, FetchInvalidModifiedImageTimeForNoEntry)
{
    storage.storeImage("/path/to/component2", {123}, image1, smallImage1);

    auto timeStamp = storage.fetchModifiedImageTime("/path/to/component");

    ASSERT_THAT(timeStamp, Eq(Sqlite::TimeStamp{}));
}

TEST_F(ImageCacheStorageSlowTest, FetchHasImage)
{
    storage.storeImage("/path/to/component", {123}, image1, smallImage1);

    auto hasImage = storage.fetchHasImage("/path/to/component");

    ASSERT_TRUE(hasImage);
}

TEST_F(ImageCacheStorageSlowTest, FetchHasImageForNullImage)
{
    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});

    auto hasImage = storage.fetchHasImage("/path/to/component");

    ASSERT_FALSE(hasImage);
}

TEST_F(ImageCacheStorageSlowTest, FetchHasImageForNoEntry)
{
    storage.storeImage("/path/to/component", {123}, QImage{}, QImage{});

    auto hasImage = storage.fetchHasImage("/path/to/component");

    ASSERT_FALSE(hasImage);
}
} // namespace
