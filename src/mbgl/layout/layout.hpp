#pragma once

#include <mbgl/style/image_impl.hpp>
#include <mbgl/text/glyph.hpp>
#include <mbgl/tile/geometry_tile_data.hpp>
#include <mbgl/text/glyph_manager.hpp>
#include <mbgl/util/containers.hpp>
#include <memory>

namespace mbgl {

class Bucket;
class BucketParameters;
class RenderLayer;
class FeatureIndex;
class LayerRenderData;
class GlyphManager;

class Layout {
public:
    virtual ~Layout() = default;

    virtual void createBucket(const ImagePositions&,
                              std::unique_ptr<FeatureIndex>&,
                              mbgl::unordered_map<std::string, LayerRenderData>&,
                              bool,
                              bool,
                              const CanonicalTileID&) = 0;

    virtual void prepareSymbols(const GlyphMap&, const GlyphPositions&, const ImageMap&, const ImagePositions&) {}

    virtual void finalizeSymbols(HBShapeResults&) {}

    virtual bool needFinalizeSymbols() { return false; }

    virtual bool hasSymbolInstances() const { return true; }

    virtual bool hasDependencies() const = 0;
};

class LayoutParameters {
public:
    const BucketParameters& bucketParameters;
    std::shared_ptr<FontFaces> fontFaces;
    GlyphDependencies& glyphDependencies;
    ImageDependencies& imageDependencies;
    std::set<std::string>& availableImages;
};

} // namespace mbgl
