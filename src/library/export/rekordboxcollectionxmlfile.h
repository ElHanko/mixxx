#pragma once

#include <QString>

#include "track/track_decl.h"

namespace mixxx {

bool writeRekordboxCollectionXmlFile(
        const QString& fileName,
        const TrackPointerList& tracks,
        QString* pError = nullptr);

} // namespace mixxx
