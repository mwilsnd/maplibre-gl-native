#include <mbgl/gl/uniform_buffer_gl.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/compression.hpp>
#include <mbgl/util/logging.hpp>

#include <cassert>

namespace mbgl {
namespace gl {

using namespace platform;

UniformBufferGL::UniformBufferGL(const void* data_, std::size_t size_)
    : UniformBuffer(size_),
      hash(util::crc32(data_, size_)) {
    MBGL_CHECK_ERROR(glGenBuffers(SwapSize, ids.data()));

    for (auto i = 0; i < SwapSize; i++) {
        MBGL_CHECK_ERROR(glBindBuffer(GL_UNIFORM_BUFFER, ids[i]));
        MBGL_CHECK_ERROR(glBufferData(GL_UNIFORM_BUFFER, size, i == 0 ? data_ : nullptr, GL_DYNAMIC_DRAW));
        // MBGL_CHECK_ERROR(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    }

    id = ids[bufPtr];
}

UniformBufferGL::UniformBufferGL(const UniformBufferGL& other)
    : UniformBuffer(other),
      hash(other.hash) {
    MBGL_CHECK_ERROR(glGenBuffers(SwapSize, ids.data()));

    MBGL_CHECK_ERROR(glCopyBufferSubData(other.id, id, 0, 0, size));
    MBGL_CHECK_ERROR(glBindBuffer(GL_COPY_READ_BUFFER, other.id));
    MBGL_CHECK_ERROR(glBindBuffer(GL_COPY_WRITE_BUFFER, id));
    MBGL_CHECK_ERROR(glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, size));

    MBGL_CHECK_ERROR(glBindBuffer(GL_COPY_READ_BUFFER, 0));
    MBGL_CHECK_ERROR(glBindBuffer(GL_COPY_WRITE_BUFFER, 0));

    bufPtr = other.bufPtr;
    id = ids[bufPtr];
}

UniformBufferGL::~UniformBufferGL() {
    if (id) {
        MBGL_CHECK_ERROR(glDeleteBuffers(SwapSize, ids.data()));
        id = 0;
    }
}

void UniformBufferGL::update(const void* data_, std::size_t size_) {
    assert(size == size_);
    if (size != size_) {
        Log::Error(
            Event::General,
            "Mismatched size given to UBO update, expected " + std::to_string(size) + ", got " + std::to_string(size_));
        return;
    }

    const uint32_t newHash = util::crc32(data_, size_);
    if (newHash != hash) {
        if (SwapSize > 1) nextBuf();
        hash = newHash;

        MBGL_CHECK_ERROR(glBindBuffer(GL_UNIFORM_BUFFER, id));
        auto ptr = MBGL_CHECK_ERROR(glMapBufferRange(
            GL_UNIFORM_BUFFER, 0, size_, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

        if (ptr) {
            std::memcpy(ptr, data_, size_);
            MBGL_CHECK_ERROR(glUnmapBuffer(GL_UNIFORM_BUFFER));
        }
    }
}

void UniformBufferGL::nextBuf() noexcept {
    bufPtr++;
    if (bufPtr == SwapSize) bufPtr = 0;
    id = ids[bufPtr];
}

} // namespace gl
} // namespace mbgl
