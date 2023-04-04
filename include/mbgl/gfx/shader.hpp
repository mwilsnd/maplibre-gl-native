#pragma once

#include <string_view>
#include <type_traits>

namespace mbgl {
namespace gfx {

class Shader;

namespace rtti {

/// @brief Allocate a new ID for shader RTTI
/// @return Unique ID number
uint32_t nextTypeId() noexcept;

/// @brief Get the RTTI-id for the provided type
/// @tparam T Type
/// @return Type-specific ID number
template<typename T>
uint32_t idForType() noexcept {
    static uint32_t id = nextTypeId();
    return id;
}

} // namespace rtti

// Assert that a type is a valid shader for downcasting.
// A valid shader must:
// * Inherit from gfx::Shader and implement name() + getTypeId()
// * Be a final class
// Note: Use the helper gfx::ShaderBase to implement RTTI
template<typename T>
inline constexpr bool is_shader_v =
    std::is_base_of_v<gfx::Shader, T> &&
    std::is_same_v<
        std::remove_cv_t<decltype(T::Name)>,
        std::string_view> &&
    std::is_final_v<T>;

/// @brief A shader is used as the base class for all programs across any supported
/// backend API. Shaders are registered with a `gfx::ShaderRegistry` instance.
class Shader {
    public:
        virtual ~Shader() = default;

        /// @brief Get the name of this shader
        /// @return Shader name
        virtual const std::string_view name() const noexcept = 0;

        /// @brief Get the type id for this instance
        /// @return Type-specific ID
        virtual uint32_t getTypeId() const noexcept = 0;

        /// @brief Downcast to a type
        /// @tparam T Derived type
        /// @return Type or nullptr if type info was not a match
        template<typename T,
            typename std::enable_if_t<is_shader_v<T>, bool>* = nullptr>
        T* to() noexcept {
            if (getTypeId() != rtti::idForType<T>()) {
                return nullptr;
            }
            return static_cast<T*>(this);
        }
};

/// @brief CRTP base for enabling shader RTTI
/// @tparam T Derived type name
template<class T>
class ShaderBase : public gfx::Shader {
    public:
#ifndef NDEBUG
        ShaderBase() {
            assert(std::is_final_v<T>);
        }
#endif

        uint32_t getTypeId() const noexcept final { return typeId; };
        const std::string_view name() const noexcept final {
            return T::Name;
        }

    private:
        const uint32_t typeId = rtti::idForType<T>();
};

} // namespace gfx
} // namespace mbgl
