#include <algorithm>
#include <mbgl/layout/symbol_layout.hpp>
#include <mbgl/layout/merge_lines.hpp>
#include <mbgl/layout/clip_lines.hpp>
#include <mbgl/math/angles.hpp>
#include <mbgl/renderer/bucket_parameters.hpp>
#include <mbgl/renderer/layers/render_symbol_layer.hpp>
#include <mbgl/text/get_anchors.hpp>
#include <mbgl/text/shaping.hpp>
#include <mbgl/text/glyph_manager.hpp>
#include <mbgl/tile/geometry_tile_data.hpp>
#include <mbgl/tile/tile.hpp>
#include <mbgl/util/utf.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/string.hpp>
#include <mbgl/util/i18n.hpp>
#include <mbgl/util/platform.hpp>
#include <mbgl/util/containers.hpp>

#include <mapbox/polylabel.hpp>

#include <numbers>

using namespace std::numbers;

namespace mbgl {

using namespace style;

template <class Property>
static bool has(const style::SymbolLayoutProperties::PossiblyEvaluated& layout) {
    return layout.get<Property>().match([](const typename Property::Type& t) { return !t.empty(); },
                                        [](const auto&) { return true; });
}

namespace {
expression::Value sectionOptionsToValue(const SectionOptions& options) {
    std::unordered_map<std::string, expression::Value> result;
    // TODO: Data driven properties that can be overridden on per section basis.
    // TextOpacity
    // TextHaloColor
    // TextHaloWidth
    // TextHaloBlur
    if (options.textColor) {
        result.emplace(expression::kFormattedSectionTextColor, *options.textColor);
    }
    return result;
}

inline const SymbolLayerProperties& toSymbolLayerProperties(const Immutable<LayerProperties>& layer) {
    return static_cast<const SymbolLayerProperties&>(*layer);
}

inline Immutable<style::SymbolLayoutProperties::PossiblyEvaluated> createLayout(
    const SymbolLayoutProperties::Unevaluated& unevaluated, float zoom) {
    auto layout = makeMutable<style::SymbolLayoutProperties::PossiblyEvaluated>(
        unevaluated.evaluate(PropertyEvaluationParameters(zoom)));

    if (layout->get<IconRotationAlignment>() == AlignmentType::Auto) {
        if (layout->get<SymbolPlacement>() != SymbolPlacementType::Point) {
            layout->get<IconRotationAlignment>() = AlignmentType::Map;
        } else {
            layout->get<IconRotationAlignment>() = AlignmentType::Viewport;
        }
    }

    if (layout->get<TextRotationAlignment>() == AlignmentType::Auto) {
        if (layout->get<SymbolPlacement>() != SymbolPlacementType::Point) {
            layout->get<TextRotationAlignment>() = AlignmentType::Map;
        } else {
            layout->get<TextRotationAlignment>() = AlignmentType::Viewport;
        }
    }

    // If unspecified `*-pitch-alignment` inherits `*-rotation-alignment`
    if (layout->get<TextPitchAlignment>() == AlignmentType::Auto) {
        layout->get<TextPitchAlignment>() = layout->get<TextRotationAlignment>();
    }
    if (layout->get<IconPitchAlignment>() == AlignmentType::Auto) {
        layout->get<IconPitchAlignment>() = layout->get<IconRotationAlignment>();
    }

    return layout;
}

GlyphIDType getCharGlyphIDType(char16_t ch,
                               const FontStack& stack,
                               std::shared_ptr<FontFaces> faces,
                               GlyphIDType lastCharType) {
    if (!faces) {
        return GlyphIDType::FontPBF;
    }

    if (util::i18n::isVariationSelector1(ch)) {
        return lastCharType;
    }

    for (const auto& name : stack) {
        for (auto& face : *faces) {
            if (face.name == name) {
                for (auto& range : face.ranges) {
                    if (ch >= range.first && ch <= range.second) return face.type;
                }
            }
        }
    }

    return GlyphIDType::FontPBF;
}

} // namespace

SymbolLayout::SymbolLayout(const BucketParameters& parameters,
                           const std::vector<Immutable<style::LayerProperties>>& layers,
                           std::unique_ptr<GeometryTileLayer> sourceLayer_,
                           const LayoutParameters& layoutParameters)
    : bucketLeaderID(layers.front()->baseImpl->id),
      sourceLayer(std::move(sourceLayer_)),
      overscaling(static_cast<float>(parameters.tileID.overscaleFactor())),
      zoom(parameters.tileID.overscaledZ),
      canonicalID(parameters.tileID.canonical),
      mode(parameters.mode),
      pixelRatio(parameters.pixelRatio),
      tileSize(static_cast<uint32_t>(util::tileSize_D * overscaling)),
      tilePixelRatio(static_cast<float>(util::EXTENT) / tileSize),
      layout(createLayout(toSymbolLayerProperties(layers.at(0)).layerImpl().layout, zoom)) {
    const SymbolLayer::Impl& leader = toSymbolLayerProperties(layers.at(0)).layerImpl();

    textSize = leader.layout.get<TextSize>();
    iconSize = leader.layout.get<IconSize>();
    textRadialOffset = leader.layout.get<TextRadialOffset>();
    textVariableAnchorOffset = leader.layout.get<TextVariableAnchorOffset>();

    const bool hasText = has<TextField>(*layout) && has<TextFont>(*layout);
    const bool hasIcon = has<IconImage>(*layout);

    if (!hasText && !hasIcon) {
        return;
    }

    const bool hasSymbolSortKey = !leader.layout.get<SymbolSortKey>().isUndefined();
    const auto symbolZOrder = layout->get<SymbolZOrder>();
    sortFeaturesByKey = symbolZOrder != SymbolZOrderType::ViewportY && hasSymbolSortKey;
    const bool zOrderByViewportY = symbolZOrder == SymbolZOrderType::ViewportY ||
                                   (symbolZOrder == SymbolZOrderType::Auto && !sortFeaturesByKey);
    sortFeaturesByY = zOrderByViewportY && (layout->get<TextAllowOverlap>() || layout->get<IconAllowOverlap>() ||
                                            layout->get<TextIgnorePlacement>() || layout->get<IconIgnorePlacement>());
    if (layout->get<SymbolPlacement>() == SymbolPlacementType::Point) {
        auto modes = layout->get<TextWritingMode>();
        // Remove duplicates and preserve order.
        std::set<style::TextWritingModeType> seen;
        auto end = std::remove_if(modes.begin(), modes.end(), [&seen, this](const auto& placementMode) {
            allowVerticalPlacement = allowVerticalPlacement || placementMode == style::TextWritingModeType::Vertical;
            return !seen.insert(placementMode).second;
        });
        modes.erase(end, modes.end());
        placementModes = std::move(modes);
    }

    for (const auto& layer : layers) {
        layerPaintProperties.emplace(layer->baseImpl->id, layer);
    }

    // Determine glyph dependencies
    const size_t featureCount = sourceLayer->featureCount();
    for (size_t i = 0; i < featureCount; ++i) {
        auto feature = sourceLayer->getFeature(i);
        if (!leader.filter(expression::EvaluationContext(this->zoom, feature.get())
                               .withCanonicalTileID(&parameters.tileID.canonical)))
            continue;

        SymbolFeature ft(std::move(feature));

        ft.index = i;

        if (hasText) {
            auto formatted = layout->evaluate<TextField>(zoom, ft, layoutParameters.availableImages, canonicalID);
            auto textTransform = layout->evaluate<TextTransform>(zoom, ft, canonicalID);
            FontStack baseFontStack = layout->evaluate<TextFont>(zoom, ft, canonicalID);

            ft.formattedText = TaggedString();
            std::map<std::size_t, std::size_t> sectionTable;

            for (std::size_t sectionIndex = 0; sectionIndex < formatted.sections.size(); sectionIndex++) {
                const auto& section = formatted.sections[sectionIndex];

                if (!section.image) {
                    try {
                        std::string u8string = section.text;
                        if (textTransform == TextTransformType::Uppercase) {
                            u8string = platform::uppercase(u8string);
                        } else if (textTransform == TextTransformType::Lowercase) {
                            u8string = platform::lowercase(u8string);
                        }

                        auto u16String = applyArabicShaping(util::convertUTF8ToUTF16(u8string));
                        const char16_t* u16Char = u16String.data();
                        std::u16string subString;
                        auto sectionScale = section.fontScale ? *section.fontScale : 1.0;
                        auto sectionFontStack = section.fontStack ? *section.fontStack : baseFontStack;

                        GlyphIDType subStringtype = getCharGlyphIDType(
                            *u16Char, sectionFontStack, layoutParameters.fontFaces, GlyphIDType::FontPBF);

                        while (*u16Char) {
                            const auto chType = getCharGlyphIDType(
                                *u16Char, sectionFontStack, layoutParameters.fontFaces, subStringtype);
                            if (chType != subStringtype) {
                                if (subString.length()) {
                                    ft.formattedText->addTextSection(subString,
                                                                     sectionScale,
                                                                     sectionFontStack,
                                                                     subStringtype,
                                                                     false,
                                                                     section.textColor);
                                    sectionTable[ft.formattedText->getSections().size() - 1] = sectionIndex;
                                    if (subStringtype != GlyphIDType::FontPBF) {
                                        layoutParameters.glyphDependencies
                                            .shapes[section.fontStack ? *section.fontStack : baseFontStack]
                                                   [subStringtype]
                                            .insert(subString);
                                    }
                                }

                                subString.clear();
                                subStringtype = chType;
                            }

                            subString += *u16Char;

                            ++u16Char;
                        }

                        if (subString.length()) {
                            ft.formattedText->addTextSection(subString,
                                                             section.fontScale ? *section.fontScale : 1.0,
                                                             section.fontStack ? *section.fontStack : baseFontStack,
                                                             subStringtype,
                                                             true,
                                                             section.textColor);
                            sectionTable[ft.formattedText->getSections().size() - 1] = sectionIndex;
                            if (subStringtype != GlyphIDType::FontPBF) {
                                layoutParameters.glyphDependencies
                                    .shapes[section.fontStack ? *section.fontStack : baseFontStack][subStringtype]
                                    .insert(subString);
                            }
                        }
                    } catch (...) {
                        mbgl::Log::Error(
                            mbgl::Event::ParseTile,
                            "Encountered section with invalid UTF-8 in tile, source: " + sourceLayer->getName() +
                                " z: " + std::to_string(canonicalID.z) + " x: " + std::to_string(canonicalID.x) +
                                " y: " + std::to_string(canonicalID.y));
                        continue; // skip section
                    }
                } else {
                    layoutParameters.imageDependencies.emplace(section.image->id(), ImageType::Icon);
                    ft.formattedText->addImageSection(section.image->id());
                }
            }

            const bool canVerticalizeText = layout->get<TextRotationAlignment>() == AlignmentType::Map &&
                                            layout->get<SymbolPlacement>() != SymbolPlacementType::Point &&
                                            ft.formattedText->allowsVerticalWritingMode();

            // Loop through all characters of this text and collect unique codepoints.
            for (std::size_t j = 0; j < ft.formattedText->length(); j++) {
                uint8_t sectionIndex = ft.formattedText->getSectionIndex(j);
                auto& section = ft.formattedText->getSections()[sectionIndex];
                if (section.imageID) continue;
                const auto& sectionFontStack = formatted.sections[sectionTable[sectionIndex]].fontStack;
                GlyphIDs& dependencies =
                    layoutParameters.glyphDependencies.glyphs[sectionFontStack ? *sectionFontStack : baseFontStack];
                if (section.type != FontPBF) {
                    dependencies.insert(GlyphID(0, section.type));
                    needFinalizeSymbolsVal = true;
                } else {
                    char16_t codePoint = ft.formattedText->getCharCodeAt(j);
                    dependencies.insert(codePoint);
                    if (canVerticalizeText ||
                        (allowVerticalPlacement && ft.formattedText->allowsVerticalWritingMode())) {
                        if (char16_t verticalChr = util::i18n::verticalizePunctuation(codePoint)) {
                            dependencies.insert(verticalChr);
                        }
                    }
                }
            }
        }

        if (hasIcon) {
            ft.icon = layout->evaluate<IconImage>(zoom, ft, layoutParameters.availableImages, canonicalID);
            layoutParameters.imageDependencies.emplace(ft.icon->id(), ImageType::Icon);
        }

        if (ft.formattedText || ft.icon) {
            if (sortFeaturesByKey) {
                ft.sortKey = layout->evaluate<SymbolSortKey>(zoom, ft, canonicalID);
                const auto lowerBound = std::lower_bound(features.begin(), features.end(), ft);
                features.insert(lowerBound, std::move(ft));
            } else {
                features.push_back(std::move(ft));
            }
        }
    }

    if (layout->get<SymbolPlacement>() == SymbolPlacementType::Line) {
        util::mergeLines(features);
    }
}

void SymbolLayout::finalizeSymbols(HBShapeResults& results) {
    for (auto& feature : features) {
        if (feature.geometry.empty()) {
            continue;
        }

        if (feature.formattedText && feature.formattedText->hasNeedHBShapeText() &&
            !feature.formattedText->hbShaped()) {
            auto shapedString = TaggedString();

            const auto& sections = feature.formattedText->getSections();
            const auto& styleText = feature.formattedText->getStyledText();

            std::u16string subString;
            auto sectionIndex = styleText.second[0];
            auto strLen = styleText.first.length();

            auto applySubString = [&]() {
                if (subString.length()) {
                    auto& section = sections[sectionIndex];
                    if (GlyphIDType::FontPBF == section.type) {
                        shapedString.addTextSection(subString,
                                                    section.scale,
                                                    section.fontStack,
                                                    section.type,
                                                    section.keySection,
                                                    section.textColor);
                    } else {
                        auto& fontstackResults = results[section.fontStack];
                        auto& typeResults = fontstackResults[section.type];
                        auto& result = typeResults[subString];

                        shapedString.addTextSection(result.str,
                                                    section.scale,
                                                    section.fontStack,
                                                    section.type,
                                                    result.adjusts,
                                                    section.keySection,
                                                    section.textColor);
                    }
                }
            };

            for (size_t charIndex = 0; charIndex < strLen; ++charIndex) {
                auto& ch = styleText.first[charIndex];
                auto& sec = styleText.second[charIndex];

                if (sectionIndex != sec) {
                    applySubString();

                    subString.clear();
                    sectionIndex = sec;
                }

                subString += ch;
            }

            applySubString();

            shapedString.setHBShaped(true);
            feature.formattedText = shapedString;

        } // feature.formattedText
    } // for (auto & feature : features ..

    needFinalizeSymbolsVal = false;
} // SymbolLayout::finalizeSymbols

bool SymbolLayout::hasDependencies() const {
    return !features.empty();
}

bool SymbolLayout::hasSymbolInstances() const {
    return !symbolInstances.empty();
}

namespace {

// The radial offset is to the edge of the text box
// In the horizontal direction, the edge of the text box is where glyphs start
// But in the vertical direction, the glyphs appear to "start" at the baseline
// We don't actually load baseline data, but we assume an offset of ONE_EM - 17
// (see "yOffset" in shaping.js)
const float baselineOffset = 7.0f;

// We don't care which shaping we get because this is used for collision
// purposes and all the justifications have the same collision box.
const Shaping& getDefaultHorizontalShaping(const ShapedTextOrientations& shapedTextOrientations) {
    if (shapedTextOrientations.right) return shapedTextOrientations.right;
    if (shapedTextOrientations.center) return shapedTextOrientations.center;
    if (shapedTextOrientations.left) return shapedTextOrientations.left;
    return shapedTextOrientations.horizontal;
}

Shaping& shapingForTextJustifyType(ShapedTextOrientations& shapedTextOrientations, style::TextJustifyType type) {
    switch (type) {
        case style::TextJustifyType::Right:
            return shapedTextOrientations.right;
        case style::TextJustifyType::Left:
            return shapedTextOrientations.left;
        case style::TextJustifyType::Center:
            return shapedTextOrientations.center;
        default:
            assert(false);
            return shapedTextOrientations.horizontal;
    }
}

std::array<float, 2> evaluateRadialOffset(style::SymbolAnchorType anchor, float radialOffset) {
    std::array<float, 2> result{{0.0f, 0.0f}};
    radialOffset = std::max(radialOffset, 0.0f); // Ignore negative offset.
    // solve for r where r^2 + r^2 = radialOffset^2
    const float hypotenuse = radialOffset / std::numbers::sqrt2_v<float>;

    switch (anchor) {
        case SymbolAnchorType::TopRight:
        case SymbolAnchorType::TopLeft:
            result[1] = hypotenuse - baselineOffset;
            break;
        case SymbolAnchorType::BottomRight:
        case SymbolAnchorType::BottomLeft:
            result[1] = -hypotenuse + baselineOffset;
            break;
        case SymbolAnchorType::Bottom:
            result[1] = -radialOffset + baselineOffset;
            break;
        case SymbolAnchorType::Top:
            result[1] = radialOffset - baselineOffset;
            break;
        default:
            break;
    }

    switch (anchor) {
        case SymbolAnchorType::TopRight:
        case SymbolAnchorType::BottomRight:
            result[0] = -hypotenuse;
            break;
        case SymbolAnchorType::TopLeft:
        case SymbolAnchorType::BottomLeft:
            result[0] = hypotenuse;
            break;
        case SymbolAnchorType::Left:
            result[0] = radialOffset;
            break;
        case SymbolAnchorType::Right:
            result[0] = -radialOffset;
            break;
        default:
            break;
    }

    return result;
}

} // namespace

// static
std::array<float, 2> SymbolLayout::evaluateVariableOffset(style::SymbolAnchorType anchor, std::array<float, 2> offset) {
    if (offset[1] == INVALID_OFFSET_VALUE) {
        return evaluateRadialOffset(anchor, offset[0]);
    }
    std::array<float, 2> result{{0.0f, 0.0f}};
    offset[0] = std::abs(offset[0]);
    offset[1] = std::abs(offset[1]);

    switch (anchor) {
        case SymbolAnchorType::TopRight:
        case SymbolAnchorType::TopLeft:
        case SymbolAnchorType::Top:
            result[1] = offset[1] - baselineOffset;
            break;
        case SymbolAnchorType::BottomRight:
        case SymbolAnchorType::BottomLeft:
        case SymbolAnchorType::Bottom:
            result[1] = -offset[1] + baselineOffset;
            break;
        case SymbolAnchorType::Center:
        case SymbolAnchorType::Left:
        case SymbolAnchorType::Right:
            break;
    }

    switch (anchor) {
        case SymbolAnchorType::TopRight:
        case SymbolAnchorType::BottomRight:
        case SymbolAnchorType::Right:
            result[0] = -offset[0];
            break;
        case SymbolAnchorType::TopLeft:
        case SymbolAnchorType::BottomLeft:
        case SymbolAnchorType::Left:
            result[0] = offset[0];
            break;
        case SymbolAnchorType::Center:
        case SymbolAnchorType::Top:
        case SymbolAnchorType::Bottom:
            break;
    }

    return result;
}

std::optional<VariableAnchorOffsetCollection> SymbolLayout::getTextVariableAnchorOffset(const SymbolFeature& feature) {
    std::optional<VariableAnchorOffsetCollection> result;

    // If style specifies text-variable-anchor-offset, just return it
    if (!textVariableAnchorOffset.isUndefined()) {
        auto variableAnchorOffset = layout->evaluate<TextVariableAnchorOffset>(zoom, feature, canonicalID);
        if (!variableAnchorOffset.empty()) {
            std::vector<AnchorOffsetPair> anchorOffsets;
            anchorOffsets.reserve(variableAnchorOffset.size());
            // Convert offsets from EM to PX, and apply baseline shift
            for (const auto& anchorOffset : variableAnchorOffset) {
                std::array<float, 2> variableTextOffset = {
                    {anchorOffset.offset[0] * util::ONE_EM, anchorOffset.offset[1] * util::ONE_EM}};
                switch (anchorOffset.anchorType) {
                    case SymbolAnchorType::TopRight:
                    case SymbolAnchorType::TopLeft:
                    case SymbolAnchorType::Top:
                        variableTextOffset[1] -= baselineOffset;
                        break;
                    case SymbolAnchorType::BottomRight:
                    case SymbolAnchorType::BottomLeft:
                    case SymbolAnchorType::Bottom:
                        variableTextOffset[1] += baselineOffset;
                        break;
                    case SymbolAnchorType::Center:
                    case SymbolAnchorType::Left:
                    case SymbolAnchorType::Right:
                        break;
                }

                anchorOffsets.push_back(AnchorOffsetPair{anchorOffset.anchorType, variableTextOffset});
            }

            result = VariableAnchorOffsetCollection(std::move(anchorOffsets));
        }
    } else {
        const std::vector<TextVariableAnchorType> variableTextAnchor = layout->evaluate<TextVariableAnchor>(
            zoom, feature, canonicalID);
        if (!variableTextAnchor.empty()) {
            std::array<float, 2> variableTextOffset;
            if (!textRadialOffset.isUndefined()) {
                variableTextOffset = {{layout->evaluate<TextRadialOffset>(zoom, feature, canonicalID) * util::ONE_EM,
                                       INVALID_OFFSET_VALUE}};
            } else {
                variableTextOffset = {{layout->evaluate<TextOffset>(zoom, feature, canonicalID)[0] * util::ONE_EM,
                                       layout->evaluate<TextOffset>(zoom, feature, canonicalID)[1] * util::ONE_EM}};
            }

            std::vector<AnchorOffsetPair> anchorOffsetPairs;
            anchorOffsetPairs.reserve(variableTextAnchor.size());
            for (auto anchor : variableTextAnchor) {
                auto offset = variableTextOffset;
                offset = SymbolLayout::evaluateVariableOffset(anchor, offset);
                anchorOffsetPairs.push_back(AnchorOffsetPair{anchor, offset});
            }

            result = VariableAnchorOffsetCollection(std::move(anchorOffsetPairs));
        }
    }

    return result;
}

void SymbolLayout::prepareSymbols(const GlyphMap& glyphMap,
                                  const GlyphPositions& glyphPositions,
                                  const ImageMap& imageMap,
                                  const ImagePositions& imagePositions) {
    const bool isPointPlacement = layout->get<SymbolPlacement>() == SymbolPlacementType::Point;
    const bool textAlongLine = layout->get<TextRotationAlignment>() == AlignmentType::Map && !isPointPlacement;

    for (auto it = features.begin(); it != features.end(); ++it) {
        auto& feature = *it;
        if (feature.geometry.empty()) continue;

        ShapedTextOrientations shapedTextOrientations;
        std::optional<PositionedIcon> shapedIcon;
        std::array<float, 2> textOffset{{0.0f, 0.0f}};
        const float layoutTextSize = layout->evaluate<TextSize>(zoom + 1, feature, canonicalID);
        const float layoutTextSizeAtBucketZoomLevel = layout->evaluate<TextSize>(zoom, feature, canonicalID);
        const float layoutIconSize = layout->evaluate<IconSize>(zoom + 1, feature, canonicalID);

        // if feature has text, shape the text
        if (feature.formattedText && layoutTextSize > 0.0f) {
            const float lineHeight = layout->get<TextLineHeight>() * util::ONE_EM;
            const float spacing = util::i18n::allowsLetterSpacing(feature.formattedText->rawText())
                                      ? layout->evaluate<TextLetterSpacing>(zoom, feature, canonicalID) * util::ONE_EM
                                      : 0.0f;

            auto applyShaping = [&](const TaggedString& formattedText,
                                    WritingModeType writingMode,
                                    SymbolAnchorType textAnchor,
                                    TextJustifyType textJustify) {
                Shaping result = getShaping(
                    /* string */ formattedText,
                    /* maxWidth: ems */
                    isPointPlacement ? layout->evaluate<TextMaxWidth>(zoom, feature, canonicalID) * util::ONE_EM : 0.0f,
                    /* ems */ lineHeight,
                    textAnchor,
                    textJustify,
                    /* ems */ spacing,
                    /* translate */ textOffset,
                    /* writingMode */ writingMode,
                    /* bidirectional algorithm object */ bidi,
                    glyphMap,
                    /* glyphs */ glyphPositions,
                    /* images */ imagePositions,
                    layoutTextSize,
                    layoutTextSizeAtBucketZoomLevel,
                    allowVerticalPlacement);

                return result;
            };

            const auto variableAnchorOffsets = getTextVariableAnchorOffset(feature);
            const SymbolAnchorType textAnchor = layout->evaluate<TextAnchor>(zoom, feature, canonicalID);
            if (!variableAnchorOffsets || variableAnchorOffsets->empty()) {
                // Layers with variable anchors use the `text-radial-offset`
                // property and the [x, y] offset vector is calculated at
                // placement time instead of layout time
                const float radialOffset = layout->evaluate<TextRadialOffset>(zoom, feature, canonicalID);
                if (radialOffset > 0.0f) {
                    // The style spec says don't use `text-offset` and
                    // `text-radial-offset` together but doesn't actually
                    // specify what happens if you use both. We go with the
                    // radial offset.
                    textOffset = evaluateRadialOffset(textAnchor, radialOffset * util::ONE_EM);
                } else {
                    textOffset = {{layout->evaluate<TextOffset>(zoom, feature, canonicalID)[0] * util::ONE_EM,
                                   layout->evaluate<TextOffset>(zoom, feature, canonicalID)[1] * util::ONE_EM}};
                }
            }
            TextJustifyType textJustify = textAlongLine ? TextJustifyType::Center
                                                        : layout->evaluate<TextJustify>(zoom, feature, canonicalID);

            const auto addVerticalShapingForPointLabelIfNeeded = [&] {
                if (allowVerticalPlacement && feature.formattedText->allowsVerticalWritingMode()) {
                    feature.formattedText->verticalizePunctuation();
                    // Vertical POI label placement is meant to be used for
                    // scripts that support vertical writing mode, thus, default
                    // style::TextJustifyType::Left justification is used. If
                    // Latin scripts would need to be supported, this should
                    // take into account other justifications.
                    shapedTextOrientations.vertical = applyShaping(
                        *feature.formattedText, WritingModeType::Vertical, textAnchor, style::TextJustifyType::Left);
                }
            };

            // If this layer uses text-variable-anchor, generate shapings for
            // all justification possibilities.
            if (!textAlongLine && variableAnchorOffsets && !variableAnchorOffsets->empty()) {
                std::vector<TextJustifyType> justifications;
                if (textJustify != TextJustifyType::Auto) {
                    justifications.push_back(textJustify);
                } else {
                    for (const auto& anchorOffset : *variableAnchorOffsets) {
                        justifications.push_back(getAnchorJustification(anchorOffset.anchorType));
                    }
                }
                for (TextJustifyType justification : justifications) {
                    Shaping& shapingForJustification = shapingForTextJustifyType(shapedTextOrientations, justification);
                    if (shapingForJustification) {
                        continue;
                    }
                    // If using text-variable-anchor for the layer, we use a
                    // center anchor for all shapings and apply the offsets for
                    // the anchor in the placement step.
                    Shaping shaping = applyShaping(
                        *feature.formattedText, WritingModeType::Horizontal, SymbolAnchorType::Center, justification);
                    if (shaping) {
                        shapingForJustification = std::move(shaping);
                        if (shapingForJustification.positionedLines.size() == 1u) {
                            shapedTextOrientations.singleLine = true;
                            break;
                        }
                    }
                }

                // Vertical point label shaping if allowVerticalPlacement is enabled.
                addVerticalShapingForPointLabelIfNeeded();
            } else {
                if (textJustify == TextJustifyType::Auto) {
                    textJustify = getAnchorJustification(textAnchor);
                }

                // Horizontal point or line label.
                Shaping shaping = applyShaping(
                    *feature.formattedText, WritingModeType::Horizontal, textAnchor, textJustify);
                if (shaping) {
                    shapedTextOrientations.horizontal = std::move(shaping);
                }

                // Vertical point label shaping if allowVerticalPlacement is enabled.
                addVerticalShapingForPointLabelIfNeeded();

                // Verticalized line label.
                if (textAlongLine && feature.formattedText->allowsVerticalWritingMode()) {
                    feature.formattedText->verticalizePunctuation();
                    shapedTextOrientations.vertical = applyShaping(
                        *feature.formattedText, WritingModeType::Vertical, textAnchor, textJustify);
                }
            }
        }

        // if feature has icon, get sprite atlas position
        SymbolContent iconType{SymbolContent::None};
        if (feature.icon) {
            auto image = imageMap.find(feature.icon->id());
            if (image != imageMap.end()) {
                iconType = SymbolContent::IconRGBA;
                shapedIcon = PositionedIcon::shapeIcon(imagePositions.at(feature.icon->id()),
                                                       layout->evaluate<IconOffset>(zoom, feature, canonicalID),
                                                       layout->evaluate<IconAnchor>(zoom, feature, canonicalID));
                if (image->second->sdf) {
                    iconType = SymbolContent::IconSDF;
                }
                if (image->second->pixelRatio != pixelRatio) {
                    iconsNeedLinear = true;
                } else if (layout->get<IconRotate>().constantOr(1) != 0) {
                    iconsNeedLinear = true;
                }
            }
        }

        // if either shapedText or icon position is present, add the feature
        const Shaping& defaultShaping = getDefaultHorizontalShaping(shapedTextOrientations);
        iconsInText = defaultShaping && defaultShaping.iconsInText;
        if (defaultShaping || shapedIcon) {
            addFeature(std::distance(features.begin(), it),
                       feature,
                       shapedTextOrientations,
                       std::move(shapedIcon),
                       imageMap,
                       textOffset,
                       layoutTextSize,
                       layoutIconSize,
                       iconType);
        }

        feature.geometry.clear();
    }

    compareText.clear();
}

void SymbolLayout::addFeature(const std::size_t layoutFeatureIndex,
                              const SymbolFeature& feature,
                              const ShapedTextOrientations& shapedTextOrientations,
                              std::optional<PositionedIcon> shapedIcon,
                              const ImageMap& imageMap,
                              std::array<float, 2> textOffset,
                              float layoutTextSize,
                              float layoutIconSize,
                              const SymbolContent iconType) {
    const float minScale = 0.5f;
    const float glyphSize = 24.0f;

    const std::array<float, 2> iconOffset = layout->evaluate<IconOffset>(zoom, feature, canonicalID);

    // To reduce the number of labels that jump around when zooming we need
    // to use a text-size value that is the same for all zoom levels.
    // This calculates text-size at a high zoom level so that all tiles can
    // use the same value when calculating anchor positions.
    const float textMaxSize = layout->evaluate<TextSize>(18, feature, canonicalID);

    const float fontScale = layoutTextSize / glyphSize;
    const float textBoxScale = tilePixelRatio * fontScale;
    const float textMaxBoxScale = tilePixelRatio * textMaxSize / glyphSize;
    const float iconBoxScale = tilePixelRatio * layoutIconSize;
    const float symbolSpacing = tilePixelRatio * layout->get<SymbolSpacing>();
    const float textPadding = layout->get<TextPadding>() * tilePixelRatio;
    const Padding iconPadding = layout->evaluate<IconPadding>(zoom, feature, canonicalID) * tilePixelRatio;
    const float textMaxAngle = util::deg2radf(layout->get<TextMaxAngle>());
    const float iconRotation = layout->evaluate<IconRotate>(zoom, feature, canonicalID);
    const float textRotation = layout->evaluate<TextRotate>(zoom, feature, canonicalID);
    const auto variableAnchorOffsets = getTextVariableAnchorOffset(feature);

    const SymbolPlacementType textPlacement = layout->get<TextRotationAlignment>() != AlignmentType::Map
                                                  ? SymbolPlacementType::Point
                                                  : layout->get<SymbolPlacement>();

    const float textRepeatDistance = symbolSpacing / 2;
    const auto evaluatedLayoutProperties = layout->evaluate(zoom, feature);
    IndexedSubfeature indexedFeature(feature.index, sourceLayer->getName(), bucketLeaderID, symbolInstances.size());

    const auto iconTextFit = evaluatedLayoutProperties.get<style::IconTextFit>();
    const bool hasIconTextFit = iconTextFit != IconTextFitType::None;
    // Adjust shaped icon size when icon-text-fit is used.
    std::optional<PositionedIcon> verticallyShapedIcon;
    if (shapedIcon && hasIconTextFit) {
        // Create vertically shaped icon for vertical writing mode if needed.
        if (allowVerticalPlacement && shapedTextOrientations.vertical) {
            verticallyShapedIcon = shapedIcon;
            verticallyShapedIcon->fitIconToText(
                shapedTextOrientations.vertical, iconTextFit, layout->get<IconTextFitPadding>(), iconOffset, fontScale);
        }
        const auto& shapedText = getDefaultHorizontalShaping(shapedTextOrientations);
        if (shapedText) {
            shapedIcon->fitIconToText(
                shapedText, iconTextFit, layout->get<IconTextFitPadding>(), iconOffset, fontScale);
        }
    }

    auto addSymbolInstance = [&](Anchor& anchor, std::shared_ptr<SymbolInstanceSharedData> sharedData) {
        assert(sharedData);
        const bool anchorInsideTile = anchor.point.x >= 0 && anchor.point.x < util::EXTENT && anchor.point.y >= 0 &&
                                      anchor.point.y < util::EXTENT;

        if (mode == MapMode::Tile || anchorInsideTile) {
            // For static/continuous rendering, only add symbols anchored within this tile:
            //  neighboring symbols will be added as part of the neighboring tiles.
            // In tiled rendering mode, add all symbols in the buffers so that we can:
            //  (1) render symbols that overlap into this tile
            //  (2) approximate collision detection effects from neighboring symbols
            symbolInstances.emplace_back(anchor,
                                         std::move(sharedData),
                                         shapedTextOrientations,
                                         shapedIcon,
                                         verticallyShapedIcon,
                                         textBoxScale,
                                         textPadding,
                                         textPlacement,
                                         textOffset,
                                         iconBoxScale,
                                         iconPadding,
                                         iconOffset,
                                         indexedFeature,
                                         layoutFeatureIndex,
                                         feature.index,
                                         feature.formattedText ? feature.formattedText->rawText() : std::u16string(),
                                         overscaling,
                                         iconRotation,
                                         textRotation,
                                         variableAnchorOffsets,
                                         allowVerticalPlacement,
                                         iconType);

            if (sortFeaturesByKey) {
                if (!sortKeyRanges.empty() && sortKeyRanges.back().sortKey == feature.sortKey) {
                    sortKeyRanges.back().end = symbolInstances.size();
                } else {
                    sortKeyRanges.push_back({feature.sortKey, symbolInstances.size() - 1, symbolInstances.size()});
                }
            }
        }
    };

    const auto createSymbolInstanceSharedData = [&](GeometryCoordinates line) {
        return std::make_shared<SymbolInstanceSharedData>(std::move(line),
                                                          shapedTextOrientations,
                                                          shapedIcon,
                                                          verticallyShapedIcon,
                                                          evaluatedLayoutProperties,
                                                          textPlacement,
                                                          textOffset,
                                                          imageMap,
                                                          iconRotation,
                                                          iconType,
                                                          hasIconTextFit,
                                                          allowVerticalPlacement);
    };

    const auto& type = feature.getType();

    if (layout->get<SymbolPlacement>() == SymbolPlacementType::Line) {
        auto clippedLines = util::clipLines(feature.geometry, 0, 0, util::EXTENT, util::EXTENT);
        for (auto& line : clippedLines) {
            Anchors anchors = getAnchors(
                line,
                symbolSpacing,
                textMaxAngle,
                (shapedTextOrientations.vertical ? shapedTextOrientations.vertical
                                                 : getDefaultHorizontalShaping(shapedTextOrientations))
                    .left,
                (shapedTextOrientations.vertical ? shapedTextOrientations.vertical
                                                 : getDefaultHorizontalShaping(shapedTextOrientations))
                    .right,
                (shapedIcon ? shapedIcon->left() : 0),
                (shapedIcon ? shapedIcon->right() : 0),
                glyphSize,
                textMaxBoxScale,
                overscaling);
            auto sharedData = createSymbolInstanceSharedData(std::move(line));
            for (auto& anchor : anchors) {
                if (!feature.formattedText ||
                    !anchorIsTooClose(feature.formattedText->rawText(), textRepeatDistance, anchor)) {
                    addSymbolInstance(anchor, sharedData);
                }
            }
        }
    } else if (layout->get<SymbolPlacement>() == SymbolPlacementType::LineCenter) {
        // No clipping, multiple lines per feature are allowed
        // "lines" with only one point are ignored as in clipLines
        for (const auto& line : feature.geometry) {
            if (line.size() > 1) {
                std::optional<Anchor> anchor = getCenterAnchor(
                    line,
                    textMaxAngle,
                    (shapedTextOrientations.vertical ? shapedTextOrientations.vertical
                                                     : getDefaultHorizontalShaping(shapedTextOrientations))
                        .left,
                    (shapedTextOrientations.vertical ? shapedTextOrientations.vertical
                                                     : getDefaultHorizontalShaping(shapedTextOrientations))
                        .right,
                    (shapedIcon ? shapedIcon->left() : 0),
                    (shapedIcon ? shapedIcon->right() : 0),
                    glyphSize,
                    textMaxBoxScale);
                if (anchor) {
                    addSymbolInstance(*anchor, createSymbolInstanceSharedData(line));
                }
            }
        }
    } else if (type == FeatureType::Polygon) {
        for (const auto& polygon : classifyRings(feature.geometry)) {
            Polygon<double> poly;
            for (const auto& ring : polygon) {
                LinearRing<double> r;
                for (const auto& p : ring) {
                    r.push_back(convertPoint<double>(p));
                }
                poly.push_back(r);
            }

            // 1 pixel worth of precision, in tile coordinates
            auto poi = mapbox::polylabel(poly, util::EXTENT / util::tileSize_D);
            Anchor anchor(static_cast<float>(poi.x), static_cast<float>(poi.y), 0.0f, static_cast<size_t>(minScale));
            addSymbolInstance(anchor, createSymbolInstanceSharedData(polygon[0]));
        }
    } else if (type == FeatureType::LineString) {
        for (const auto& line : feature.geometry) {
            // Skip invalid LineStrings.
            if (line.empty()) continue;

            Anchor anchor(
                static_cast<float>(line[0].x), static_cast<float>(line[0].y), 0.0f, static_cast<size_t>(minScale));
            addSymbolInstance(anchor, createSymbolInstanceSharedData(line));
        }
    } else if (type == FeatureType::Point) {
        for (const auto& points : feature.geometry) {
            for (const auto& point : points) {
                Anchor anchor(
                    static_cast<float>(point.x), static_cast<float>(point.y), 0.0f, static_cast<size_t>(minScale));
                addSymbolInstance(anchor, createSymbolInstanceSharedData({point}));
            }
        }
    }
}

bool SymbolLayout::anchorIsTooClose(const std::u16string& text, const float repeatDistance, const Anchor& anchor) {
    if (compareText.find(text) == compareText.end()) {
        compareText.emplace(text, Anchors());
    } else {
        const auto& otherAnchors = compareText.find(text)->second;
        for (const Anchor& otherAnchor : otherAnchors) {
            if (util::dist<float>(anchor.point, otherAnchor.point) < repeatDistance) {
                return true;
            }
        }
    }
    compareText[text].push_back(anchor);
    return false;
}

// Analog of `addToLineVertexArray` in JS. This version doesn't need to build up
// a line array like the JS version does, but it uses the same logic to
// calculate tile distances.
std::vector<float> SymbolLayout::calculateTileDistances(const GeometryCoordinates& line, const Anchor& anchor) {
    std::vector<float> tileDistances(line.size());
    if (anchor.segment) {
        std::size_t segment = *anchor.segment;
        assert(segment < line.size());
        auto sumForwardLength = (segment + 1 < line.size()) ? util::dist<float>(anchor.point, line[segment + 1]) : .0f;
        auto sumBackwardLength = util::dist<float>(anchor.point, line[segment]);
        for (std::size_t i = segment + 1; i < line.size(); ++i) {
            tileDistances[i] = sumForwardLength;
            if (i < line.size() - 1) {
                sumForwardLength += util::dist<float>(line[i + 1], line[i]);
            }
        }
        for (std::size_t i = segment;; --i) {
            tileDistances[i] = sumBackwardLength;
            if (i != 0u) {
                sumBackwardLength += util::dist<float>(line[i - 1], line[i]);
            } else {
                break; // Add break to avoid unsigned integer overflow when i==0
            }
        }
    }
    return tileDistances;
}

void SymbolLayout::createBucket(const ImagePositions&,
                                std::unique_ptr<FeatureIndex>&,
                                mbgl::unordered_map<std::string, LayerRenderData>& renderData,
                                const bool firstLoad,
                                const bool showCollisionBoxes,
                                const CanonicalTileID& canonical) {
    auto bucket = std::make_shared<SymbolBucket>(layout,
                                                 layerPaintProperties,
                                                 textSize,
                                                 iconSize,
                                                 zoom,
                                                 iconsNeedLinear,
                                                 sortFeaturesByY,
                                                 bucketLeaderID,
                                                 std::move(symbolInstances),
                                                 std::move(sortKeyRanges),
                                                 tilePixelRatio,
                                                 allowVerticalPlacement,
                                                 std::move(placementModes),
                                                 iconsInText);

    for (SymbolInstance& symbolInstance : bucket->symbolInstances) {
        if (!symbolInstance.check(SYM_GUARD_LOC)) {
            continue;
        }

        const bool hasText = symbolInstance.hasText();
        const bool hasIcon = symbolInstance.hasIcon();
        const bool singleLine = symbolInstance.getSingleLine();

        const auto& feature = features.at(symbolInstance.getLayoutFeatureIndex());

        // Insert final placement into collision tree and add glyphs/icons to buffers

        // Process icon first, so that text symbols would have reference to
        // iconIndex which is used when dynamic vertices for icon-text-fit image
        // have to be updated.
        if (hasIcon) {
            const Range<float> sizeData = bucket->iconSizeBinder->getVertexSizeData(feature);
            auto& iconBuffer = symbolInstance.hasSdfIcon() ? bucket->sdfIcon : bucket->icon;
            const auto placeIcon = [&](const SymbolQuads& iconQuads, auto& index, const WritingModeType writingMode) {
                iconBuffer.placedSymbols.emplace_back(symbolInstance.getAnchor().point,
                                                      symbolInstance.getAnchor().segment.value_or(0u),
                                                      sizeData.min,
                                                      sizeData.max,
                                                      symbolInstance.getIconOffset(),
                                                      writingMode,
                                                      symbolInstance.line(),
                                                      std::vector<float>());
                index = iconBuffer.placedSymbols.size() - 1;
                PlacedSymbol& iconSymbol = iconBuffer.placedSymbols.back();
                iconSymbol.angle = (allowVerticalPlacement && writingMode == WritingModeType::Vertical)
                                       ? pi_v<float> / 2
                                       : 0.0f;
                iconSymbol.vertexStartIndex = addSymbols(
                    iconBuffer, sizeData, iconQuads, symbolInstance.getAnchor(), iconSymbol, feature.sortKey);
            };

            placeIcon(*symbolInstance.iconQuads(), symbolInstance.refPlacedIconIndex(), WritingModeType::None);

            if (symbolInstance.verticalIconQuads()) {
                placeIcon(*symbolInstance.verticalIconQuads(),
                          symbolInstance.refPlacedVerticalIconIndex(),
                          WritingModeType::Vertical);
                symbolInstance.check(SYM_GUARD_LOC);
            }

            for (auto& pair : bucket->paintProperties) {
                pair.second.iconBinders.populateVertexVectors(
                    feature, iconBuffer.vertices().elements(), symbolInstance.getDataFeatureIndex(), {}, {}, canonical);
            }
        }

        if (hasText && feature.formattedText) {
            std::optional<std::size_t> lastAddedSection;
            if (singleLine) {
                std::optional<std::size_t> placedTextIndex;
                lastAddedSection = addSymbolGlyphQuads(*bucket,
                                                       symbolInstance,
                                                       feature,
                                                       symbolInstance.getWritingModes(),
                                                       placedTextIndex,
                                                       symbolInstance.rightJustifiedGlyphQuads(),
                                                       canonical,
                                                       lastAddedSection);
                symbolInstance.setPlacedRightTextIndex(placedTextIndex);
                symbolInstance.setPlacedCenterTextIndex(placedTextIndex);
                symbolInstance.setPlacedLeftTextIndex(placedTextIndex);
            } else {
                if (symbolInstance.getRightJustifiedGlyphQuadsSize()) {
                    lastAddedSection = addSymbolGlyphQuads(*bucket,
                                                           symbolInstance,
                                                           feature,
                                                           symbolInstance.getWritingModes(),
                                                           symbolInstance.refPlacedRightTextIndex(),
                                                           symbolInstance.rightJustifiedGlyphQuads(),
                                                           canonical,
                                                           lastAddedSection);
                }
                if (symbolInstance.getCenterJustifiedGlyphQuadsSize()) {
                    lastAddedSection = addSymbolGlyphQuads(*bucket,
                                                           symbolInstance,
                                                           feature,
                                                           symbolInstance.getWritingModes(),
                                                           symbolInstance.refPlacedCenterTextIndex(),
                                                           symbolInstance.centerJustifiedGlyphQuads(),
                                                           canonical,
                                                           lastAddedSection);
                }
                if (symbolInstance.getLeftJustifiedGlyphQuadsSize()) {
                    lastAddedSection = addSymbolGlyphQuads(*bucket,
                                                           symbolInstance,
                                                           feature,
                                                           symbolInstance.getWritingModes(),
                                                           symbolInstance.refPlacedLeftTextIndex(),
                                                           symbolInstance.leftJustifiedGlyphQuads(),
                                                           canonical,
                                                           lastAddedSection);
                }
            }
            if ((symbolInstance.getWritingModes() & WritingModeType::Vertical) &&
                symbolInstance.getVerticalGlyphQuadsSize()) {
                lastAddedSection = addSymbolGlyphQuads(*bucket,
                                                       symbolInstance,
                                                       feature,
                                                       WritingModeType::Vertical,
                                                       symbolInstance.refPlacedVerticalTextIndex(),
                                                       symbolInstance.verticalGlyphQuads(),
                                                       canonical,
                                                       lastAddedSection);
            }
            symbolInstance.check(SYM_GUARD_LOC);
            assert(lastAddedSection); // True, as hasText == true;
            updatePaintPropertiesForSection(*bucket, feature, *lastAddedSection, canonical);
        }

        symbolInstance.releaseSharedData();
    }

    if (showCollisionBoxes) {
        addToDebugBuffers(*bucket);
    }
    if (bucket->hasData()) {
        for (const auto& pair : layerPaintProperties) {
            if (!firstLoad) {
                bucket->justReloaded = true;
            }
            renderData.emplace(pair.first, LayerRenderData{bucket, pair.second});
        }
    }
}

void SymbolLayout::updatePaintPropertiesForSection(SymbolBucket& bucket,
                                                   const SymbolFeature& feature,
                                                   std::size_t sectionIndex,
                                                   const CanonicalTileID& canonical) {
    const auto& formattedSection = sectionOptionsToValue((*feature.formattedText).sectionAt(sectionIndex));
    for (auto& pair : bucket.paintProperties) {
        pair.second.textBinders.populateVertexVectors(
            feature, bucket.text.vertices().elements(), feature.index, {}, {}, canonical, formattedSection);
    }
}

std::size_t SymbolLayout::addSymbolGlyphQuads(SymbolBucket& bucket,
                                              SymbolInstance& symbolInstance,
                                              const SymbolFeature& feature,
                                              WritingModeType writingMode,
                                              std::optional<size_t>& placedIndex,
                                              const SymbolQuads& glyphQuads,
                                              const CanonicalTileID& canonical,
                                              std::optional<std::size_t> lastAddedSection) {
    const Range<float> sizeData = bucket.textSizeBinder->getVertexSizeData(feature);
    const bool hasFormatSectionOverrides = bucket.hasFormatSectionOverrides();
    const auto& placedIconIndex = writingMode == WritingModeType::Vertical ? symbolInstance.getPlacedVerticalIconIndex()
                                                                           : symbolInstance.getPlacedIconIndex();
    bucket.text.placedSymbols.emplace_back(symbolInstance.getAnchor().point,
                                           symbolInstance.getAnchor().segment.value_or(0u),
                                           sizeData.min,
                                           sizeData.max,
                                           symbolInstance.getTextOffset(),
                                           writingMode,
                                           symbolInstance.line(),
                                           calculateTileDistances(symbolInstance.line(), symbolInstance.getAnchor()),
                                           placedIconIndex);
    placedIndex = bucket.text.placedSymbols.size() - 1;
    PlacedSymbol& placedSymbol = bucket.text.placedSymbols.back();
    placedSymbol.angle = (allowVerticalPlacement && writingMode == WritingModeType::Vertical) ? pi_v<float> / 2 : 0.0f;

    bool firstSymbol = true;
    for (const auto& symbolQuad : glyphQuads) {
        if (hasFormatSectionOverrides) {
            if (lastAddedSection && *lastAddedSection != symbolQuad.sectionIndex) {
                updatePaintPropertiesForSection(bucket, feature, *lastAddedSection, canonical);
            }
            lastAddedSection = symbolQuad.sectionIndex;
        }
        const std::size_t index = addSymbol(
            bucket.text, sizeData, symbolQuad, symbolInstance.getAnchor(), placedSymbol, feature.sortKey);
        if (firstSymbol) {
            placedSymbol.vertexStartIndex = index;
            firstSymbol = false;
        }
    }

    return lastAddedSection ? *lastAddedSection : 0u;
}

size_t SymbolLayout::addSymbol(SymbolBucket::Buffer& buffer,
                               const Range<float> sizeData,
                               const SymbolQuad& symbol,
                               const Anchor& labelAnchor,
                               PlacedSymbol& placedSymbol,
                               float sortKey) {
    constexpr const uint16_t vertexLength = 4;

    const auto& tl = symbol.tl;
    const auto& tr = symbol.tr;
    const auto& bl = symbol.bl;
    const auto& br = symbol.br;
    const auto& tex = symbol.tex;
    const auto& pixelOffsetTL = symbol.pixelOffsetTL;
    const auto& pixelOffsetBR = symbol.pixelOffsetBR;
    const auto& minFontScale = symbol.minFontScale;

    if (buffer.segments.empty() ||
        buffer.segments.back().vertexLength + vertexLength > std::numeric_limits<uint16_t>::max() ||
        std::fabs(buffer.segments.back().sortKey - sortKey) > std::numeric_limits<float>::epsilon()) {
        buffer.segments.emplace_back(buffer.vertices().elements(), buffer.triangles.elements(), 0ul, 0ul, sortKey);
    }

    // We're generating triangle fans, so we always start with the first
    // coordinate in this polygon.
    auto& segment = buffer.segments.back();
    assert(segment.vertexLength <= std::numeric_limits<uint16_t>::max());
    auto index = static_cast<uint16_t>(segment.vertexLength);

    // coordinates (2 triangles)
    auto& vertices = buffer.vertices();
    vertices.emplace_back(SymbolBucket::layoutVertex(labelAnchor.point,
                                                     tl,
                                                     symbol.glyphOffset.y,
                                                     tex.x,
                                                     tex.y,
                                                     sizeData,
                                                     symbol.isSDF,
                                                     pixelOffsetTL,
                                                     minFontScale));
    vertices.emplace_back(SymbolBucket::layoutVertex(labelAnchor.point,
                                                     tr,
                                                     symbol.glyphOffset.y,
                                                     tex.x + tex.w,
                                                     tex.y,
                                                     sizeData,
                                                     symbol.isSDF,
                                                     {pixelOffsetBR.x, pixelOffsetTL.y},
                                                     minFontScale));
    vertices.emplace_back(SymbolBucket::layoutVertex(labelAnchor.point,
                                                     bl,
                                                     symbol.glyphOffset.y,
                                                     tex.x,
                                                     tex.y + tex.h,
                                                     sizeData,
                                                     symbol.isSDF,
                                                     {pixelOffsetTL.x, pixelOffsetBR.y},
                                                     minFontScale));
    vertices.emplace_back(SymbolBucket::layoutVertex(labelAnchor.point,
                                                     br,
                                                     symbol.glyphOffset.y,
                                                     tex.x + tex.w,
                                                     tex.y + tex.h,
                                                     sizeData,
                                                     symbol.isSDF,
                                                     pixelOffsetBR,
                                                     minFontScale));

    // Dynamic/Opacity vertices are initialized so that the vertex count always
    // agrees with the layout vertex buffer, but they will always be updated
    // before rendering happens
    auto dynamicVertex = SymbolBucket::dynamicLayoutVertex(labelAnchor.point, 0);
    buffer.dynamicVertices().emplace_back(dynamicVertex);
    buffer.dynamicVertices().emplace_back(dynamicVertex);
    buffer.dynamicVertices().emplace_back(dynamicVertex);
    buffer.dynamicVertices().emplace_back(dynamicVertex);

    auto opacityVertex = SymbolBucket::opacityVertex(true, 1.0);
    buffer.opacityVertices().emplace_back(opacityVertex);
    buffer.opacityVertices().emplace_back(opacityVertex);
    buffer.opacityVertices().emplace_back(opacityVertex);
    buffer.opacityVertices().emplace_back(opacityVertex);

    // add the two triangles, referencing the four coordinates we just inserted.
    buffer.triangles.emplace_back(index + 0, index + 1, index + 2);
    buffer.triangles.emplace_back(index + 1, index + 2, index + 3);

    segment.vertexLength += vertexLength;
    segment.indexLength += 6;

    placedSymbol.glyphOffsets.push_back(symbol.glyphOffset.x);

    return index;
}

size_t SymbolLayout::addSymbols(SymbolBucket::Buffer& buffer,
                                const Range<float> sizeData,
                                const SymbolQuads& symbols,
                                const Anchor& labelAnchor,
                                PlacedSymbol& placedSymbol,
                                float sortKey) {
    bool firstSymbol = true;
    size_t firstIndex = 0;
    for (auto& symbol : symbols) {
        const size_t index = addSymbol(buffer, sizeData, symbol, labelAnchor, placedSymbol, sortKey);
        if (firstSymbol) {
            firstIndex = index;
            firstSymbol = false;
        }
    }
    return firstIndex;
}

void SymbolLayout::addToDebugBuffers(SymbolBucket& bucket) {
    if (!hasSymbolInstances()) {
        return;
    }

    for (const SymbolInstance& symbolInstance : symbolInstances) {
        auto populateCollisionBox = [&](const auto& feature, bool isText) {
            SymbolBucket::CollisionBuffer& collisionBuffer =
                feature.alongLine
                    ? (isText
                           ? static_cast<SymbolBucket::CollisionBuffer&>(bucket.getOrCreateTextCollisionCircleBuffer())
                           : static_cast<SymbolBucket::CollisionBuffer&>(bucket.getOrCreateIconCollisionCircleBuffer()))
                    : (isText ? static_cast<SymbolBucket::CollisionBuffer&>(bucket.getOrCreateTextCollisionBox())
                              : static_cast<SymbolBucket::CollisionBuffer&>(bucket.getOrCreateIconCollisionBox()));

            for (const CollisionBox& box : feature.boxes) {
                auto& anchor = box.anchor;

                Point<float> tl{box.x1, box.y1};
                Point<float> tr{box.x2, box.y1};
                Point<float> bl{box.x1, box.y2};
                Point<float> br{box.x2, box.y2};

                static constexpr std::size_t vertexLength = 4;
                const std::size_t indexLength = feature.alongLine ? 6 : 8;

                if (collisionBuffer.segments.empty() || collisionBuffer.segments.back().vertexLength + vertexLength >
                                                            std::numeric_limits<uint16_t>::max()) {
                    collisionBuffer.segments.emplace_back(
                        collisionBuffer.vertices().elements(),
                        feature.alongLine ? (isText ? bucket.textCollisionCircle->triangles.elements()
                                                    : bucket.iconCollisionCircle->triangles.elements())
                                          : (isText ? bucket.textCollisionBox->lines.elements()
                                                    : bucket.iconCollisionBox->lines.elements()));
                }

                auto& segment = collisionBuffer.segments.back();
                auto index = static_cast<uint16_t>(segment.vertexLength);

                collisionBuffer.vertices().emplace_back(
                    SymbolBucket::collisionLayoutVertex(anchor, symbolInstance.getAnchor().point, tl));
                collisionBuffer.vertices().emplace_back(
                    SymbolBucket::collisionLayoutVertex(anchor, symbolInstance.getAnchor().point, tr));
                collisionBuffer.vertices().emplace_back(
                    SymbolBucket::collisionLayoutVertex(anchor, symbolInstance.getAnchor().point, br));
                collisionBuffer.vertices().emplace_back(
                    SymbolBucket::collisionLayoutVertex(anchor, symbolInstance.getAnchor().point, bl));

                // Dynamic vertices are initialized so that the vertex count
                // always agrees with the layout vertex buffer, but they will
                // always be updated before rendering happens
                auto dynamicVertex = SymbolBucket::collisionDynamicVertex(false, false, {});
                collisionBuffer.dynamicVertices().emplace_back(dynamicVertex);
                collisionBuffer.dynamicVertices().emplace_back(dynamicVertex);
                collisionBuffer.dynamicVertices().emplace_back(dynamicVertex);
                collisionBuffer.dynamicVertices().emplace_back(dynamicVertex);

                if (feature.alongLine) {
                    auto& collisionCircle = (isText ? bucket.textCollisionCircle : bucket.iconCollisionCircle);
                    collisionCircle->triangles.emplace_back(index, index + 1, index + 2);
                    collisionCircle->triangles.emplace_back(index, index + 2, index + 3);
                } else {
                    auto& collisionBox = (isText ? bucket.textCollisionBox : bucket.iconCollisionBox);
                    collisionBox->lines.emplace_back(index + 0, index + 1);
                    collisionBox->lines.emplace_back(index + 1, index + 2);
                    collisionBox->lines.emplace_back(index + 2, index + 3);
                    collisionBox->lines.emplace_back(index + 3, index + 0);
                }

                segment.vertexLength += vertexLength;
                segment.indexLength += indexLength;
            }
        };
        symbolInstance.check(SYM_GUARD_LOC);
        populateCollisionBox(symbolInstance.getTextCollisionFeature(), true /*isText*/);
        if (symbolInstance.getVerticalTextCollisionFeature()) {
            populateCollisionBox(*symbolInstance.getVerticalTextCollisionFeature(), true /*isText*/);
        }
        if (symbolInstance.getVerticalIconCollisionFeature()) {
            populateCollisionBox(*symbolInstance.getVerticalIconCollisionFeature(), false /*isText*/);
        }
        populateCollisionBox(symbolInstance.getIconCollisionFeature(), false /*isText*/);
        symbolInstance.check(SYM_GUARD_LOC);
    }
}

} // namespace mbgl
