#include "library/export/rekordboxcollectionxmlfile.h"

#include <QSaveFile>

#include "library/export/rekordboxcollectionxml.h"

namespace mixxx {

bool writeRekordboxCollectionXmlFile(
        const QString& fileName,
        const TrackPointerList& tracks,
        QString* pError) {
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (pError) {
            *pError = QStringLiteral("Could not open XML output file \"%1\": %2")
                              .arg(fileName, file.errorString());
        }
        return false;
    }

    if (!writeRekordboxCollectionXml(&file, tracks, pError)) {
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (pError) {
            *pError = QStringLiteral("Could not finish writing XML output file \"%1\": %2")
                              .arg(fileName, file.errorString());
        }
        return false;
    }

    if (pError) {
        pError->clear();
    }
    return true;
}

} // namespace mixxx
