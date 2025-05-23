#include <mbgl/renderer/buckets/debug_bucket.hpp>
#include <mbgl/geometry/debug_font_data.hpp>
#include <mbgl/tile/tile_id.hpp>
#include <mbgl/util/string.hpp>

#include <cmath>
#include <string>

namespace mbgl {

DebugBucket::DebugBucket(const OverscaledTileID& id,
                         const bool renderable_,
                         const bool complete_,
                         std::optional<Timestamp> modified_,
                         std::optional<Timestamp> expires_,
                         MapDebugOptions debugMode_)
    : renderable(renderable_),
      complete(complete_),
      modified(std::move(modified_)),
      expires(std::move(expires_)),
      debugMode(debugMode_) {
    auto addText = [&](const std::string& text, double left, double baseline, double scale) {
        for (uint8_t c : text) {
            if (c < 32 || c >= 127) continue;

            std::optional<Point<int16_t>> prev;

            const glyph& glyph = simplex[c - 32];
            for (int32_t j = 0; j < glyph.length; j += 2) {
                if (glyph.data[j] == -1 && glyph.data[j + 1] == -1) {
                    prev = {};
                } else {
                    Point<int16_t> p{int16_t(::round(left + glyph.data[j] * scale)),
                                     int16_t(::round(baseline - glyph.data[j + 1] * scale))};

                    vertices.emplace_back(FillBucket::layoutVertex(p));

                    if (prev) {
                        indices.emplace_back(static_cast<uint16_t>(vertices.elements() - 2),
                                             static_cast<uint16_t>(vertices.elements() - 1));
                    }

                    prev = p;
                }
            }

            left += glyph.width * scale;
        }
    };

    double baseline = 200;
    if (debugMode & MapDebugOptions::ParseStatus) {
        const std::string text = util::toString(id) + " - " +
                                 (complete     ? "complete"
                                  : renderable ? "renderable"
                                               : "pending");
        addText(text, 50, baseline, 5);
        baseline += 200;
    }

    if (debugMode & MapDebugOptions::Timestamps && modified && expires) {
        const std::string modifiedText = "modified: " + util::iso8601(*modified);
        addText(modifiedText, 50, baseline, 5);

        const std::string expiresText = "expires: " + util::iso8601(*expires);
        addText(expiresText, 50, baseline + 200, 5);
    }

    segments.emplace_back(0, 0, vertices.elements(), indices.elements());
}

void DebugBucket::upload([[maybe_unused]] gfx::UploadPass& uploadPass) {}

} // namespace mbgl
