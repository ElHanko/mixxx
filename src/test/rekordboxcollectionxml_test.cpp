#include "library/export/rekordboxcollectionxml.h"

#include <utility>

#include <QBuffer>
#include <QHash>
#include <QIODevice>
#include <QLocale>
#include <QStringList>
#include <QVariant>
#include <QXmlStreamReader>

#include <gtest/gtest.h>

#include "track/track.h"
#include "util/color/rgbcolor.h"

namespace {

struct XmlElement {
    QString name;
    QString parentName;
    QHash<QString, QString> attributes;
};

class FailingWriteDevice : public QIODevice {
  public:
    FailingWriteDevice() = default;

  protected:
    qint64 readData(char*, qint64) override {
        return -1;
    }

    qint64 writeData(const char*, qint64) override {
        return -1;
    }
};

TrackPointer makeTrack(const QString& location, int id) {
    auto pTrack = Track::newDummy(location, TrackId(QVariant(id)));
    pTrack->setTitle(QStringLiteral("Track title"));
    pTrack->setArtist(QStringLiteral("Track artist"));
    pTrack->setAlbum(QStringLiteral("Track album"));
    pTrack->updateGenre(QStringLiteral("Track genre"));
    pTrack->setAudioProperties(
            mixxx::audio::ChannelCount(2),
            mixxx::audio::SampleRate(44100),
            mixxx::audio::Bitrate(320),
            mixxx::Duration::fromSeconds(245.6));
    EXPECT_TRUE(pTrack->trySetBpm(123.45));
    pTrack->setKeyText(QStringLiteral("8A"));
    return pTrack;
}

QString serialize(const TrackPointerList& tracks) {
    QByteArray data;
    QBuffer buffer(&data);
    EXPECT_TRUE(buffer.open(QIODevice::WriteOnly));
    QString error;
    EXPECT_TRUE(mixxx::writeRekordboxCollectionXml(&buffer, tracks, &error));
    EXPECT_TRUE(error.isEmpty());
    buffer.close();
    return QString::fromUtf8(data);
}

QList<XmlElement> parseElements(const QString& xml) {
    QList<XmlElement> elements;
    QStringList elementStack;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
        case QXmlStreamReader::StartElement: {
            XmlElement element;
            element.name = reader.name().toString();
            element.parentName = elementStack.isEmpty() ? QString() : elementStack.constLast();
            for (const auto& attribute : reader.attributes()) {
                element.attributes.insert(
                        attribute.name().toString(), attribute.value().toString());
            }
            elements.append(std::move(element));
            elementStack.append(reader.name().toString());
            break;
        }
        case QXmlStreamReader::EndElement:
            elementStack.removeLast();
            break;
        default:
            break;
        }
    }
    EXPECT_FALSE(reader.hasError()) << reader.errorString().toStdString();
    return elements;
}

const XmlElement* findElement(
        const QList<XmlElement>& elements,
        const QString& name,
        const QString& parentName = QString()) {
    for (const auto& element : elements) {
        if (element.name == name && element.parentName == parentName) {
            return &element;
        }
    }
    return nullptr;
}

const XmlElement* findPositionMark(const QList<XmlElement>& elements, const QString& name) {
    for (const auto& element : elements) {
        if (element.name == QStringLiteral("POSITION_MARK") &&
                element.parentName == QStringLiteral("TRACK") &&
                element.attributes.value(QStringLiteral("Name")) == name) {
            return &element;
        }
    }
    return nullptr;
}

QList<const XmlElement*> positionMarks(const QList<XmlElement>& elements) {
    QList<const XmlElement*> marks;
    for (const auto& element : elements) {
        if (element.name == QStringLiteral("POSITION_MARK") &&
                element.parentName == QStringLiteral("TRACK")) {
            marks.append(&element);
        }
    }
    return marks;
}

} // namespace

TEST(RekordboxCollectionXmlTest, WritesTrackWithoutCues) {
    const auto pTrack = makeTrack(QStringLiteral("/music/normal-track.mp3"), 42);

    const QList<XmlElement> elements = parseElements(serialize({pTrack}));

    const auto* pDocument = findElement(elements, QStringLiteral("DJ_PLAYLISTS"));
    ASSERT_NE(pDocument, nullptr);
    EXPECT_EQ(
            pDocument->attributes.value(QStringLiteral("Version")),
            QStringLiteral("1.0.0"));
    const auto* pProduct = findElement(
            elements, QStringLiteral("PRODUCT"), QStringLiteral("DJ_PLAYLISTS"));
    ASSERT_NE(pProduct, nullptr);
    EXPECT_EQ(
            pProduct->attributes.value(QStringLiteral("Name")),
            QStringLiteral("rekordbox"));
    EXPECT_EQ(
            pProduct->attributes.value(QStringLiteral("Version")),
            QStringLiteral("6.0.0"));
    EXPECT_EQ(
            pProduct->attributes.value(QStringLiteral("Company")),
            QStringLiteral("AlphaTheta"));
    const auto* pCollection = findElement(
            elements, QStringLiteral("COLLECTION"), QStringLiteral("DJ_PLAYLISTS"));
    ASSERT_NE(pCollection, nullptr);
    EXPECT_EQ(
            pCollection->attributes.value(QStringLiteral("Entries")), QStringLiteral("1"));
    const auto* pXmlTrack = findElement(
            elements, QStringLiteral("TRACK"), QStringLiteral("COLLECTION"));
    ASSERT_NE(pXmlTrack, nullptr);
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("TrackID")), QStringLiteral("42"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Name")), QStringLiteral("Track title"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Artist")),
            QStringLiteral("Track artist"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Album")), QStringLiteral("Track album"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Genre")), QStringLiteral("Track genre"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Kind")), QStringLiteral("MP3 File"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("TotalTime")), QStringLiteral("246"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("SampleRate")), QStringLiteral("44100"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("BitRate")), QStringLiteral("320"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("AverageBpm")), QStringLiteral("123.45"));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Tonality")), pTrack->getKeyText());
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Location")),
            QStringLiteral("file://localhost/music/normal-track.mp3"));
    const auto* pPlaylists = findElement(
            elements, QStringLiteral("PLAYLISTS"), QStringLiteral("DJ_PLAYLISTS"));
    ASSERT_NE(pPlaylists, nullptr);
    const auto* pRootNode = findElement(
            elements, QStringLiteral("NODE"), QStringLiteral("PLAYLISTS"));
    ASSERT_NE(pRootNode, nullptr);
    EXPECT_EQ(pRootNode->attributes.value(QStringLiteral("Type")), QStringLiteral("0"));
    EXPECT_EQ(pRootNode->attributes.value(QStringLiteral("Name")), QStringLiteral("ROOT"));
    EXPECT_EQ(pRootNode->attributes.value(QStringLiteral("Count")), QStringLiteral("0"));
    EXPECT_TRUE(positionMarks(elements).isEmpty());
}

TEST(RekordboxCollectionXmlTest, WritesHotCueLoopAndMainCue) {
    const auto pTrack = makeTrack(QStringLiteral("/music/cues.mp3"), 7);
    const auto pHotCue = pTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            3,
            mixxx::audio::FramePos(44100),
            mixxx::audio::kInvalidFramePos,
            mixxx::RgbColor(0x102030));
    ASSERT_NE(pHotCue, nullptr);
    pHotCue->setLabel(QStringLiteral("Hot cue"));
    const auto pLoop = pTrack->createAndAddCue(
            mixxx::CueType::Loop,
            6,
            mixxx::audio::FramePos(88200),
            mixxx::audio::FramePos(176400),
            mixxx::RgbColor(0x405060));
    ASSERT_NE(pLoop, nullptr);
    pLoop->setLabel(QStringLiteral("Saved loop"));
    pTrack->setMainCuePosition(mixxx::audio::FramePos(132300));

    const QList<XmlElement> elements = parseElements(serialize({pTrack}));

    const auto* pHotCueMark = findPositionMark(elements, QStringLiteral("Hot cue"));
    ASSERT_NE(pHotCueMark, nullptr);
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Type")), QStringLiteral("0"));
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Start")), QStringLiteral("1.000"));
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Num")), QStringLiteral("3"));
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Red")), QStringLiteral("16"));
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Green")), QStringLiteral("32"));
    EXPECT_EQ(pHotCueMark->attributes.value(QStringLiteral("Blue")), QStringLiteral("48"));
    const auto* pMainCueMark = findPositionMark(elements, QString());
    ASSERT_NE(pMainCueMark, nullptr);
    EXPECT_EQ(pMainCueMark->attributes.value(QStringLiteral("Type")), QStringLiteral("0"));
    EXPECT_EQ(pMainCueMark->attributes.value(QStringLiteral("Start")), QStringLiteral("3.000"));
    EXPECT_EQ(pMainCueMark->attributes.value(QStringLiteral("Num")), QStringLiteral("-1"));
    EXPECT_FALSE(pMainCueMark->attributes.contains(QStringLiteral("Red")));
    const auto* pLoopMark = findPositionMark(elements, QStringLiteral("Saved loop"));
    ASSERT_NE(pLoopMark, nullptr);
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Type")), QStringLiteral("4"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Start")), QStringLiteral("2.000"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("End")), QStringLiteral("4.000"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Num")), QStringLiteral("6"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Red")), QStringLiteral("64"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Green")), QStringLiteral("80"));
    EXPECT_EQ(pLoopMark->attributes.value(QStringLiteral("Blue")), QStringLiteral("96"));
}

TEST(RekordboxCollectionXmlTest, OmitsTemporaryLoop) {
    const auto pTrack = makeTrack(QStringLiteral("/music/temporary-loop.mp3"), 11);
    pTrack->createAndAddCue(
            mixxx::CueType::Loop,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(44100),
            mixxx::audio::FramePos(88200));
    pTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            2,
            mixxx::audio::kInvalidFramePos,
            mixxx::audio::FramePos(44100));

    const QList<XmlElement> elements = parseElements(serialize({pTrack}));

    EXPECT_TRUE(positionMarks(elements).isEmpty());
}

TEST(RekordboxCollectionXmlTest, EscapesTextAndEncodesLocation) {
    const auto pTrack = makeTrack(QStringLiteral("/music/Set & Mix/Track #1.mp3"), 9);
    pTrack->setTitle(QStringLiteral("A & B <C> \"D\""));
    const auto pHotCue = pTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            3,
            mixxx::audio::FramePos(44100),
            mixxx::audio::kInvalidFramePos);
    ASSERT_NE(pHotCue, nullptr);
    pHotCue->setLabel(QStringLiteral("Cue & <label> \"quoted\""));

    const QString xml = serialize({pTrack});
    const QList<XmlElement> elements = parseElements(xml);

    const auto* pXmlTrack = findElement(
            elements, QStringLiteral("TRACK"), QStringLiteral("COLLECTION"));
    ASSERT_NE(pXmlTrack, nullptr);
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Name")),
            QStringLiteral("A & B <C> \"D\""));
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Location")),
            QStringLiteral("file://localhost/music/Set%20%26%20Mix/Track%20%231.mp3"));
    const auto* pHotCueMark = findPositionMark(
            elements, QStringLiteral("Cue & <label> \"quoted\""));
    ASSERT_NE(pHotCueMark, nullptr);
    EXPECT_TRUE(xml.contains(QStringLiteral("Name=\"A &amp; B &lt;C&gt; &quot;D&quot;\"")));
    EXPECT_TRUE(
            xml.contains(QStringLiteral("Name=\"Cue &amp; &lt;label&gt; &quot;quoted&quot;\"")));
}

TEST(RekordboxCollectionXmlTest, NormalizesWindowsLocationSeparators) {
    const auto pTrack = makeTrack(QStringLiteral("C:\\Music\\Track #1.mp3"), 10);

    const QList<XmlElement> elements = parseElements(serialize({pTrack}));

    const auto* pXmlTrack = findElement(
            elements, QStringLiteral("TRACK"), QStringLiteral("COLLECTION"));
    ASSERT_NE(pXmlTrack, nullptr);
    EXPECT_EQ(
            pXmlTrack->attributes.value(QStringLiteral("Location")),
            QStringLiteral("file://localhost/C:/Music/Track%20%231.mp3"));
}

TEST(RekordboxCollectionXmlTest, FormatsNumbersIndependentlyOfTheDefaultLocale) {
    const QLocale originalLocale;
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const auto pTrack = makeTrack(QStringLiteral("/music/locale.mp3"), 12);

    const QList<XmlElement> elements = parseElements(serialize({pTrack}));

    QLocale::setDefault(originalLocale);
    const auto* pXmlTrack = findElement(
            elements, QStringLiteral("TRACK"), QStringLiteral("COLLECTION"));
    ASSERT_NE(pXmlTrack, nullptr);
    EXPECT_EQ(pXmlTrack->attributes.value(QStringLiteral("AverageBpm")), QStringLiteral("123.45"));
}

TEST(RekordboxCollectionXmlTest, RejectsNonWritableOutputDevice) {
    QByteArray data;
    QBuffer buffer(&data);
    QString error;

    EXPECT_FALSE(mixxx::writeRekordboxCollectionXml(&buffer, {}, &error));
    EXPECT_EQ(error, QStringLiteral("The XML output device is not writable."));
}

TEST(RekordboxCollectionXmlTest, ReportsWriteError) {
    FailingWriteDevice device;
    ASSERT_TRUE(device.open(QIODevice::WriteOnly));
    QString error;

    EXPECT_FALSE(mixxx::writeRekordboxCollectionXml(&device, {}, &error));
    EXPECT_EQ(error, QStringLiteral("Failed to write rekordbox collection XML."));
}

TEST(RekordboxCollectionXmlTest, RejectsTrackWithoutLocation) {
    const auto pTrack = makeTrack(QString(), 13);
    QByteArray data;
    QBuffer buffer(&data);
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly));
    QString error;

    EXPECT_FALSE(mixxx::writeRekordboxCollectionXml(&buffer, {pTrack}, &error));
    EXPECT_EQ(error, QStringLiteral("Cannot write a track without a location."));
    EXPECT_TRUE(data.isEmpty());
}
