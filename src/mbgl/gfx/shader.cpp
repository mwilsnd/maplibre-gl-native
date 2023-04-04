#include <mbgl/gfx/shader.hpp>

namespace mbgl {
namespace gfx {
namespace rtti {
    
uint32_t nextTypeId() noexcept {
    static uint32_t id{0};
    return ++id;
}

} // namespace rtti
} // namespace gfx
} // namespace mbgl
