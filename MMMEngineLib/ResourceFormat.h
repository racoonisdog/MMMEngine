#pragma once
#include <cstdint>

// Texture 포맷
namespace MMMEngine::Formats {

#pragma pack(push, 1)
    struct MTexHeader {
        static constexpr uint32_t MAGIC = 'MTEX';
        static constexpr uint32_t VERSION = 1;

        uint32_t magic = MAGIC;
        uint32_t version = VERSION;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 0;
        uint32_t format = 0;      // DXGI_FORMAT
        uint32_t arraySize = 1;
    };

    struct MTexMipData {
        uint64_t rowPitch = 0;
        uint64_t slicePitch = 0;
        // 이후 slicePitch 바이트의 픽셀 데이터
    };

    // Mesh 포맷
    struct MMeshHeader {
        static constexpr uint32_t MAGIC = 'MMSH';
        static constexpr uint32_t VERSION = 1;

        uint32_t magic = MAGIC;
        uint32_t version = VERSION;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;
    };

    struct MMeshVertex {
        float position[3];
        float normal[3];
        float texcoord[2];
        float tangent[3];
    };

    // Audio 포맷
    struct MAudioHeader {
        static constexpr uint32_t MAGIC = 'MAUD';
        static constexpr uint32_t VERSION = 1;

        uint32_t magic = MAGIC;
        uint32_t version = VERSION;
        uint32_t sampleRate = 0;
        uint32_t channels = 0;
        uint32_t bitsPerSample = 0;
        uint64_t dataSize = 0;
    };
#pragma pack(pop)

    static_assert(sizeof(MTexHeader) == 28, "MTexHeader size mismatch!");
    static_assert(sizeof(MTexMipData) == 16, "MTexMipData size mismatch!");
    static_assert(sizeof(MMeshHeader) == 20, "MMeshHeader size mismatch!");
    static_assert(sizeof(MMeshVertex) == 44, "MMeshVertex size mismatch!");
    static_assert(sizeof(MAudioHeader) == 28, "MAudioHeader size mismatch!");
}