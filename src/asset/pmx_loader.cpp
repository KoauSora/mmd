#include "pmx_loader.hpp"

#include "log.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace asset {
namespace {

struct PmxHeaderInfo {
    float version = 0.0f;
    bool utf8 = false;
    int additionalUvCount = 0;

    int vertexIndexSize = 0;
    int textureIndexSize = 0;
    int materialIndexSize = 0;
    int boneIndexSize = 0;
    int morphIndexSize = 0;
    int rigidBodyIndexSize = 0;
};

static std::string normalizePmxPathString(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static std::string sharedToonFileName(std::uint8_t index) {
    const int clamped = std::clamp(static_cast<int>(index), 0, 9);
    const int toonNumber = clamped + 1;
    return "toon" + (toonNumber < 10 ? std::string("0") : std::string()) + std::to_string(toonNumber) + ".bmp";
}

static void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

static std::string utf16LeBytesToUtf8(const std::vector<char>& bytes) {
    std::string out;
    out.reserve(bytes.size());

    if (bytes.size() % 2 != 0) {
        return {};
    }

    std::vector<std::uint16_t> codeUnits;
    codeUnits.reserve(bytes.size() / 2);

    for (std::size_t i = 0; i < bytes.size(); i += 2) {
        const auto lo = static_cast<std::uint8_t>(bytes[i]);
        const auto hi = static_cast<std::uint8_t>(bytes[i + 1]);
        codeUnits.push_back(static_cast<std::uint16_t>(lo | (hi << 8)));
    }

    for (std::size_t i = 0; i < codeUnits.size(); ++i) {
        std::uint32_t cp = codeUnits[i];

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 < codeUnits.size()) {
                const std::uint32_t low = codeUnits[i + 1];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                    ++i;
                }
            }
        }

        appendUtf8(out, cp);
    }

    return out;
}

class BinaryReader {
public:
    explicit BinaryReader(const std::string& path)
        : ifs_(path, std::ios::binary) {
        if (ifs_.good()) {
            const auto cur = ifs_.tellg();
            ifs_.seekg(0, std::ios::end);
            const auto end = ifs_.tellg();
            ifs_.seekg(cur, std::ios::beg);
            if (end >= 0) {
                fileSize_ = static_cast<std::size_t>(end);
            }
        }
    }

    bool good() const {
        return ifs_.good();
    }

    bool eof() const {
        return ifs_.eof();
    }

    std::size_t remaining() {
        const auto cur = ifs_.tellg();
        if (cur < 0) {
            return 0;
        }
        const std::size_t pos = static_cast<std::size_t>(cur);
        if (pos >= fileSize_) {
            return 0;
        }
        return fileSize_ - pos;
    }

    void readBytes(void* dst, std::size_t size) {
        ifs_.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
    }

    void skip(std::size_t size) {
        ifs_.seekg(static_cast<std::streamoff>(size), std::ios::cur);
    }

    std::uint8_t readU8() {
        std::uint8_t v = 0;
        readBytes(&v, sizeof(v));
        return v;
    }

    std::int8_t readI8() {
        std::int8_t v = 0;
        readBytes(&v, sizeof(v));
        return v;
    }

    std::uint16_t readU16() {
        std::array<std::uint8_t, 2> b {};
        readBytes(b.data(), b.size());
        return static_cast<std::uint16_t>(b[0] | (b[1] << 8));
    }

    std::int16_t readI16() {
        return static_cast<std::int16_t>(readU16());
    }

    std::uint32_t readU32() {
        std::array<std::uint8_t, 4> b {};
        readBytes(b.data(), b.size());
        return static_cast<std::uint32_t>(
            b[0] |
            (b[1] << 8) |
            (b[2] << 16) |
            (b[3] << 24)
        );
    }

    std::int32_t readI32() {
        return static_cast<std::int32_t>(readU32());
    }

    float readF32() {
        const std::uint32_t u = readU32();
        float f = 0.0f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }

    glm::vec2 readVec2() {
        return {readF32(), readF32()};
    }

    glm::vec3 readVec3() {
        return {readF32(), readF32(), readF32()};
    }

    glm::vec4 readVec4() {
        return {readF32(), readF32(), readF32(), readF32()};
    }

    std::string readText(bool utf8) {
        const std::int32_t byteCount = readI32();
        if (byteCount <= 0) {
            return {};
        }

        std::vector<char> bytes(static_cast<std::size_t>(byteCount));
        readBytes(bytes.data(), bytes.size());

        if (utf8) {
            return std::string(bytes.begin(), bytes.end());
        }

        return utf16LeBytesToUtf8(bytes);
    }

    int readIndex(int size, bool allowNegative) {
        switch (size) {
        case 1:
            if (allowNegative) {
                return static_cast<int>(readI8());
            }
            return static_cast<int>(readU8());
        case 2:
            if (allowNegative) {
                return static_cast<int>(readI16());
            }
            return static_cast<int>(readU16());
        case 4:
            return static_cast<int>(readI32());
        default:
            return -1;
        }
    }

private:
    std::ifstream ifs_;
    std::size_t fileSize_ = 0;
};

static bool readHeader(BinaryReader& br, PmxHeaderInfo& out) {
    char magic[4] {};
    br.readBytes(magic, 4);

    if (!(magic[0] == 'P' && magic[1] == 'M' && magic[2] == 'X' && magic[3] == ' ')) {
        core::log::error("Invalid PMX magic.");
        return false;
    }

    out.version = br.readF32();

    const std::uint8_t globalsCount = br.readU8();
    if (globalsCount < 8) {
        core::log::error("Invalid PMX globals count.");
        return false;
    }

    std::vector<std::uint8_t> globals(globalsCount, 0);
    br.readBytes(globals.data(), globals.size());

    out.utf8 = (globals[0] == 1);
    out.additionalUvCount = static_cast<int>(globals[1]);
    out.vertexIndexSize = static_cast<int>(globals[2]);
    out.textureIndexSize = static_cast<int>(globals[3]);
    out.materialIndexSize = static_cast<int>(globals[4]);
    out.boneIndexSize = static_cast<int>(globals[5]);
    out.morphIndexSize = static_cast<int>(globals[6]);
    out.rigidBodyIndexSize = static_cast<int>(globals[7]);

    return true;
}

static glm::vec4 normalizedWeights(const glm::vec4& w) {
    const float sum = w.x + w.y + w.z + w.w;
    if (sum <= std::numeric_limits<float>::epsilon()) {
        return {1.0f, 0.0f, 0.0f, 0.0f};
    }
    return w / sum;
}

} // namespace

bool PmxLoader::loadFromFile(const std::string& path, PmxAsset& outAsset) {
    outAsset = {};

    BinaryReader br(path);
    if (!br.good()) {
        core::log::error("Failed to open PMX file.");
        return false;
    }

    PmxHeaderInfo header;
    if (!readHeader(br, header)) {
        return false;
    }

    // model info texts: local name, universal name, local comment, universal comment
    [[maybe_unused]] const std::string localName = br.readText(header.utf8);
    [[maybe_unused]] const std::string universalName = br.readText(header.utf8);
    [[maybe_unused]] const std::string localComment = br.readText(header.utf8);
    [[maybe_unused]] const std::string universalComment = br.readText(header.utf8);

    // -------------------------------------------------------------------------
    // vertices
    // -------------------------------------------------------------------------
    const std::int32_t vertexCount = br.readI32();
    if (vertexCount < 0) {
        core::log::error("Invalid PMX vertex count.");
        return false;
    }

    outAsset.vertices.reserve(static_cast<std::size_t>(vertexCount));

    for (std::int32_t i = 0; i < vertexCount; ++i) {
        PmxVertex v;
        v.position = br.readVec3();
        v.normal = br.readVec3();
        v.uv = br.readVec2();

        for (int auv = 0; auv < header.additionalUvCount; ++auv) {
            br.readVec4();
        }

        const std::uint8_t deformType = br.readU8();
        switch (deformType) {
        case 0: { // BDEF1
            const int b0 = br.readIndex(header.boneIndexSize, true);
            v.boneIds = {std::max(b0, 0), 0, 0, 0};
            v.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
            break;
        }
        case 1: { // BDEF2
            const int b0 = br.readIndex(header.boneIndexSize, true);
            const int b1 = br.readIndex(header.boneIndexSize, true);
            const float w0 = br.readF32();

            v.boneIds = {std::max(b0, 0), std::max(b1, 0), 0, 0};
            v.boneWeights = normalizedWeights({w0, 1.0f - w0, 0.0f, 0.0f});
            break;
        }
        case 2: { // BDEF4
            const int b0 = br.readIndex(header.boneIndexSize, true);
            const int b1 = br.readIndex(header.boneIndexSize, true);
            const int b2 = br.readIndex(header.boneIndexSize, true);
            const int b3 = br.readIndex(header.boneIndexSize, true);

            const float w0 = br.readF32();
            const float w1 = br.readF32();
            const float w2 = br.readF32();
            const float w3 = br.readF32();

            v.boneIds = {
                std::max(b0, 0),
                std::max(b1, 0),
                std::max(b2, 0),
                std::max(b3, 0)
            };
            v.boneWeights = normalizedWeights({w0, w1, w2, w3});
            break;
        }
        case 3: { // SDEF, 先降级成 BDEF2
            const int b0 = br.readIndex(header.boneIndexSize, true);
            const int b1 = br.readIndex(header.boneIndexSize, true);
            const float w0 = br.readF32();

            br.readVec3(); // C
            br.readVec3(); // R0
            br.readVec3(); // R1

            v.boneIds = {std::max(b0, 0), std::max(b1, 0), 0, 0};
            v.boneWeights = normalizedWeights({w0, 1.0f - w0, 0.0f, 0.0f});
            break;
        }
        case 4: { // QDEF, 先按 BDEF4 处理
            const int b0 = br.readIndex(header.boneIndexSize, true);
            const int b1 = br.readIndex(header.boneIndexSize, true);
            const int b2 = br.readIndex(header.boneIndexSize, true);
            const int b3 = br.readIndex(header.boneIndexSize, true);

            const float w0 = br.readF32();
            const float w1 = br.readF32();
            const float w2 = br.readF32();
            const float w3 = br.readF32();

            v.boneIds = {
                std::max(b0, 0),
                std::max(b1, 0),
                std::max(b2, 0),
                std::max(b3, 0)
            };
            v.boneWeights = normalizedWeights({w0, w1, w2, w3});
            break;
        }
        default:
            core::log::error("Unsupported PMX deform type.");
            return false;
        }

        br.readF32(); // edge scale

        outAsset.vertices.push_back(v);
    }

    // -------------------------------------------------------------------------
    // surfaces / indices
    // -------------------------------------------------------------------------
    const std::int32_t surfaceIndexCount = br.readI32();
    if (surfaceIndexCount < 0 || (surfaceIndexCount % 3) != 0) {
        core::log::error("Invalid PMX surface index count.");
        return false;
    }

    outAsset.indices.reserve(static_cast<std::size_t>(surfaceIndexCount));

    for (std::int32_t i = 0; i < surfaceIndexCount; i += 3) {
        const int i0 = br.readIndex(header.vertexIndexSize, false);
        const int i1 = br.readIndex(header.vertexIndexSize, false);
        const int i2 = br.readIndex(header.vertexIndexSize, false);

        // Keep PMX index winding as-is. Face orientation should be controlled by source data.
        outAsset.indices.push_back(static_cast<std::uint32_t>(i0));
        outAsset.indices.push_back(static_cast<std::uint32_t>(i1));
        outAsset.indices.push_back(static_cast<std::uint32_t>(i2));
    }

    // -------------------------------------------------------------------------
    // textures
    // -------------------------------------------------------------------------
    const std::int32_t textureCount = br.readI32();
    if (textureCount < 0) {
        core::log::error("Invalid PMX texture count.");
        return false;
    }

    std::vector<std::string> textures;
    textures.reserve(static_cast<std::size_t>(textureCount));

    const std::filesystem::path pmxDir = std::filesystem::path(path).parent_path();

    for (std::int32_t i = 0; i < textureCount; ++i) {
        std::string texPath = normalizePmxPathString(br.readText(header.utf8));

        if (!texPath.empty()) {
            std::filesystem::path fullPath = pmxDir / texPath;
            texPath = fullPath.lexically_normal().string();
        }

        textures.push_back(texPath);
    }

    // -------------------------------------------------------------------------
    // materials
    // -------------------------------------------------------------------------
    const std::int32_t materialCount = br.readI32();
    if (materialCount < 0) {
        core::log::error("Invalid PMX material count.");
        return false;
    }

    outAsset.materials.reserve(static_cast<std::size_t>(materialCount));

    int runningIndexOffset = 0;

    for (std::int32_t i = 0; i < materialCount; ++i) {
        PmxMaterial mat;

        mat.name = br.readText(header.utf8);
        br.readText(header.utf8); // universal name

        br.readVec4(); // diffuse RGBA
        br.readVec3(); // specular RGB
        br.readF32();  // specular strength
        br.readVec3(); // ambient RGB

        const std::uint8_t flags = br.readU8();

        br.readVec4(); // edge color
        br.readF32();  // edge size

        const int textureIndex = br.readIndex(header.textureIndexSize, true);
        [[maybe_unused]] const int environmentIndex = br.readIndex(header.textureIndexSize, true);
        [[maybe_unused]] const std::uint8_t environmentBlendMode = br.readU8();

        const std::uint8_t toonReference = br.readU8();
        if (toonReference == 0) {
            const int toonTexIndex = br.readIndex(header.textureIndexSize, true);
            if (toonTexIndex >= 0 && toonTexIndex < static_cast<int>(textures.size())) {
                mat.toonTexturePath = textures[static_cast<std::size_t>(toonTexIndex)];
            }
        } else {
            const std::uint8_t sharedToonIndex = br.readU8();
            const std::filesystem::path sharedToonPath = pmxDir / sharedToonFileName(sharedToonIndex);
            mat.toonTexturePath = sharedToonPath.lexically_normal().string();
        }

        br.readText(header.utf8); // metadata

        const std::int32_t materialSurfaceCount = br.readI32();

        mat.indexOffset = runningIndexOffset;
        mat.indexCount = materialSurfaceCount;
        mat.twoSided = (flags & 0x01) != 0;

        if (textureIndex >= 0 && textureIndex < static_cast<int>(textures.size())) {
            mat.diffuseTexturePath = textures[static_cast<std::size_t>(textureIndex)];
        }

        outAsset.materials.push_back(mat);
        runningIndexOffset += materialSurfaceCount;
    }

    // -------------------------------------------------------------------------
    // bones
    // -------------------------------------------------------------------------
    const std::int32_t boneCount = br.readI32();
    if (boneCount < 0) {
        core::log::error("Invalid PMX bone count.");
        return false;
    }

    outAsset.bones.reserve(static_cast<std::size_t>(boneCount));

    for (std::int32_t i = 0; i < boneCount; ++i) {
        PmxBone bone;

        bone.name = br.readText(header.utf8);
        br.readText(header.utf8); // universal name

        bone.position = br.readVec3();
        bone.parentIndex = br.readIndex(header.boneIndexSize, true);
        [[maybe_unused]] const std::int32_t layer = br.readI32();
        const std::uint16_t flags = br.readU16();

        // tail
        if ((flags & 0x0001u) == 0) {
            br.readVec3();
        } else {
            br.readIndex(header.boneIndexSize, true);
        }

        // inherit rotation / translation
        if ((flags & 0x0100u) != 0 || (flags & 0x0200u) != 0) {
            bone.inheritRotation = (flags & 0x0100u) != 0;
            bone.inheritTranslation = (flags & 0x0200u) != 0;
            bone.inheritParentIndex = br.readIndex(header.boneIndexSize, true);
            bone.inheritRate = br.readF32();
        }

        // fixed axis
        if ((flags & 0x0400u) != 0) {
            br.readVec3();
        }

        // local coordinate
        if ((flags & 0x0800u) != 0) {
            br.readVec3();
            br.readVec3();
        }

        // external parent deform
        if ((flags & 0x2000u) != 0) {
            br.readI32();
        }

        // IK
        if ((flags & 0x0020u) != 0) {
            bone.hasIk = true;
            bone.ik = {};
            bone.ik.targetBoneIndex = br.readIndex(header.boneIndexSize, true); // target bone
            bone.ik.loopCount = br.readI32(); // loop count
            bone.ik.limitAngle = br.readF32(); // limit angle (radians in PMX spec)
            const std::int32_t linkCount = br.readI32();
            if (linkCount > 0) {
                bone.ik.links.reserve(static_cast<std::size_t>(linkCount));
            }
            for (std::int32_t link = 0; link < linkCount; ++link) {
                PmxIkLink lk;
                lk.boneIndex = br.readIndex(header.boneIndexSize, true);
                const std::uint8_t hasLimit = br.readU8();
                if (hasLimit != 0) {
                    lk.hasLimit = true;
                    lk.limitMin = br.readVec3();
                    lk.limitMax = br.readVec3();
                }
                bone.ik.links.push_back(lk);
            }
        }

        outAsset.bones.push_back(bone);
    }

    // -------------------------------------------------------------------------
    // morphs (expressions)
    // -------------------------------------------------------------------------
    const std::int32_t morphCount = br.readI32();
    if (morphCount < 0) {
        core::log::error("Invalid PMX morph count.");
        return false;
    }
    outAsset.morphs.clear();
    outAsset.morphs.reserve(static_cast<std::size_t>(morphCount));

    for (std::int32_t i = 0; i < morphCount; ++i) {
        PmxMorph morph;
        morph.name = br.readText(header.utf8);
        br.readText(header.utf8); // universal name
        morph.panel = static_cast<int>(br.readU8());
        morph.type = static_cast<int>(br.readU8());
        const std::int32_t offsetCount = br.readI32();
        if (offsetCount < 0) {
            core::log::error("Invalid PMX morph offset count.");
            return false;
        }

        if (morph.type == 1) { // vertex morph
            morph.vertexOffsets.reserve(static_cast<std::size_t>(offsetCount));
            for (std::int32_t j = 0; j < offsetCount; ++j) {
                PmxVertexMorphOffset off;
                off.vertexIndex = br.readIndex(header.vertexIndexSize, false);
                off.offset = br.readVec3();
                morph.vertexOffsets.push_back(off);
            }
        } else {
            // Skip unsupported morph types for now.
            for (std::int32_t j = 0; j < offsetCount; ++j) {
                switch (morph.type) {
                case 0: // group: morph index + weight
                    br.readIndex(header.morphIndexSize, true);
                    br.readF32();
                    break;
                case 2: // bone: bone index + translation + rotation
                    br.readIndex(header.boneIndexSize, true);
                    br.readVec3();
                    br.readVec4(); // quaternion xyzw
                    break;
                case 3: // UV
                case 4: // additional UV1
                case 5: // additional UV2
                case 6: // additional UV3
                case 7: // additional UV4
                    br.readIndex(header.vertexIndexSize, false);
                    br.readVec4();
                    break;
                case 8: // material (skip)
                    br.readIndex(header.materialIndexSize, true);
                    br.readU8(); // calc type
                    br.readVec4(); // diffuse
                    br.readVec3(); // specular
                    br.readF32();  // specular power
                    br.readVec3(); // ambient
                    br.readVec4(); // edge color
                    br.readF32();  // edge size
                    br.readVec4(); // texture factor
                    br.readVec4(); // sphere factor
                    br.readVec4(); // toon factor
                    break;
                default:
                    // Unknown morph type: best effort fail fast.
                    core::log::warn("Unsupported PMX morph type=" + std::to_string(morph.type) + " (skipping may be wrong).");
                    // Cannot reliably skip.
                    return false;
                }
            }
        }

        outAsset.morphs.push_back(std::move(morph));
    }

    // -------------------------------------------------------------------------
    // display frames (bones/morph UI groups) - skip for now
    // -------------------------------------------------------------------------
    const std::int32_t frameCount = br.readI32();
    if (frameCount < 0) {
        core::log::error("Invalid PMX display frame count.");
        return false;
    }
    for (std::int32_t i = 0; i < frameCount; ++i) {
        br.readText(header.utf8); // local name
        br.readText(header.utf8); // universal name
        br.readU8();              // special flag
        const std::int32_t elementCount = br.readI32();
        if (elementCount < 0) {
            core::log::error("Invalid PMX display frame element count.");
            return false;
        }
        for (std::int32_t e = 0; e < elementCount; ++e) {
            const std::uint8_t elementType = br.readU8(); // 0: bone, 1: morph
            if (elementType == 0) {
                br.readIndex(header.boneIndexSize, true);
            } else if (elementType == 1) {
                br.readIndex(header.morphIndexSize, true);
            } else {
                core::log::error("Invalid PMX display frame element type.");
                return false;
            }
        }
    }

    // -------------------------------------------------------------------------
    // rigid bodies
    // -------------------------------------------------------------------------
    const std::int32_t rigidBodyCount = br.readI32();
    if (rigidBodyCount < 0) {
        core::log::error("Invalid PMX rigid body count.");
        return false;
    }
    outAsset.rigidBodies.clear();
    outAsset.rigidBodies.reserve(static_cast<std::size_t>(rigidBodyCount));

    for (std::int32_t i = 0; i < rigidBodyCount; ++i) {
        PmxRigidBody rb;
        rb.name = br.readText(header.utf8);
        br.readText(header.utf8); // universal name

        rb.boneIndex = br.readIndex(header.boneIndexSize, true);
        rb.group = static_cast<int>(br.readU8());
        rb.collisionMask = br.readU16();

        rb.shapeType = static_cast<int>(br.readU8());
        rb.shapeSize = br.readVec3();
        rb.shapePosition = br.readVec3();
        rb.shapeRotation = br.readVec3();

        rb.mass = br.readF32();
        rb.linearDamping = br.readF32();
        rb.angularDamping = br.readF32();
        rb.restitution = br.readF32();
        rb.friction = br.readF32();

        rb.mode = static_cast<int>(br.readU8());

        outAsset.rigidBodies.push_back(std::move(rb));
    }

    // -------------------------------------------------------------------------
    // joints
    // -------------------------------------------------------------------------
    const std::int32_t jointCount = br.readI32();
    if (jointCount < 0) {
        core::log::error("Invalid PMX joint count.");
        return false;
    }
    outAsset.joints.clear();
    outAsset.joints.reserve(static_cast<std::size_t>(jointCount));

    for (std::int32_t i = 0; i < jointCount; ++i) {
        PmxJoint j;
        j.name = br.readText(header.utf8);
        br.readText(header.utf8); // universal name

        j.type = static_cast<int>(br.readU8());
        j.rigidBodyA = br.readIndex(header.rigidBodyIndexSize, true);
        j.rigidBodyB = br.readIndex(header.rigidBodyIndexSize, true);

        j.position = br.readVec3();
        j.rotation = br.readVec3();

        j.limitPosMin = br.readVec3();
        j.limitPosMax = br.readVec3();
        j.limitRotMin = br.readVec3();
        j.limitRotMax = br.readVec3();

        j.springPos = br.readVec3();
        j.springRot = br.readVec3();

        outAsset.joints.push_back(std::move(j));
    }

    // -------------------------------------------------------------------------
    // soft bodies (PMX 2.1). PMX 2.0 ends after joints.
    // -------------------------------------------------------------------------
    outAsset.softBodies.clear();
    if (br.remaining() >= sizeof(std::int32_t)) {
        const std::int32_t softBodyCount = br.readI32();
        if (softBodyCount < 0) {
            core::log::error("Invalid PMX soft body count.");
            return false;
        }
        outAsset.softBodies.reserve(static_cast<std::size_t>(softBodyCount));

        for (std::int32_t i = 0; i < softBodyCount; ++i) {
            PmxSoftBody sb;
            sb.name = br.readText(header.utf8);
            sb.universalName = br.readText(header.utf8);

            sb.shape = static_cast<int>(br.readU8());
            sb.materialIndex = br.readIndex(header.materialIndexSize, true);
            sb.group = static_cast<int>(br.readU8());
            sb.collisionMask = br.readU16();
            sb.flags = static_cast<int>(br.readU8());

            sb.bLinkDistance = br.readI32();
            sb.clusterCount = br.readI32();
            sb.totalMass = br.readF32();
            sb.collisionMargin = br.readF32();
            sb.aeroModel = br.readI32();

            sb.config.vcf = br.readF32();
            sb.config.dp = br.readF32();
            sb.config.dg = br.readF32();
            sb.config.lf = br.readF32();
            sb.config.pr = br.readF32();
            sb.config.vc = br.readF32();
            sb.config.df = br.readF32();
            sb.config.mt = br.readF32();
            sb.config.chr = br.readF32();
            sb.config.khr = br.readF32();
            sb.config.shr = br.readF32();
            sb.config.ahr = br.readF32();

            sb.cluster.srhrCl = br.readF32();
            sb.cluster.skhrCl = br.readF32();
            sb.cluster.sshrCl = br.readF32();
            sb.cluster.srSplitCl = br.readF32();
            sb.cluster.skSplitCl = br.readF32();
            sb.cluster.ssSplitCl = br.readF32();

            sb.iteration.vIt = br.readI32();
            sb.iteration.pIt = br.readI32();
            sb.iteration.dIt = br.readI32();
            sb.iteration.cIt = br.readI32();

            sb.material.lst = br.readF32();
            sb.material.ast = br.readF32();
            sb.material.vst = br.readF32();

            const std::int32_t anchorCount = br.readI32();
            if (anchorCount < 0) {
                core::log::error("Invalid PMX soft body anchor count.");
                return false;
            }
            if (anchorCount > 0) {
                sb.anchors.reserve(static_cast<std::size_t>(anchorCount));
            }
            for (std::int32_t a = 0; a < anchorCount; ++a) {
                PmxSoftBodyAnchor an;
                an.rigidBodyIndex = br.readIndex(header.rigidBodyIndexSize, true);
                an.vertexIndex = br.readIndex(header.vertexIndexSize, false);
                an.nearMode = static_cast<int>(br.readU8());
                sb.anchors.push_back(an);
            }

            const std::int32_t pinCount = br.readI32();
            if (pinCount < 0) {
                core::log::error("Invalid PMX soft body pin vertex count.");
                return false;
            }
            if (pinCount > 0) {
                sb.pinVertexIndices.reserve(static_cast<std::size_t>(pinCount));
            }
            for (std::int32_t p = 0; p < pinCount; ++p) {
                sb.pinVertexIndices.push_back(br.readIndex(header.vertexIndexSize, false));
            }

            outAsset.softBodies.push_back(std::move(sb));
        }
    }

    core::log::info("PMX loaded.");
    core::log::info(
        "PMX physics: rigidBodies=" + std::to_string(outAsset.rigidBodies.size()) +
        " joints=" + std::to_string(outAsset.joints.size()) +
        " softBodies=" + std::to_string(outAsset.softBodies.size())
    );
    return true;
}

} // namespace asset