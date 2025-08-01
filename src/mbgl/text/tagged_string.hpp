#pragma once

#include <mbgl/text/bidi.hpp>
#include <mbgl/text/harfbuzz.hpp>
#include <mbgl/style/expression/formatted.hpp>
#include <mbgl/util/font_stack.hpp>

#include <optional>

namespace mbgl {

struct SectionOptions {
    SectionOptions(double scale_,
                   FontStack fontStack_,
                   GlyphIDType type_,
                   uint32_t startIndex_,
                   std::optional<Color> textColor_ = std::nullopt)
        : scale(scale_),
          fontStack(fontStack_),
          fontStackHash(FontStackHasher()(std::move(fontStack_))),
          type(type_),
          startIndex(startIndex_),
          textColor(std::move(textColor_)) {}

    SectionOptions(double scale_,
                   FontStackHash fontStackHash_,
                   GlyphIDType type_,
                   uint32_t startIndex_,
                   std::optional<Color> textColor_ = std::nullopt)
        : scale(scale_),
          fontStackHash(fontStackHash_),
          type(type_),
          startIndex(startIndex_),
          textColor(std::move(textColor_)) {}

    explicit SectionOptions(std::string imageID_)
        : scale(1.0),
          type(GlyphIDType::FontPBF),
          imageID(std::move(imageID_)) {}

    double scale;
    FontStack fontStack;
    FontStackHash fontStackHash;

    GlyphIDType type;

    std::shared_ptr<std::vector<HBShapeAdjust>> adjusts;

    int32_t startIndex;
    bool keySection = true;

    std::optional<std::string> imageID;

    std::optional<Color> textColor;
};

/**
 * A TaggedString is the shaping-code counterpart of the Formatted type
 * Whereas Formatted matches the logical structure of a 'format' expression,
 * a TaggedString represents the same data at a per-character level so that
 * character-rearranging operations (e.g. BiDi) preserve formatting.
 * Text is represented as:
 * - A string of characters
 * - A matching array of indices, pointing to:
 * - An array of SectionsOptions, representing the evaluated formatting
 *    options of the original sections.
 *
 * Once the guts of a TaggedString have been re-arranged by BiDi, you can
 * iterate over the contents in order, using getCharCodeAt and getSection
 * to get the formatting options for each character in turn.
 */
struct TaggedString {
    TaggedString() = default;

    TaggedString(std::u16string text_, SectionOptions options)
        : styledText(std::move(text_), std::vector<uint8_t>(text_.size(), 0)) {
        sections.push_back(std::move(options));
    }

    TaggedString(StyledText styledText_, std::vector<SectionOptions> sections_)
        : styledText(std::move(styledText_)),
          sections(std::move(sections_)) {}

    std::size_t length() const { return styledText.first.length(); }

    std::size_t sectionCount() const { return sections.size(); }

    bool empty() const { return styledText.first.empty(); }

    const SectionOptions& getSection(std::size_t index) const { return sections.at(styledText.second.at(index)); }

    char16_t getCharCodeAt(std::size_t index) const { return styledText.first[index]; }

    const std::u16string& rawText() const { return styledText.first; }

    const StyledText& getStyledText() const { return styledText; }

    void addTextSection(const std::u16string& text,
                        double scale,
                        const FontStack& fontStack,
                        GlyphIDType type,
                        bool keySection = true,
                        std::optional<Color> textColor_ = std::nullopt);

    void addTextSection(const std::u16string& text,
                        double scale,
                        const FontStack& fontStack,
                        GlyphIDType type,
                        std::shared_ptr<std::vector<HBShapeAdjust>>& adjusts,
                        bool keySection = true,
                        std::optional<Color> textColor_ = std::nullopt);

    void addImageSection(const std::string& imageID);

    const SectionOptions& sectionAt(std::size_t index) const { return sections.at(index); }

    const std::vector<SectionOptions>& getSections() const { return sections; }

    uint8_t getSectionIndex(std::size_t characterIndex) const { return styledText.second.at(characterIndex); }

    double getMaxScale() const;
    void trim();

    void verticalizePunctuation();
    bool allowsVerticalWritingMode();

    bool hbShaped() const { return textHBShaped; }

    void setHBShaped(bool shaped) { textHBShaped = shaped; }

    bool hasNeedHBShapeText() const { return hasNeedShapeTextVal; }

private:
    std::optional<char16_t> getNextImageSectionCharCode();

private:
    StyledText styledText;
    std::vector<SectionOptions> sections;
    std::optional<bool> supportsVerticalWritingMode;
    // Max number of images within a text is 6400 U+E000–U+F8FF
    // that covers Basic Multilingual Plane Unicode Private Use Area (PUA).
    char16_t imageSectionID = 0u;
    bool textHBShaped = false;
    bool hasNeedShapeTextVal = false;
};

} // namespace mbgl
