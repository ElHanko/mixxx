#include "library/export/rekordboxcollectionxmlfile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QVariant>
#include <QXmlStreamReader>

#include <gtest/gtest.h>

#include "track/track.h"

namespace {

TEST(RekordboxCollectionXmlFileTest, ReportsTargetOpenError) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString fileName = temporaryDirectory.filePath("missing/collection.xml");

    QString error;
    EXPECT_FALSE(mixxx::writeRekordboxCollectionXmlFile(fileName, {}, &error));
    EXPECT_TRUE(error.startsWith(QStringLiteral("Could not open XML output file")));
    EXPECT_FALSE(QFile::exists(fileName));
}

TEST(RekordboxCollectionXmlFileTest, PropagatesSerializerError) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString fileName = temporaryDirectory.filePath("collection.xml");
    const TrackPointerList tracks{Track::newDummy(QString(), TrackId(QVariant(1)))};

    QString error;
    EXPECT_FALSE(mixxx::writeRekordboxCollectionXmlFile(fileName, tracks, &error));
    EXPECT_EQ(error, QStringLiteral("Cannot write a track without a location."));
    EXPECT_FALSE(QFile::exists(fileName));
}

TEST(RekordboxCollectionXmlFileTest, WritesEmptyCollection) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString fileName = temporaryDirectory.filePath("collection.xml");

    QString error;
    ASSERT_TRUE(mixxx::writeRekordboxCollectionXmlFile(fileName, {}, &error));
    EXPECT_TRUE(error.isEmpty());

    QFile file(fileName);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(&file);
    ASSERT_TRUE(xml.readNextStartElement());
    EXPECT_EQ(xml.name().toString(), QStringLiteral("DJ_PLAYLISTS"));
    ASSERT_TRUE(xml.readNextStartElement());
    EXPECT_EQ(xml.name().toString(), QStringLiteral("PRODUCT"));
    xml.skipCurrentElement();
    ASSERT_FALSE(xml.hasError());
    ASSERT_TRUE(xml.readNextStartElement());
    EXPECT_EQ(xml.name().toString(), QStringLiteral("COLLECTION"));
    EXPECT_EQ(xml.attributes().value(QStringLiteral("Entries")).toString(), QStringLiteral("0"));
    EXPECT_FALSE(xml.hasError());
}

} // namespace
