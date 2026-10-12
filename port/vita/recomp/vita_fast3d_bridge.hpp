#pragma once

#include <cstddef>
#include <cstdint>

#include "vita_fast3d_abi.hpp"

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;

enum GBIVersion { GBI_0, GBI_1, GBI_2, GBI_1_S2DEX, GBI_2_S2DEX,
    GBI_BETA, GBI_DKR, GBI_LL, GBI_GE, GBI_CONKER, GBI_PD,
    GBI_ACCLAIM, GBI_AM };
struct UcodeInfo { GBIVersion version; };
UcodeInfo GBIMicrocode_DetectVersion(u32, u32, u32, u32);

constexpr u8 G_RDP_SETCIMG = 0xFF;
constexpr u8 G_RDP_SETZIMG = 0xFE;
constexpr u8 G_RDP_SETTIMG = 0xFD;
constexpr u8 G_RDP_SETTILE = 0xF5;
constexpr u8 G_RDP_LOADTILE = 0xF4;
constexpr u8 G_RDP_LOADBLOCK = 0xF3;
constexpr u8 G_RDP_LOADTLUT = 0xF0;
constexpr u8 G_RDP_RDPFULLSYNC = 0xE9;
constexpr u8 G_GBI2_MV_VIEWPORT = 8;
constexpr u8 G_GBI2_MV_LIGHT = 10;
constexpr u8 G_MV_VIEWPORT = 0x80;
constexpr u8 G_MV_LOOKATY = 0x82;
constexpr u8 G_MV_LOOKATX = 0x84;
constexpr u8 G_MV_L0 = 0x86;
constexpr u8 G_MV_L7 = 0x94;

class Fast3DMemoryBridge {
public:
    Fast3DMemoryBridge(u8* rdram, u32 rdram_size);
    ~Fast3DMemoryBridge();
    void SetInterpreter(Fast::Interpreter* interpreter) { mInterpreter = interpreter; }
    void* BuildDisplayList(u32 address, GBIVersion version);
    bool SawFullSync() const { return mSawFullSync; }

private:
    void Reset();
    void* Alloc(std::size_t size, std::size_t alignment = 16);
    u32 Resolve(u32 address) const;
    u32 Read32(u32 address) const;
    s16 ReadS16(u32 address) const;
    u16 ReadU16(u32 address) const;
    u8 ReadU8(u32 address) const;
    void* ConvertMatrix(u32 address);
    void* ConvertVertices(u32 address, u32 count);
    void* ConvertViewport(u32 address);
    void* ConvertLight(u32 address, u32 size);
    void* StageS2DEXImage(u32 address, u16 imageW, u16 imageH, u8 imageSiz);
    void* ConvertS2DEXBg(u32 address, bool scalable);
    void* ConvertS2DEXSprite(u32 address);
    void* ConvertS2DEXObjMtx(u32 address);
    void* ConvertS2DEXObjSubMtx(u32 address);
    void* ConvertS2DEXObjTxtr(u32 address);
    void* ConvertS2DEXObjTxSprite(u32 address);
    u32 CountListCommands(u32 address, GBIVersion version) const;
    void* TranslateList(u32 address, GBIVersion& version, u32 depth);
    void CopyRdramBytes(u8* dst, u32 address, u32 size) const;
    void HashRdramBytes(u32 address, u32 size, u32 hash[4]) const;
    bool StageTextureImage(u32 requiredBytes, bool palette = false,
                           u32 sourceOffset = 0, u32 rowBytes = 0,
                           u8 swizzleGroup = 0);

    struct ArenaChunk { u8* data; std::size_t capacity; std::size_t used; };
    struct TextureStage {
        u32 address, size, hash[4], validatedBuild, rowBytes;
        u8 swizzleGroup;
        bool dirty;
        u8* data;
    };
    u8* mRdram;
    u32 mRdramSize;
    u32 mSegments[16];
    Fast::Interpreter* mInterpreter;
    ArenaChunk* mArenaChunks;
    u32 mArenaChunkCount, mArenaChunkCapacity;
    TextureStage* mTextureStages;
    u32 mTextureStageCount, mTextureStageCapacity, mBuildSerial;
    bool mTranslationFailed, mSawFullSync, mHaveTextureImage;
    u32 mTextureImageW0, mTextureImageResolved;
    void* mTextureImageCommand;
    u8* mTextureImageStaging;
};
