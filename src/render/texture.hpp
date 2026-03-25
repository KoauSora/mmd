#pragma once

#include <glad/glad.h>

#include <string>

namespace render {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool load2D(const std::string& path, bool flipVertically = true);
    bool createWhite();

    void bind(unsigned int slot = 0) const;
    void destroy();

    bool valid() const;
    bool hasTransparency() const;
    float transparentPixelRatio() const;

private:
    GLuint id_ = 0;
    bool hasTransparency_ = false;
    float transparentPixelRatio_ = 0.0f;
};

} // namespace render