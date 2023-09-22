#include <mbgl/util/utf.hpp>
#include <unicode/unistr.h>

namespace mbgl {
namespace util {

std::u16string convertUTF8ToUTF16(const std::string& str) {
    auto ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str));
    return std::u16string(ustr.getBuffer(), static_cast<size_t>(ustr.length()));
}

std::string convertUTF16ToUTF8(const std::u16string& str) {
    std::string ustr;
    icu::UnicodeString(true /* isTerminated */, str.c_str(), static_cast<int32_t>(str.length())).toUTF8String(ustr);
    return ustr;
}

} // namespace util
} // namespace mbgl
