
#include <mbgl/test/util.hpp>

#include <mbgl/text/bidi.hpp>
#include <mbgl/text/tagged_string.hpp>
#include <mbgl/text/shaping.hpp>
#include <mbgl/util/constants.hpp>

using namespace mbgl;
using namespace util;

TEST(Shaping, ZWSP) {
    GlyphPosition glyphPosition;
    glyphPosition.metrics.width = 18;
    glyphPosition.metrics.height = 18;
    glyphPosition.metrics.left = 2;
    glyphPosition.metrics.top = -8;
    glyphPosition.metrics.advance = 21;

    Glyph glyph;
    glyph.id = u'中';
    glyph.metrics = glyphPosition.metrics;

    BiDi bidi;
    auto immutableGlyph = Immutable<Glyph>(makeMutable<Glyph>(std::move(glyph)));
    const std::vector<std::string> fontStack{{"font-stack"}};
    const SectionOptions sectionOptions(1.0f, fontStack, GlyphIDType::FontPBF, 0);
    const float layoutTextSize = 16.0f;
    const float layoutTextSizeAtBucketZoomLevel = 16.0f;
    GlyphMap glyphs = {{FontStackHasher()(fontStack), {{u'中', std::move(immutableGlyph)}}}};
    GlyphPositions glyphPositions = {{FontStackHasher()(fontStack), {{u'中', std::move(glyphPosition)}}}};
    ImagePositions imagePositions;

    const auto testGetShaping = [&](const TaggedString& string, unsigned maxWidthInChars) {
        return getShaping(string,
                          maxWidthInChars * ONE_EM,
                          ONE_EM, // lineHeight
                          style::SymbolAnchorType::Center,
                          style::TextJustifyType::Center,
                          0,              // spacing
                          {{0.0f, 0.0f}}, // translate
                          WritingModeType::Horizontal,
                          bidi,
                          glyphs,
                          glyphPositions,
                          imagePositions,
                          layoutTextSize,
                          layoutTextSizeAtBucketZoomLevel,
                          /*allowVerticalPlacement*/ false);
    };

    // 3 lines
    // 中中中中中中
    // 中中中中中中
    // 中中
    {
        TaggedString string(u"中中\u200b中中\u200b中中\u200b中中中中中中\u200b中中", sectionOptions);
        auto shaping = testGetShaping(string, 5);
        ASSERT_EQ(shaping.positionedLines.size(), 3);
        ASSERT_EQ(shaping.top, -36);
        ASSERT_EQ(shaping.bottom, 36);
        ASSERT_EQ(shaping.left, -63);
        ASSERT_EQ(shaping.right, 63);
        ASSERT_EQ(shaping.writingMode, WritingModeType::Horizontal);
    }

    // 2 lines
    // 中中
    // 中
    {
        TaggedString string(u"中中\u200b中", sectionOptions);
        auto shaping = testGetShaping(string, 1);
        ASSERT_EQ(shaping.positionedLines.size(), 2);
        ASSERT_EQ(shaping.top, -24);
        ASSERT_EQ(shaping.bottom, 24);
        ASSERT_EQ(shaping.left, -21);
        ASSERT_EQ(shaping.right, 21);
        ASSERT_EQ(shaping.writingMode, WritingModeType::Horizontal);
    }

    // 1 line
    // 中中
    {
        TaggedString string(u"中中\u200b", sectionOptions);
        auto shaping = testGetShaping(string, 2);
        ASSERT_EQ(shaping.positionedLines.size(), 1);
        ASSERT_EQ(shaping.top, -12);
        ASSERT_EQ(shaping.bottom, 12);
        ASSERT_EQ(shaping.left, -21);
        ASSERT_EQ(shaping.right, 21);
        ASSERT_EQ(shaping.writingMode, WritingModeType::Horizontal);
    }

    // 5 'new' lines.
    {
        TaggedString string(u"\u200b\u200b\u200b\u200b\u200b", sectionOptions);
        auto shaping = testGetShaping(string, 1);
        ASSERT_EQ(shaping.positionedLines.size(), 5);
        ASSERT_EQ(shaping.top, -60);
        ASSERT_EQ(shaping.bottom, 60);
        ASSERT_EQ(shaping.left, 0);
        ASSERT_EQ(shaping.right, 0);
        ASSERT_EQ(shaping.writingMode, WritingModeType::Horizontal);
    }
}

void setupShapedText(Shaping& shapedText, float textSize) {
    const auto glyph = PositionedGlyph(32,
                                       0.0f,
                                       0.0f,
                                       false,
                                       0,
                                       1.0,
                                       /*texRect*/ {},
                                       /*metrics*/ {},
                                       /*imageID*/ std::nullopt);
    shapedText.right = textSize;
    shapedText.bottom = textSize;
    shapedText.positionedLines.emplace_back();
    shapedText.positionedLines.back().positionedGlyphs.emplace_back(glyph);
}

void testApplyTextFit(const Rect<uint16_t>& rectangle,
                      const style::ImageContent& content,
                      const std::optional<style::TextFit> textFitWidth,
                      const std::optional<style::TextFit> textFitHeight,
                      const Shaping& shapedText,
                      float fontScale,
                      float expectedRight,
                      float expectedBottom) {
    ImagePosition image = {
        rectangle,
        style::Image::Impl("test",
                           PremultipliedImage({static_cast<uint32_t>(rectangle.w), static_cast<uint32_t>(rectangle.h)}),
                           1.0f,
                           false,
                           {},
                           {},
                           content,
                           textFitWidth,
                           textFitHeight)};
    auto shapedIcon = PositionedIcon::shapeIcon(image, {0, 0}, style::SymbolAnchorType::TopLeft);
    shapedIcon.fitIconToText(shapedText, style::IconTextFitType::Both, {0, 0, 0, 0}, {0, 0}, fontScale);
    const auto& icon = shapedIcon.applyTextFit();
    ASSERT_EQ(icon.top(), 0);
    ASSERT_EQ(icon.left(), 0);
    ASSERT_EQ(icon.right(), expectedRight);
    ASSERT_EQ(icon.bottom(), expectedBottom);
}

TEST(Shaping, applyTextFit) {
    float textSize = 4;
    float fontScale = 4;
    float expectedImageSize = textSize * fontScale;
    Shaping shapedText;
    setupShapedText(shapedText, textSize);

    {
        // applyTextFitHorizontal
        // This set of tests against applyTextFit starts with a 100x20 image with a 5,5,95,15 content box
        // that has been fitted to a 4*4 text with scale 4, resulting in a 16*16 image.
        const auto horizontalRectangle = Rect<uint16_t>(0, 0, 100, 20);
        const style::ImageContent horizontalContent = {5, 5, 95, 15};

        {
            // applyTextFit: not specified
            // No change should happen
            testApplyTextFit(horizontalRectangle,
                             horizontalContent,
                             std::nullopt,
                             std::nullopt,
                             shapedText,
                             fontScale,
                             expectedImageSize,
                             expectedImageSize);
        }

        {
            // applyTextFit: both stretchOrShrink
            // No change should happen
            testApplyTextFit(horizontalRectangle,
                             horizontalContent,
                             style::TextFit::stretchOrShrink,
                             style::TextFit::stretchOrShrink,
                             shapedText,
                             fontScale,
                             expectedImageSize,
                             expectedImageSize);
        }

        {
            // applyTextFit: stretchOnly, proportional
            // Since textFitWidth is stretchOnly, it should be returned to
            // the aspect ratio of the content rectangle (9:1) aspect ratio so 144x16.
            testApplyTextFit(horizontalRectangle,
                             horizontalContent,
                             style::TextFit::stretchOnly,
                             style::TextFit::proportional,
                             shapedText,
                             fontScale,
                             expectedImageSize * 9,
                             expectedImageSize);
        }
    }

    {
        // applyTextFitVertical
        // This set of tests against applyTextFit starts with a 20x100 image with a 5,5,15,95 content box
        // that has been fitted to a 4*4 text with scale 4, resulting in a 16*16 image.
        const auto verticalRectangle = Rect<uint16_t>(0, 0, 20, 100);
        const style::ImageContent verticalContent = {5, 5, 15, 95};

        {
            // applyTextFit: stretchOnly, proportional
            // Since textFitWidth is stretchOnly, it should be returned to
            // the aspect ratio of the content rectangle (9:1) aspect ratio so 144x16.
            testApplyTextFit(verticalRectangle,
                             verticalContent,
                             style::TextFit::proportional,
                             style::TextFit::stretchOnly,
                             shapedText,
                             fontScale,
                             expectedImageSize,
                             expectedImageSize * 9);
        }
    }
}
