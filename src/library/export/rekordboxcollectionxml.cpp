#include "library/export/rekordboxcollectionxml.h"

#include <algorithm>
#include <optional>

#include <QColor>
#include <QFileInfo>
#include <QIODevice>
#include <QUrl>
#include <QXmlStreamWriter>

#include "track/cue.h"
#include "track/track.h"
#include "util/color/rgbcolor.h"

namespace {

QString rekordboxLocation(const QString& path) {
    QString encodedPath = path;
    encodedPath.replace('\\', '/');
    if (!encodedPath.startsWith('/')) {
        encodedPath.prepend('/');
    }
    return QStringLiteral("file://localhost") +
            QString::fromLatin1(QUrl::toPercentEncoding(encodedPath, "/:"));
}

bool isAbsoluteWindowsPath(const QString& path) {
    return path.size() >= 3 &&
            path.at(0).isLetter() &&
            path.at(1) == QChar(':') &&
            (path.at(2) == QChar('/') || path.at(2) == QChar('\\'));
}

QString trackLocationForExport(const TrackPointer& pTrack) {
    const auto fileInfo = pTrack->getFileInfo();
    if (fileInfo.hasLocation()) {
        return fileInfo.location();
    }

    const QString path = fileInfo.asQFileInfo().filePath();
    return isAbsoluteWindowsPath(path) ? path : QString();
}

bool isValidCuePosition(mixxx::audio::FramePos position) {
    return position.isValid() && position.value() >= 0;
}

bool cuePositionLess(const CuePointer& pCueA, const CuePointer& pCueB) {
    return pCueA->getPosition() < pCueB->getPosition();
}

void writePositionMark(
        QXmlStreamWriter* pXml,
        const QString& name,
        int type,
        double start,
        std::optional<double> end,
        int number,
        const QColor& color) {
    pXml->writeStartElement(QStringLiteral("POSITION_MARK"));
    pXml->writeAttribute(QStringLiteral("Name"), name);
    pXml->writeAttribute(QStringLiteral("Type"), QString::number(type));
    pXml->writeAttribute(QStringLiteral("Start"), QString::number(start, 'f', 3));
    if (end) {
        pXml->writeAttribute(QStringLiteral("End"), QString::number(*end, 'f', 3));
    }
    pXml->writeAttribute(QStringLiteral("Num"), QString::number(number));
    if (color.isValid()) {
        pXml->writeAttribute(QStringLiteral("Red"), QString::number(color.red()));
        pXml->writeAttribute(QStringLiteral("Green"), QString::number(color.green()));
        pXml->writeAttribute(QStringLiteral("Blue"), QString::number(color.blue()));
    }
    pXml->writeEndElement();
}

void writeTrack(QXmlStreamWriter* pXml, const TrackPointer& pTrack) {
    const double sampleRate = pTrack->getSampleRate().isValid()
            ? static_cast<double>(pTrack->getSampleRate().value())
            : 44100.0;
    const QString location = trackLocationForExport(pTrack);

    pXml->writeStartElement(QStringLiteral("TRACK"));
    pXml->writeAttribute(QStringLiteral("TrackID"), pTrack->getId().toString());
    pXml->writeAttribute(
            QStringLiteral("Name"),
            pTrack->getTitle().isEmpty() ? QFileInfo(location).fileName() : pTrack->getTitle());
    pXml->writeAttribute(QStringLiteral("Artist"), pTrack->getArtist());
    pXml->writeAttribute(QStringLiteral("Album"), pTrack->getAlbum());
    pXml->writeAttribute(QStringLiteral("Genre"), pTrack->getGenre());
    pXml->writeAttribute(
            QStringLiteral("Kind"),
            QFileInfo(location).suffix().toUpper() + QStringLiteral(" File"));
    pXml->writeAttribute(
            QStringLiteral("TotalTime"), QString::number(qRound(pTrack->getDuration())));
    pXml->writeAttribute(
            QStringLiteral("SampleRate"), QString::number(static_cast<int>(sampleRate)));
    pXml->writeAttribute(QStringLiteral("BitRate"), QString::number(pTrack->getBitrate()));
    if (pTrack->getBpm() > 0.0) {
        pXml->writeAttribute(
                QStringLiteral("AverageBpm"),
                QString::number(pTrack->getBpm(), 'f', 2));
    }
    if (!pTrack->getKeyText().isEmpty()) {
        pXml->writeAttribute(QStringLiteral("Tonality"), pTrack->getKeyText());
    }
    pXml->writeAttribute(QStringLiteral("Location"), rekordboxLocation(location));

    QList<CuePointer> hotCues;
    QList<CuePointer> mainCues;
    QList<CuePointer> loops;
    const QList<CuePointer> cues = pTrack->getCuePoints();
    for (const auto& pCue : cues) {
        if (!pCue) {
            continue;
        }
        const auto position = pCue->getPosition();
        switch (pCue->getType()) {
        case mixxx::CueType::HotCue:
            if (isValidCuePosition(position) &&
                    pCue->getHotCue() >= mixxx::kFirstHotCueIndex) {
                hotCues.append(pCue);
            }
            break;
        case mixxx::CueType::MainCue:
            if (isValidCuePosition(position)) {
                mainCues.append(pCue);
            }
            break;
        case mixxx::CueType::Loop:
            if (isValidCuePosition(position) && pCue->getEndPosition().isValid() &&
                    pCue->getEndPosition().value() > position.value() &&
                    pCue->getHotCue() >= mixxx::kFirstHotCueIndex) {
                loops.append(pCue);
            }
            break;
        default:
            break;
        }
    }

    std::sort(hotCues.begin(), hotCues.end(), [](const CuePointer& pCueA, const CuePointer& pCueB) {
        if (pCueA->getHotCue() != pCueB->getHotCue()) {
            return pCueA->getHotCue() < pCueB->getHotCue();
        }
        return cuePositionLess(pCueA, pCueB);
    });
    std::sort(mainCues.begin(), mainCues.end(), cuePositionLess);
    std::sort(loops.begin(), loops.end(), [](const CuePointer& pCueA, const CuePointer& pCueB) {
        if (pCueA->getHotCue() != pCueB->getHotCue()) {
            return pCueA->getHotCue() < pCueB->getHotCue();
        }
        return cuePositionLess(pCueA, pCueB);
    });

    for (const auto& pCue : hotCues) {
        const auto position = pCue->getPosition();
        writePositionMark(
                pXml,
                pCue->getLabel(),
                0,
                position.value() / sampleRate,
                std::nullopt,
                pCue->getHotCue(),
                mixxx::RgbColor::toQColor(pCue->getColor()));
    }

    for (const auto& pCue : mainCues) {
        const auto position = pCue->getPosition();
        writePositionMark(
                pXml,
                QString(),
                0,
                position.value() / sampleRate,
                std::nullopt,
                Cue::kNoHotCue,
                QColor());
    }

    for (const auto& pCue : loops) {
        const auto position = pCue->getPosition();
        const auto endPosition = pCue->getEndPosition();
        writePositionMark(
                pXml,
                pCue->getLabel(),
                4,
                position.value() / sampleRate,
                endPosition.value() / sampleRate,
                pCue->getHotCue(),
                mixxx::RgbColor::toQColor(pCue->getColor()));
    }

    pXml->writeEndElement();
}

} // namespace

namespace mixxx {

bool writeRekordboxCollectionXml(
        QIODevice* pDevice,
        const TrackPointerList& tracks,
        QString* pError) {
    if (!pDevice || !pDevice->isWritable()) {
        if (pError) {
            *pError = QStringLiteral("The XML output device is not writable.");
        }
        return false;
    }

    int trackCount = 0;
    for (const auto& pTrack : tracks) {
        if (!pTrack) {
            continue;
        }
        if (trackLocationForExport(pTrack).isEmpty()) {
            if (pError) {
                *pError = QStringLiteral("Cannot write a track without a location.");
            }
            return false;
        }
        ++trackCount;
    }

    QXmlStreamWriter xml(pDevice);
    xml.setAutoFormatting(true);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("DJ_PLAYLISTS"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("1.0.0"));
    xml.writeStartElement(QStringLiteral("PRODUCT"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("rekordbox"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("6.0.0"));
    xml.writeAttribute(QStringLiteral("Company"), QStringLiteral("AlphaTheta"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("COLLECTION"));
    xml.writeAttribute(QStringLiteral("Entries"), QString::number(trackCount));
    for (const auto& pTrack : tracks) {
        if (pTrack) {
            writeTrack(&xml, pTrack);
        }
    }
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("PLAYLISTS"));
    xml.writeStartElement(QStringLiteral("NODE"));
    xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("ROOT"));
    xml.writeAttribute(QStringLiteral("Count"), QStringLiteral("0"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    if (xml.hasError()) {
        if (pError) {
            *pError = QStringLiteral("Failed to write rekordbox collection XML.");
        }
        return false;
    }
    if (pError) {
        pError->clear();
    }
    return true;
}

} // namespace mixxx
