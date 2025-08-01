#pragma once

#include <mbgl/gfx/texture2d.hpp>
#include <mbgl/util/image.hpp>
#include <mbgl/vulkan/renderer_backend.hpp>

#include <memory>

namespace mbgl {
namespace vulkan {

class Context;
class RenderPass;

struct ImageAllocation {
    const VmaAllocator& allocator;
    VmaAllocation allocation{};
    vk::Image image{};
    vk::UniqueImageView imageView{};

    ImageAllocation() = delete;
    ImageAllocation(ImageAllocation&) = delete;
    ImageAllocation& operator=(const ImageAllocation& other) = delete;
    ImageAllocation(ImageAllocation&& other) = default;

    ImageAllocation(const VmaAllocator& allocator_)
        : allocator(allocator_) {}

    ~ImageAllocation() { destroy(); }

    bool create(const VmaAllocationCreateInfo& allocInfo, const vk::ImageCreateInfo& imageInfo);
    void destroy();

    void setName(const std::string& name) const;
};

using UniqueImageAllocation = std::unique_ptr<ImageAllocation>;
using SharedImageAllocation = std::shared_ptr<ImageAllocation>;

enum class Texture2DUsage {
    ShaderInput,
    Attachment,
    Read,
};

class Texture2D : public gfx::Texture2D {
public:
    Texture2D(Context& context_);
    ~Texture2D() override;

    gfx::Texture2D& setSamplerConfiguration(const SamplerState&) noexcept override;
    gfx::Texture2D& setFormat(gfx::TexturePixelType, gfx::TextureChannelDataType) noexcept override;
    gfx::Texture2D& setSize(Size size_) noexcept override;
    gfx::Texture2D& setImage(std::shared_ptr<PremultipliedImage>) noexcept override;
    gfx::Texture2D& setUsage(Texture2DUsage) noexcept;

    gfx::TexturePixelType getFormat() const noexcept override { return pixelFormat; }
    Size getSize() const noexcept override { return size; }
    size_t getDataSize() const noexcept override;
    size_t getPixelStride() const noexcept override;
    size_t numChannels() const noexcept override;

    bool isDirty() const { return samplerStateDirty || textureDirty; }
    bool isModifiedAfter(const std::chrono::duration<double>& t) const { return t < lastModified; }

    void create() noexcept override;

    void upload() noexcept override;
    void upload(const void* pixelData, const Size& size_) noexcept override;
    void uploadSubRegion(const void* pixelData, const Size& size, uint16_t xOffset, uint16_t yOffset) noexcept override;
    void uploadSubRegion(const void* pixelData,
                         const Size& size,
                         uint16_t xOffset,
                         uint16_t yOffset,
                         const vk::UniqueCommandBuffer& buffer) noexcept;

    bool needsUpload() const noexcept override { return !!imageData; };

    vk::Format getVulkanFormat() const { return vulkanFormat(pixelFormat, channelType); }

    const vk::ImageLayout& getVulkanImageLayout() const { return imageLayout; }
    const vk::UniqueImageView& getVulkanImageView() const { return imageAllocation->imageView; }
    const vk::Image& getVulkanImage() const { return imageAllocation->image; }
    const vk::Sampler& getVulkanSampler();

    void copyImage(vk::Image image);
    std::shared_ptr<PremultipliedImage> readImage();

private:
    static vk::Format vulkanFormat(const gfx::TexturePixelType, const gfx::TextureChannelDataType);
    static vk::Filter vulkanFilter(const gfx::TextureFilterType);
    static vk::SamplerAddressMode vulkanAddressMode(const gfx::TextureWrapType);

    void createTexture();
    void createSampler();

    void destroyTexture();
    void destroySampler();

    void transitionToTransferLayout(const vk::UniqueCommandBuffer&);
    void transitionToShaderReadLayout(const vk::UniqueCommandBuffer&);
    void transitionToGeneralLayout(const vk::UniqueCommandBuffer&);

    uint32_t getMipLevels() const;
    void generateMips(const vk::UniqueCommandBuffer& buffer);

private:
    Context& context;

    Size size{0, 0};
    gfx::TexturePixelType pixelFormat{gfx::TexturePixelType::RGBA};
    gfx::TextureChannelDataType channelType{gfx::TextureChannelDataType::UnsignedByte};
    SamplerState samplerState{};

    std::shared_ptr<PremultipliedImage> imageData{nullptr};
    bool textureDirty{true};
    bool samplerStateDirty{true};
    std::chrono::duration<double> lastModified{0};

    SharedImageAllocation imageAllocation;
    vk::ImageLayout imageLayout{vk::ImageLayout::eUndefined};

    vk::Sampler sampler{};

    Texture2DUsage textureUsage{Texture2DUsage::ShaderInput};
};

} // namespace vulkan
} // namespace mbgl
