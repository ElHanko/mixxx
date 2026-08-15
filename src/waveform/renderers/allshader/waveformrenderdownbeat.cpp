#include "waveform/renderers/allshader/waveformrenderdownbeat.h"

#include <QDomNode>

#include "moc_waveformrenderdownbeat.cpp"
#include "rendergraph/geometry.h"
#include "rendergraph/material/unicolormaterial.h"
#include "rendergraph/vertexupdaters/vertexupdater.h"
#include "skin/legacy/skincontext.h"
#include "track/track.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

using namespace rendergraph;

namespace allshader {

WaveformRenderDownbeat::WaveformRenderDownbeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : ::WaveformRendererAbstract(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    initForRectangles<UniColorMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRenderDownbeat::setup(const QDomNode& node, const SkinContext& skinContext) {
    m_color = QColor(skinContext.selectString(node, QStringLiteral("DownbeatColor")));
    m_color = WSkinColor::getCorrectColor(m_color).toRgb();
}

void WaveformRenderDownbeat::draw(QPainter* painter, QPaintEvent* event) {
    Q_UNUSED(painter);
    Q_UNUSED(event);
    DEBUG_ASSERT(false);
}

void WaveformRenderDownbeat::preprocess() {
    if (!preprocessInner()) {
        geometry().allocate(0);
        markDirtyGeometry();
    }
}

bool WaveformRenderDownbeat::preprocessInner() {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();
    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    const auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                                : ::WaveformRendererAbstract::Play;
    const mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return false;
    }

#ifndef __SCENEGRAPH__
    const int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return false;
    }
    m_color.setAlphaF(alpha / 100.0f);
#endif

    if (!m_color.alpha()) {
        return true;
    }

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return false;
    }

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            m_waveformRenderer->getLastDisplayedPosition(positionType) * trackSamples);
    if (!startPosition.isValid() || !endPosition.isValid()) {
        return false;
    }

    constexpr int kVerticesPerLine = 6;
    int numDownbeats = 0;
    for (auto it = trackBeats->iteratorFrom(startPosition);
            it != trackBeats->cend() && *it <= endPosition;
            ++it) {
        if (trackBeats->isDownbeat(it)) {
            ++numDownbeats;
        }
    }

    geometry().allocate(numDownbeats * kVerticesPerLine);
    VertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::Point2D>()};
    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const float rendererBreadth = m_waveformRenderer->getBreadth();
    for (auto it = trackBeats->iteratorFrom(startPosition);
            it != trackBeats->cend() && *it <= endPosition;
            ++it) {
        if (!trackBeats->isDownbeat(it)) {
            continue;
        }
        double xBeatPoint = m_waveformRenderer->transformSamplePositionInRendererWorld(
                it->toEngineSamplePos(), positionType);
        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;
        const float x1 = static_cast<float>(xBeatPoint);
        vertexUpdater.addRectangle(
                {x1, 0.f},
                {x1 + 1.f, m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth});
    }
    markDirtyGeometry();
    DEBUG_ASSERT(numDownbeats * kVerticesPerLine == vertexUpdater.index());

    material().setUniform(1, m_color);
    markDirtyMaterial();
    return true;
}

} // namespace allshader
