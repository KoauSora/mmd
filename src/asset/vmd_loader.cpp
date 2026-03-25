#include "vmd_loader.hpp"

#include "log.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if __has_include(<iconv.h>)
#include <iconv.h>
#define ASSET_HAS_ICONV 1
#else
#define ASSET_HAS_ICONV 0
#endif

namespace asset {

namespace {

static std::string trimNullTerminated(const char* bytes, std::size_t n) {
    std::size_t len = 0;
    while (len < n && bytes[len] != '\0') {
        ++len;
    }
    return std::string(bytes, bytes + len);
}

static std::string decodeShiftJisToUtf8(const char* bytes, std::size_t n) {
    const std::string sjis = trimNullTerminated(bytes, n);
    if (sjis.empty()) {
        return {};
    }

#if ASSET_HAS_ICONV
    iconv_t cd = iconv_open("UTF-8", "SHIFT-JIS");
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        return sjis;
    }

    std::string out;
    out.resize(sjis.size() * 4 + 8);

    const char* inBuf = sjis.data();
    std::size_t inBytesLeft = sjis.size();
    char* outBuf = out.data();
    std::size_t outBytesLeft = out.size();

    while (inBytesLeft > 0) {
        const std::size_t res = iconv(cd, const_cast<char**>(&inBuf), &inBytesLeft, &outBuf, &outBytesLeft);
        if (res == static_cast<std::size_t>(-1)) {
            iconv_close(cd);
            return sjis;
        }
    }

    iconv_close(cd);
    out.resize(out.size() - outBytesLeft);
    return out;
#else
    (void)n;
    return sjis;
#endif
}

class BinaryReader {
public:
    explicit BinaryReader(const std::string& path)
        : ifs_(path, std::ios::binary) {
    }

    bool good() const {
        return ifs_.good();
    }

    template <typename T>
    bool read(T& out) {
        if (!ifs_.read(reinterpret_cast<char*>(&out), static_cast<std::streamsize>(sizeof(T)))) {
            return false;
        }
        return true;
    }

    bool readBytes(void* dst, std::size_t n) {
        if (!ifs_.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n))) {
            return false;
        }
        return true;
    }

private:
    std::ifstream ifs_;
};

static bool startsWith(const std::string& s, const char* prefix) {
    const std::size_t n = std::strlen(prefix);
    if (s.size() < n) {
        return false;
    }
    return std::memcmp(s.data(), prefix, n) == 0;
}

} // namespace

bool VmdLoader::loadFromFile(const std::string& path, VmdClip& outClip) {
    outClip = {};

    BinaryReader br(path);
    if (!br.good()) {
        core::log::error("Failed to open VMD: " + path);
        return false;
    }

    // Header: magic[30], modelName[20] (Shift-JIS)
    std::array<char, 30> magic {};
    std::array<char, 20> modelNameRaw {};
    if (!br.readBytes(magic.data(), magic.size()) || !br.readBytes(modelNameRaw.data(), modelNameRaw.size())) {
        core::log::error("VMD header read failed: " + path);
        return false;
    }

    const std::string magicStr = trimNullTerminated(magic.data(), magic.size());
    if (!startsWith(magicStr, "Vocaloid Motion Data")) {
        core::log::error("Not a VMD file (bad magic): " + path);
        return false;
    }

    const std::string modelName = decodeShiftJisToUtf8(modelNameRaw.data(), modelNameRaw.size());
    if (!modelName.empty()) {
        core::log::info("VMD model name: " + modelName);
    }

    std::uint32_t boneFrameCount = 0;
    if (!br.read(boneFrameCount)) {
        core::log::error("VMD read bone frame count failed: " + path);
        return false;
    }

    outClip.boneTracks.clear();
    outClip.trackLookup.clear();
    outClip.boneTracks.reserve(128);
    outClip.morphTracks.clear();
    outClip.morphLookup.clear();

    float maxTime = 0.0f;
    constexpr float kFps = 30.0f;

    for (std::uint32_t i = 0; i < boneFrameCount; ++i) {
        std::array<char, 15> boneNameRaw {};
        std::uint32_t frameNo = 0;
        float pos[3] = {0.0f, 0.0f, 0.0f};
        float rot[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // x,y,z,w in VMD
        std::array<std::uint8_t, 64> interp {};

        if (!br.readBytes(boneNameRaw.data(), boneNameRaw.size()) ||
            !br.read(frameNo) ||
            !br.readBytes(pos, sizeof(pos)) ||
            !br.readBytes(rot, sizeof(rot)) ||
            !br.readBytes(interp.data(), interp.size())) {
            core::log::error("VMD bone frame read failed at index " + std::to_string(i) + ": " + path);
            return false;
        }

        const std::string boneName = decodeShiftJisToUtf8(boneNameRaw.data(), boneNameRaw.size());
        if (boneName.empty()) {
            continue;
        }

        const float t = static_cast<float>(frameNo) / kFps;
        maxTime = std::max(maxTime, t);

        std::size_t trackIndex = 0;
        auto it = outClip.trackLookup.find(boneName);
        if (it == outClip.trackLookup.end()) {
            trackIndex = outClip.boneTracks.size();
            outClip.trackLookup[boneName] = trackIndex;
            VmdBoneTrack track;
            track.boneName = boneName;
            outClip.boneTracks.push_back(std::move(track));
        } else {
            trackIndex = it->second;
        }

        VmdBoneKeyframe kf;
        kf.time = t;
        kf.translation = glm::vec3(pos[0], pos[1], pos[2]);
        kf.rotation = glm::normalize(glm::quat(rot[3], rot[0], rot[1], rot[2]));

        outClip.boneTracks[trackIndex].keyframes.push_back(kf);
    }

    // Sort keyframes by time for each track
    for (auto& track : outClip.boneTracks) {
        std::sort(track.keyframes.begin(), track.keyframes.end(), [](const VmdBoneKeyframe& a, const VmdBoneKeyframe& b) {
            return a.time < b.time;
        });
    }

    // Morph (face/expression) keyframes
    std::uint32_t morphFrameCount = 0;
    if (!br.read(morphFrameCount)) {
        core::log::error("VMD read morph frame count failed: " + path);
        return false;
    }

    outClip.morphTracks.reserve(64);
    for (std::uint32_t i = 0; i < morphFrameCount; ++i) {
        std::array<char, 15> morphNameRaw {};
        std::uint32_t frameNo = 0;
        float weight = 0.0f;

        if (!br.readBytes(morphNameRaw.data(), morphNameRaw.size()) ||
            !br.read(frameNo) ||
            !br.read(weight)) {
            core::log::error("VMD morph frame read failed at index " + std::to_string(i) + ": " + path);
            return false;
        }

        const std::string morphName = decodeShiftJisToUtf8(morphNameRaw.data(), morphNameRaw.size());
        if (morphName.empty()) {
            continue;
        }

        const float t = static_cast<float>(frameNo) / kFps;
        maxTime = std::max(maxTime, t);

        std::size_t trackIndex = 0;
        auto it = outClip.morphLookup.find(morphName);
        if (it == outClip.morphLookup.end()) {
            trackIndex = outClip.morphTracks.size();
            outClip.morphLookup[morphName] = trackIndex;
            VmdMorphTrack track;
            track.morphName = morphName;
            outClip.morphTracks.push_back(std::move(track));
        } else {
            trackIndex = it->second;
        }

        VmdMorphKeyframe kf;
        kf.time = t;
        kf.weight = weight;
        outClip.morphTracks[trackIndex].keyframes.push_back(kf);
    }

    for (auto& track : outClip.morphTracks) {
        std::sort(track.keyframes.begin(), track.keyframes.end(), [](const VmdMorphKeyframe& a, const VmdMorphKeyframe& b) {
            return a.time < b.time;
        });
    }

    outClip.duration = maxTime;
    if (outClip.duration <= 0.0f) {
        core::log::warn("VMD loaded but contains no bone animation: " + path);
    } else {
        core::log::info(
            "VMD loaded: boneTracks=" + std::to_string(outClip.boneTracks.size()) +
            " morphTracks=" + std::to_string(outClip.morphTracks.size()) +
            " duration=" + std::to_string(outClip.duration) + "s"
        );
    }

    return true;
}

} // namespace asset