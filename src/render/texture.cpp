#include "texture.hpp"

#include "log.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace render {

Texture::~Texture() {
    destroy();
}

bool Texture::load2D(const std::string& path, bool flipVertically) {
    destroy();
    hasTransparency_ = false;
    transparentPixelRatio_ = 0.0f;

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!pixels) {
        core::log::error("Failed to load texture: " + path);
        return false;
    }

    GLenum format = GL_RGB;
    if (channels == 4) {
        format = GL_RGBA;
        // Generic transparency detection for any model/material.
        const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::size_t transparentPixels = 0;
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const unsigned char alpha = pixels[i * 4 + 3];
            if (alpha < 250) {
                hasTransparency_ = true;
                ++transparentPixels;
            }
        }
        if (pixelCount > 0) {
            transparentPixelRatio_ = static_cast<float>(transparentPixels) / static_cast<float>(pixelCount);
        }
    } else if (channels == 1) {
        format = GL_RED;
    }

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        static_cast<GLint>(format),
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        pixels
    );
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);
    return true;
}

bool Texture::createWhite() {
    destroy();
    hasTransparency_ = false;
    transparentPixelRatio_ = 0.0f;

    const unsigned char pixel[4] = {255, 255, 255, 255};

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        1,
        1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixel
    );

    return true;
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::destroy() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}

bool Texture::valid() const {
    return id_ != 0;
}

bool Texture::hasTransparency() const {
    return hasTransparency_;
}

float Texture::transparentPixelRatio() const {
    return transparentPixelRatio_;
}

} // namespace render