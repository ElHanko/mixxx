#pragma once

#include <QString>

#include "track/track_decl.h"

class QIODevice;

namespace mixxx {

bool writeRekordboxCollectionXml(
        QIODevice* pDevice,
        const TrackPointerList& tracks,
        QString* pError = nullptr);

} // namespace mixxx
