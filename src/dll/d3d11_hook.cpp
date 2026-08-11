#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <climits>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <MinHook.h>

#pragma intrinsic(_ReturnAddress)
#include "d3d11_hook.h"
#include "game.h"
#include "sigscan.h"
#include "vr.h"
#include "title_adapter.h"
#include "../common/config.h"
#include "../common/coop_probe_logic.h"
#include "../common/log.h"
#include "../common/runtime_types.h"

// We can't hook "the game's swapchain" directly because it doesn't exist yet
// when we're injected. Instead we create a throwaway D3D11 device + swapchain
// of our own, read the addresses of Present/ResizeBuffers out of its vtable
// (all swapchains in the process share the same implementation), hook those,
// and throw the dummy away.

typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* Present1Fn)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void(STDMETHODCALLTYPE* OMSetRenderTargetsFn)(ID3D11DeviceContext*, UINT,
    ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
typedef HRESULT(STDMETHODCALLTYPE* CreateBufferFn)(ID3D11Device*,
    const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**);
typedef HRESULT(STDMETHODCALLTYPE* CreateInputLayoutFn)(ID3D11Device*,
    const D3D11_INPUT_ELEMENT_DESC*, UINT, const void*, SIZE_T,
    ID3D11InputLayout**);
typedef void(STDMETHODCALLTYPE* IASetInputLayoutFn)(ID3D11DeviceContext*,
    ID3D11InputLayout*);
typedef void(STDMETHODCALLTYPE* IASetVertexBuffersFn)(ID3D11DeviceContext*,
    UINT, UINT, ID3D11Buffer* const*, const UINT*, const UINT*);
typedef void(STDMETHODCALLTYPE* IASetIndexBufferFn)(ID3D11DeviceContext*,
    ID3D11Buffer*, DXGI_FORMAT, UINT);
typedef void(STDMETHODCALLTYPE* IASetPrimitiveTopologyFn)(ID3D11DeviceContext*,
    D3D11_PRIMITIVE_TOPOLOGY);
typedef void(STDMETHODCALLTYPE* VSSetConstantBuffersFn)(ID3D11DeviceContext*,
    UINT, UINT, ID3D11Buffer* const*);
typedef HRESULT(STDMETHODCALLTYPE* H3ProbeMapFn)(ID3D11DeviceContext*,
    ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
typedef void(STDMETHODCALLTYPE* H3ProbeUnmapFn)(ID3D11DeviceContext*,
    ID3D11Resource*, UINT);
typedef void(STDMETHODCALLTYPE* H3ProbeUpdateSubresourceFn)(
    ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*,
    const void*, UINT, UINT);
typedef void(STDMETHODCALLTYPE* DrawIndexedInstancedFn)(ID3D11DeviceContext*,
    UINT, UINT, UINT, INT, UINT);
typedef void(STDMETHODCALLTYPE* H3ProbeDrawIndexedFn)(ID3D11DeviceContext*,
    UINT, UINT, INT);
typedef void(STDMETHODCALLTYPE* H3ProbeDrawFn)(ID3D11DeviceContext*,
    UINT, UINT);
typedef void(STDMETHODCALLTYPE* H3ProbeDrawInstancedFn)(ID3D11DeviceContext*,
    UINT, UINT, UINT, UINT);
typedef void(STDMETHODCALLTYPE* H3ProbeDrawIndexedInstancedIndirectFn)(
    ID3D11DeviceContext*, ID3D11Buffer*, UINT);
typedef void(STDMETHODCALLTYPE* H3ProbeDrawInstancedIndirectFn)(
    ID3D11DeviceContext*, ID3D11Buffer*, UINT);
typedef void(__fastcall* H3ResourceFixupFn)(void*);
#if HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
typedef void(STDMETHODCALLTYPE* DrawIndexedFn)(ID3D11DeviceContext*, UINT, UINT, INT);
#endif
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
typedef void(STDMETHODCALLTYPE* CopyResourceFn)(ID3D11DeviceContext*,
    ID3D11Resource*, ID3D11Resource*);
#endif

static PresentFn g_origPresent = nullptr;
static Present1Fn g_origPresent1 = nullptr;
static ResizeBuffersFn g_origResizeBuffers = nullptr;
static OMSetRenderTargetsFn g_origOMSetRenderTargets = nullptr;
static CreateBufferFn g_origCreateBuffer = nullptr;
static CreateInputLayoutFn g_origCreateInputLayout = nullptr;
static IASetInputLayoutFn g_origIASetInputLayout = nullptr;
static IASetVertexBuffersFn g_origIASetVertexBuffers = nullptr;
static IASetIndexBufferFn g_origIASetIndexBuffer = nullptr;
static IASetPrimitiveTopologyFn g_origIASetPrimitiveTopology = nullptr;
static VSSetConstantBuffersFn g_origH3ProbeVSSetConstantBuffers = nullptr;
static H3ProbeMapFn g_origH3ProbeMap = nullptr;
static H3ProbeUnmapFn g_origH3ProbeUnmap = nullptr;
static H3ProbeUpdateSubresourceFn g_origH3ProbeUpdateSubresource = nullptr;
static DrawIndexedInstancedFn g_origDrawIndexedInstanced = nullptr;
static H3ProbeDrawIndexedFn g_origH3ProbeDrawIndexed = nullptr;
static H3ProbeDrawFn g_origH3ProbeDraw = nullptr;
static H3ProbeDrawInstancedFn g_origH3ProbeDrawInstanced = nullptr;
static H3ProbeDrawIndexedInstancedIndirectFn
    g_origH3ProbeDrawIndexedInstancedIndirect = nullptr;
static H3ProbeDrawInstancedIndirectFn
    g_origH3ProbeDrawInstancedIndirect = nullptr;
static H3ResourceFixupFn g_origH3ResourceFixup = nullptr;
static const uintptr_t* g_h3PackedAddressBaseLocation = nullptr;
static thread_local void* g_h3CurrentResourceContext = nullptr;
static std::atomic<bool> g_h3ResourceProbeInstallStarted{false};
static std::atomic<unsigned> g_h3DecoratorBufferProbeSamples{0};
static std::atomic<unsigned> g_h3DecoratorExactProbeSamples{0};
static std::atomic<unsigned> g_h3DecoratorBlockProbeSamples{0};
static std::atomic<bool> g_h3DecoratorLiveBlockScanStarted{false};
static std::atomic<bool> g_h3DecoratorDrawProbeEnabled{false};
static bool g_h3DecoratorDiagnosticEnabled = false;
static bool g_h3DecoratorContactCaptureEnabled = false;
static bool g_h3DecoratorSelfTestEnabled = false;
static std::atomic<uint32_t> g_h3DecoratorSelfTestState{0};
static std::atomic<uint32_t> g_h3DecoratorSelfTestAttempts{0};
constexpr bool kEnableH3DecoratorDrawFamilyProbe = true;
// Candidate b223294 proved that copying each bound vertex constant buffer in
// decorator draw callbacks can make the null-driver wall transaction miss its
// deadline. The captured evidence is preserved, but this behavior stays inert.
constexpr bool kEnableH3DecoratorShaderConstantProbe = false;
#if HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
static DrawIndexedFn g_origDrawIndexed = nullptr;
// The July 26 HUD-discovery detour performed synchronous GPU readback and
// logging before DrawIndexed. It produced no HUD/world evidence and remained
// active in every later Reach build. Candidate a50da4a proved removing it does
// not change the black-world defect. It remains disabled because restoring
// blocking GPU readback and file I/O to a render hot hook would be unsafe.
constexpr bool kEnableReachDrawIndexedDiagnostic = false;
#endif
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
static CopyResourceFn g_origCopyResource = nullptr;
#endif

// Environment-only decorator draw discovery.  The hot hooks below only copy
// raw binding state into fixed storage.  A separate worker drains that storage
// and performs the diagnostic logging, so no file I/O, allocation, COM call,
// lock, or GPU readback is introduced into a render callback.
constexpr unsigned kH3ProbeVertexSlots =
    D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
constexpr unsigned kH3ProbeBufferSlots = 1u << 15u;
constexpr unsigned kH3ProbeDrawSlots = 512u;
constexpr unsigned kH3ProbeInputLayoutSlots = 1u << 10u;
constexpr unsigned kH3ProbeInputElements = 16u;
constexpr unsigned kH3ProbeConstantSlots =
    D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
constexpr unsigned kH3ProbeConstantBytes = 512u;
constexpr unsigned kH3ProbeConstantBufferSlots = 1u << 11u;

struct H3ProbeBufferMetadata
{
    std::atomic<ID3D11Buffer*> key{nullptr};
    const void* source = nullptr;
    UINT byteWidth = 0;
    UINT bindFlags = 0;
    UINT miscFlags = 0;
    UINT structureStride = 0;
    uintptr_t halo3CreatorRva = UINTPTR_MAX;
    uintptr_t mccCreatorRva = UINTPTR_MAX;
    bool decoratorPlacement = false;
};

struct H3ProbeConstantBufferState
{
    std::atomic<ID3D11Buffer*> key{nullptr};
    UINT byteWidth = 0;
    std::atomic<unsigned> dataSequence{0};
    std::atomic<unsigned> dataBytes{0};
    uint8_t data[kH3ProbeConstantBytes]{};
};

struct H3ProbeConstantSnapshot
{
    ID3D11Buffer* buffer = nullptr;
    UINT byteWidth = 0;
    unsigned sequence = 0;
    unsigned dataBytes = 0;
    uint8_t data[kH3ProbeConstantBytes]{};
};

struct H3ProbeDrawRecord
{
    ID3D11Buffer* vertexBuffers[kH3ProbeVertexSlots]{};
    UINT vertexStrides[kH3ProbeVertexSlots]{};
    UINT vertexOffsets[kH3ProbeVertexSlots]{};
    ID3D11Buffer* indexBuffer = nullptr;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
    UINT indexOffset = 0;
    UINT indexCountPerInstance = 0;
    UINT instanceCount = 0;
    UINT startIndexLocation = 0;
    INT baseVertexLocation = 0;
    UINT startInstanceLocation = 0;
    unsigned placementSlot = UINT_MAX;
    unsigned kind = 0;
    ID3D11Buffer* indirectArgsBuffer = nullptr;
    UINT indirectArgsOffset = 0;
    ID3D11InputLayout* inputLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    H3ProbeConstantSnapshot vertexConstants[kH3ProbeConstantSlots]{};
};

struct H3ProbeInputElement
{
    char semantic[24]{};
    UINT semanticIndex = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT inputSlot = 0;
    UINT alignedByteOffset = 0;
    D3D11_INPUT_CLASSIFICATION inputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    UINT instanceDataStepRate = 0;
};

struct H3ProbeInputLayoutMetadata
{
    std::atomic<ID3D11InputLayout*> key{nullptr};
    UINT elementCount = 0;
    H3ProbeInputElement elements[kH3ProbeInputElements]{};
};

struct H3ProbeDrawSlot
{
    std::atomic<bool> ready{false};
    H3ProbeDrawRecord record{};
};

static H3ProbeBufferMetadata g_h3ProbeBuffers[kH3ProbeBufferSlots]{};
static H3ProbeConstantBufferState
    g_h3ProbeConstantBuffers[kH3ProbeConstantBufferSlots]{};
static H3ProbeInputLayoutMetadata
    g_h3ProbeInputLayouts[kH3ProbeInputLayoutSlots]{};
static H3ProbeDrawSlot g_h3ProbeDraws[kH3ProbeDrawSlots]{};
static std::atomic<unsigned> g_h3ProbeDrawCount{0};
static thread_local ID3D11Buffer*
    g_h3ProbeBoundVertexBuffers[kH3ProbeVertexSlots]{};
static thread_local UINT g_h3ProbeBoundVertexStrides[kH3ProbeVertexSlots]{};
static thread_local UINT g_h3ProbeBoundVertexOffsets[kH3ProbeVertexSlots]{};
static thread_local ID3D11Buffer* g_h3ProbeBoundIndexBuffer = nullptr;
static thread_local DXGI_FORMAT g_h3ProbeBoundIndexFormat = DXGI_FORMAT_UNKNOWN;
static thread_local UINT g_h3ProbeBoundIndexOffset = 0;
static thread_local ID3D11InputLayout* g_h3ProbeBoundInputLayout = nullptr;
static thread_local D3D11_PRIMITIVE_TOPOLOGY g_h3ProbeBoundTopology =
    D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
static thread_local ID3D11Buffer*
    g_h3ProbeBoundVertexConstants[kH3ProbeConstantSlots]{};
static thread_local ID3D11Resource* g_h3ProbeMappedResource = nullptr;
static thread_local void* g_h3ProbeMappedData = nullptr;
static thread_local unsigned g_h3ProbeMappedBytes = 0u;

constexpr unsigned kH3DecoratorFrameDraws = 128u;
struct H3DecoratorFrameDraw
{
    const uint8_t* geometrySource = nullptr;
    uint32_t geometryBytes = 0;
    uint32_t startVertex = 0;
    uint32_t vertexCount = 0;
    const uint8_t* placementSource = nullptr;
    uint32_t placementBytes = 0;
    uint32_t placementOffset = 0;
    uint32_t instanceCount = 0;
    PhysicalContactVec3 positionMinimum{};
    PhysicalContactVec3 positionSize{};
    PhysicalContactVec3 blockMinimum{};
    PhysicalContactVec3 blockStep{};
};

struct H3DecoratorFrame
{
    std::atomic<uint32_t> version{0};
    uint32_t drawCount = 0;
    H3DecoratorFrameDraw draws[kH3DecoratorFrameDraws]{};
};

static H3DecoratorFrame g_h3DecoratorFrames[3]{};
static unsigned g_h3DecoratorWriteFrame = 0;
static std::atomic<unsigned> g_h3DecoratorPublishedFrame{UINT_MAX};

static unsigned H3ProbeBufferHash(ID3D11Buffer* buffer)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(buffer);
    return static_cast<unsigned>((value >> 4u) ^ (value >> 21u)) &
        (kH3ProbeBufferSlots - 1u);
}

static unsigned H3ProbeConstantBufferHash(ID3D11Buffer* buffer)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(buffer);
    return static_cast<unsigned>((value >> 4u) ^ (value >> 17u)) &
        (kH3ProbeConstantBufferSlots - 1u);
}

static H3ProbeConstantBufferState* H3ProbeFindConstantBuffer(
    ID3D11Buffer* buffer)
{
    if (!buffer)
        return nullptr;
    const unsigned first = H3ProbeConstantBufferHash(buffer);
    for (unsigned probe = 0; probe < 16u; ++probe)
    {
        H3ProbeConstantBufferState& entry = g_h3ProbeConstantBuffers[
            (first + probe) & (kH3ProbeConstantBufferSlots - 1u)];
        ID3D11Buffer* key = entry.key.load(std::memory_order_acquire);
        if (key == buffer)
            return &entry;
        if (!key || key == reinterpret_cast<ID3D11Buffer*>(1u))
            return nullptr;
    }
    return nullptr;
}

static void H3ProbeRegisterConstantBuffer(
    ID3D11Buffer* buffer, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initialData)
{
    if (!buffer || !desc ||
        (desc->BindFlags & D3D11_BIND_CONSTANT_BUFFER) == 0u ||
        (!g_h3DecoratorDiagnosticEnabled &&
         desc->ByteWidth != 48u && desc->ByteWidth != 96u))
        return;
    const unsigned first = H3ProbeConstantBufferHash(buffer);
    ID3D11Buffer* const reserved = reinterpret_cast<ID3D11Buffer*>(1u);
    for (unsigned probe = 0; probe < 16u; ++probe)
    {
        H3ProbeConstantBufferState& entry = g_h3ProbeConstantBuffers[
            (first + probe) & (kH3ProbeConstantBufferSlots - 1u)];
        ID3D11Buffer* expected = nullptr;
        if (entry.key.compare_exchange_strong(
                expected, reserved, std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            entry.byteWidth = desc->ByteWidth;
            if (initialData && initialData->pSysMem && desc->ByteWidth > 0u)
            {
                const unsigned limit = g_h3DecoratorDiagnosticEnabled
                    ? kH3ProbeConstantBytes : 96u;
                const unsigned bytes = std::min(desc->ByteWidth, limit);
                std::memcpy(entry.data, initialData->pSysMem, bytes);
                entry.dataBytes.store(bytes, std::memory_order_relaxed);
                entry.dataSequence.store(2u, std::memory_order_relaxed);
            }
            entry.key.store(buffer, std::memory_order_release);
            return;
        }
        if (expected == buffer)
            return;
    }
}

static void H3ProbeRegisterBuffer(
    ID3D11Buffer* buffer, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initialData, bool decoratorPlacement,
    uintptr_t halo3CreatorRva, uintptr_t mccCreatorRva)
{
    if (!buffer || !desc)
        return;
    const unsigned first = H3ProbeBufferHash(buffer);
    ID3D11Buffer* const reserved = reinterpret_cast<ID3D11Buffer*>(1u);
    for (unsigned probe = 0; probe < 32u; ++probe)
    {
        H3ProbeBufferMetadata& entry =
            g_h3ProbeBuffers[(first + probe) & (kH3ProbeBufferSlots - 1u)];
        ID3D11Buffer* expected = nullptr;
        if (entry.key.compare_exchange_strong(
                expected, reserved, std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            entry.source = initialData ? initialData->pSysMem : nullptr;
            entry.byteWidth = desc->ByteWidth;
            entry.bindFlags = desc->BindFlags;
            entry.miscFlags = desc->MiscFlags;
            entry.structureStride = desc->StructureByteStride;
            entry.halo3CreatorRva = halo3CreatorRva;
            entry.mccCreatorRva = mccCreatorRva;
            entry.decoratorPlacement = decoratorPlacement;
            // Publish only after all plain fields are populated.  Readers only
            // consume metadata after an acquire load returns the exact key.
            entry.key.store(buffer, std::memory_order_release);
            return;
        }
        if (expected == buffer)
            return;
    }
}

static const H3ProbeBufferMetadata* H3ProbeFindBuffer(ID3D11Buffer* buffer)
{
    if (!buffer)
        return nullptr;
    const unsigned first = H3ProbeBufferHash(buffer);
    for (unsigned probe = 0; probe < 32u; ++probe)
    {
        const H3ProbeBufferMetadata& entry =
            g_h3ProbeBuffers[(first + probe) & (kH3ProbeBufferSlots - 1u)];
        ID3D11Buffer* key = entry.key.load(std::memory_order_acquire);
        if (key == buffer)
            return &entry;
        if (!key || key == reinterpret_cast<ID3D11Buffer*>(1u))
            return nullptr;
    }
    return nullptr;
}

static void H3ProbePublishBufferData(
    ID3D11Buffer* buffer, const void* source, unsigned sourceBytes)
{
    if (!buffer || !source || sourceBytes == 0u)
        return;
    H3ProbeConstantBufferState* state = H3ProbeFindConstantBuffer(buffer);
    if (!state)
        return;
    const unsigned limit = g_h3DecoratorDiagnosticEnabled
        ? kH3ProbeConstantBytes : 96u;
    const unsigned bytes = std::min(
        std::min(sourceBytes, state->byteWidth), limit);
    if (bytes == 0u)
        return;
    state->dataSequence.fetch_add(1u, std::memory_order_acq_rel);
    std::memcpy(state->data, source, bytes);
    state->dataBytes.store(bytes, std::memory_order_relaxed);
    state->dataSequence.fetch_add(1u, std::memory_order_release);
}

static void H3ProbeCaptureConstantSnapshot(
    ID3D11Buffer* buffer, H3ProbeConstantSnapshot& destination)
{
    destination.buffer = buffer;
    const H3ProbeBufferMetadata* metadata = H3ProbeFindBuffer(buffer);
    if (!metadata || (metadata->bindFlags & D3D11_BIND_CONSTANT_BUFFER) == 0u)
        return;
    H3ProbeConstantBufferState* state = H3ProbeFindConstantBuffer(buffer);
    if (!state)
        return;
    destination.byteWidth = state->byteWidth;
    for (unsigned attempt = 0; attempt < 3u; ++attempt)
    {
        const unsigned before =
            state->dataSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0u)
            continue;
        const unsigned bytes = std::min(
            state->dataBytes.load(std::memory_order_relaxed),
            kH3ProbeConstantBytes);
        if (bytes > 0u)
            std::memcpy(destination.data, state->data, bytes);
        const unsigned after =
            state->dataSequence.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0u)
        {
            destination.sequence = after;
            destination.dataBytes = bytes;
            return;
        }
    }
    destination.sequence = 0u;
    destination.dataBytes = 0u;
}

static bool H3DecoratorCopyConstant(
    ID3D11Buffer* buffer, void* destination, unsigned requiredBytes)
{
    if (!destination || !requiredBytes || requiredBytes > 96u)
        return false;
    H3ProbeConstantBufferState* state = H3ProbeFindConstantBuffer(buffer);
    if (!state || state->byteWidth < requiredBytes)
        return false;
    for (unsigned attempt = 0; attempt < 3u; ++attempt)
    {
        const unsigned before =
            state->dataSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0u ||
            state->dataBytes.load(std::memory_order_relaxed) < requiredBytes)
            continue;
        std::memcpy(destination, state->data, requiredBytes);
        const unsigned after =
            state->dataSequence.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0u)
            return true;
    }
    return false;
}

static unsigned H3ProbeInputLayoutHash(ID3D11InputLayout* layout)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(layout);
    return static_cast<unsigned>((value >> 4u) ^ (value >> 17u)) &
        (kH3ProbeInputLayoutSlots - 1u);
}

static void H3ProbeRegisterInputLayout(
    ID3D11InputLayout* layout, const D3D11_INPUT_ELEMENT_DESC* elements,
    UINT elementCount)
{
    if (!layout || !elements || elementCount == 0u ||
        elementCount > kH3ProbeInputElements)
        return;
    const unsigned first = H3ProbeInputLayoutHash(layout);
    ID3D11InputLayout* const reserved =
        reinterpret_cast<ID3D11InputLayout*>(1u);
    for (unsigned probe = 0; probe < 16u; ++probe)
    {
        H3ProbeInputLayoutMetadata& entry = g_h3ProbeInputLayouts[
            (first + probe) & (kH3ProbeInputLayoutSlots - 1u)];
        ID3D11InputLayout* expected = nullptr;
        if (entry.key.compare_exchange_strong(
                expected, reserved, std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            entry.elementCount = elementCount;
            for (UINT i = 0; i < elementCount; ++i)
            {
                H3ProbeInputElement& destination = entry.elements[i];
                const D3D11_INPUT_ELEMENT_DESC& source = elements[i];
                if (source.SemanticName)
                {
                    strncpy_s(destination.semantic, source.SemanticName,
                              _TRUNCATE);
                }
                destination.semanticIndex = source.SemanticIndex;
                destination.format = source.Format;
                destination.inputSlot = source.InputSlot;
                destination.alignedByteOffset = source.AlignedByteOffset;
                destination.inputSlotClass = source.InputSlotClass;
                destination.instanceDataStepRate = source.InstanceDataStepRate;
            }
            entry.key.store(layout, std::memory_order_release);
            return;
        }
        if (expected == layout)
            return;
    }
}

static const H3ProbeInputLayoutMetadata* H3ProbeFindInputLayout(
    ID3D11InputLayout* layout)
{
    if (!layout)
        return nullptr;
    const unsigned first = H3ProbeInputLayoutHash(layout);
    for (unsigned probe = 0; probe < 16u; ++probe)
    {
        const H3ProbeInputLayoutMetadata& entry = g_h3ProbeInputLayouts[
            (first + probe) & (kH3ProbeInputLayoutSlots - 1u)];
        ID3D11InputLayout* key = entry.key.load(std::memory_order_acquire);
        if (key == layout)
            return &entry;
        if (!key || key == reinterpret_cast<ID3D11InputLayout*>(1u))
            return nullptr;
    }
    return nullptr;
}

static HRESULT STDMETHODCALLTYPE H3ProbeCreateInputLayoutHook(
    ID3D11Device* device, const D3D11_INPUT_ELEMENT_DESC* elements,
    UINT elementCount, const void* shaderBytecode, SIZE_T bytecodeLength,
    ID3D11InputLayout** layout)
{
    const HRESULT result = g_origCreateInputLayout(
        device, elements, elementCount, shaderBytecode, bytecodeLength,
        layout);
    if (SUCCEEDED(result) && layout && *layout)
        H3ProbeRegisterInputLayout(*layout, elements, elementCount);
    return result;
}

static HRESULT H3ProbeCreateBuffer(
    ID3D11Device* device, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initialData, ID3D11Buffer** buffer,
    bool decoratorPlacement, uintptr_t halo3CreatorRva,
    uintptr_t mccCreatorRva)
{
    const HRESULT result =
        g_origCreateBuffer(device, desc, initialData, buffer);
    if (SUCCEEDED(result) && buffer && *buffer)
    {
        H3ProbeRegisterBuffer(
            *buffer, desc, initialData, decoratorPlacement,
            halo3CreatorRva, mccCreatorRva);
        H3ProbeRegisterConstantBuffer(*buffer, desc, initialData);
    }
    return result;
}

static void STDMETHODCALLTYPE H3ProbeIASetInputLayoutHook(
    ID3D11DeviceContext* context, ID3D11InputLayout* layout)
{
    g_h3ProbeBoundInputLayout = layout;
    g_origIASetInputLayout(context, layout);
}

static void STDMETHODCALLTYPE H3ProbeIASetVertexBuffersHook(
    ID3D11DeviceContext* context, UINT startSlot, UINT count,
    ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets)
{
    if (startSlot < kH3ProbeVertexSlots)
    {
        const UINT bounded = std::min(
            count, static_cast<UINT>(kH3ProbeVertexSlots - startSlot));
        for (UINT i = 0; i < bounded; ++i)
        {
            const UINT slot = startSlot + i;
            g_h3ProbeBoundVertexBuffers[slot] = buffers ? buffers[i] : nullptr;
            g_h3ProbeBoundVertexStrides[slot] = strides ? strides[i] : 0u;
            g_h3ProbeBoundVertexOffsets[slot] = offsets ? offsets[i] : 0u;
        }
    }
    g_origIASetVertexBuffers(
        context, startSlot, count, buffers, strides, offsets);
}

static void STDMETHODCALLTYPE H3ProbeIASetIndexBufferHook(
    ID3D11DeviceContext* context, ID3D11Buffer* buffer,
    DXGI_FORMAT format, UINT offset)
{
    g_h3ProbeBoundIndexBuffer = buffer;
    g_h3ProbeBoundIndexFormat = format;
    g_h3ProbeBoundIndexOffset = offset;
    g_origIASetIndexBuffer(context, buffer, format, offset);
}

static void STDMETHODCALLTYPE H3ProbeIASetPrimitiveTopologyHook(
    ID3D11DeviceContext* context, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    g_h3ProbeBoundTopology = topology;
    g_origIASetPrimitiveTopology(context, topology);
}

static void STDMETHODCALLTYPE H3ProbeVSSetConstantBuffersHook(
    ID3D11DeviceContext* context, UINT startSlot, UINT count,
    ID3D11Buffer* const* buffers)
{
    if (startSlot < kH3ProbeConstantSlots)
    {
        const UINT bounded = std::min(
            count, static_cast<UINT>(kH3ProbeConstantSlots - startSlot));
        for (UINT i = 0; i < bounded; ++i)
        {
            g_h3ProbeBoundVertexConstants[startSlot + i] =
                buffers ? buffers[i] : nullptr;
        }
    }
    g_origH3ProbeVSSetConstantBuffers(context, startSlot, count, buffers);
}

static HRESULT STDMETHODCALLTYPE H3ProbeMapHook(
    ID3D11DeviceContext* context, ID3D11Resource* resource,
    UINT subresource, D3D11_MAP mapType, UINT mapFlags,
    D3D11_MAPPED_SUBRESOURCE* mapped)
{
    const HRESULT result = g_origH3ProbeMap(
        context, resource, subresource, mapType, mapFlags, mapped);
    g_h3ProbeMappedResource = nullptr;
    g_h3ProbeMappedData = nullptr;
    g_h3ProbeMappedBytes = 0u;
    if (SUCCEEDED(result) && subresource == 0u && mapped && mapped->pData)
    {
        ID3D11Buffer* buffer = reinterpret_cast<ID3D11Buffer*>(resource);
        const H3ProbeBufferMetadata* metadata = H3ProbeFindBuffer(buffer);
        if (metadata &&
            (metadata->bindFlags & D3D11_BIND_CONSTANT_BUFFER) != 0u)
        {
            g_h3ProbeMappedResource = resource;
            g_h3ProbeMappedData = mapped->pData;
            g_h3ProbeMappedBytes = metadata->byteWidth;
        }
    }
    return result;
}

static void STDMETHODCALLTYPE H3ProbeUnmapHook(
    ID3D11DeviceContext* context, ID3D11Resource* resource,
    UINT subresource)
{
    if (subresource == 0u && resource == g_h3ProbeMappedResource &&
        g_h3ProbeMappedData && g_h3ProbeMappedBytes > 0u)
    {
        H3ProbePublishBufferData(
            reinterpret_cast<ID3D11Buffer*>(resource),
            g_h3ProbeMappedData, g_h3ProbeMappedBytes);
    }
    g_h3ProbeMappedResource = nullptr;
    g_h3ProbeMappedData = nullptr;
    g_h3ProbeMappedBytes = 0u;
    g_origH3ProbeUnmap(context, resource, subresource);
}

static void STDMETHODCALLTYPE H3ProbeUpdateSubresourceHook(
    ID3D11DeviceContext* context, ID3D11Resource* destination,
    UINT subresource, const D3D11_BOX* box, const void* source,
    UINT sourceRowPitch, UINT sourceDepthPitch)
{
    if (subresource == 0u && !box && source)
    {
        ID3D11Buffer* buffer = reinterpret_cast<ID3D11Buffer*>(destination);
        const H3ProbeBufferMetadata* metadata = H3ProbeFindBuffer(buffer);
        if (metadata &&
            (metadata->bindFlags & D3D11_BIND_CONSTANT_BUFFER) != 0u)
        {
            H3ProbePublishBufferData(buffer, source, metadata->byteWidth);
        }
    }
    g_origH3ProbeUpdateSubresource(
        context, destination, subresource, box, source,
        sourceRowPitch, sourceDepthPitch);
}

static void H3ProbeCaptureDraw(
    unsigned kind, UINT countPerInstance, UINT instanceCount,
    UINT startLocation, INT baseVertexLocation, UINT startInstanceLocation,
    ID3D11Buffer* indirectArgsBuffer = nullptr, UINT indirectArgsOffset = 0u)
{
    unsigned placementSlot = UINT_MAX;
    for (unsigned slot = 0; slot < kH3ProbeVertexSlots; ++slot)
    {
        const H3ProbeBufferMetadata* metadata =
            H3ProbeFindBuffer(g_h3ProbeBoundVertexBuffers[slot]);
        if (metadata && metadata->decoratorPlacement)
        {
            placementSlot = slot;
            break;
        }
    }
    if (placementSlot != UINT_MAX)
    {
        if (g_h3DecoratorContactCaptureEnabled && kind == 3u &&
            placementSlot != 0u &&
            g_h3ProbeBoundTopology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP &&
            g_h3ProbeBoundVertexStrides[0] == 20u &&
            g_h3ProbeBoundVertexStrides[placementSlot] == 16u)
        {
            const H3ProbeBufferMetadata* geometry =
                H3ProbeFindBuffer(g_h3ProbeBoundVertexBuffers[0]);
            const H3ProbeBufferMetadata* placements =
                H3ProbeFindBuffer(
                    g_h3ProbeBoundVertexBuffers[placementSlot]);
            float meshConstants[12]{};
            float blockConstants[24]{};
            const bool constants = H3DecoratorCopyConstant(
                    g_h3ProbeBoundVertexConstants[2], meshConstants,
                    sizeof(meshConstants)) &&
                H3DecoratorCopyConstant(
                    g_h3ProbeBoundVertexConstants[4], blockConstants,
                    sizeof(blockConstants));
            const uint64_t geometryFirstByte =
                static_cast<uint64_t>(g_h3ProbeBoundVertexOffsets[0]) +
                static_cast<uint64_t>(startLocation) * 20u;
            const uint64_t geometryEndByte = geometryFirstByte +
                static_cast<uint64_t>(countPerInstance) * 20u;
            const uint64_t placementFirstByte =
                static_cast<uint64_t>(
                    g_h3ProbeBoundVertexOffsets[placementSlot]) +
                static_cast<uint64_t>(startInstanceLocation) * 16u;
            const uint64_t placementEndByte = placementFirstByte +
                static_cast<uint64_t>(instanceCount) * 16u;
            H3DecoratorFrameDraw candidate{};
            if (geometry && placements && geometry->source &&
                placements->source && constants && countPerInstance >= 3u &&
                countPerInstance <=
                    PhysicalContactTriangleMesh::kMaximumTriangles + 2u &&
                instanceCount > 0u && instanceCount <= 4096u &&
                (g_h3ProbeBoundVertexOffsets[0] % 20u) == 0u &&
                geometryEndByte <= geometry->byteWidth &&
                placementEndByte <= placements->byteWidth)
            {
                candidate.geometrySource = static_cast<const uint8_t*>(
                    geometry->source);
                candidate.geometryBytes = geometry->byteWidth;
                candidate.startVertex =
                    g_h3ProbeBoundVertexOffsets[0] / 20u + startLocation;
                candidate.vertexCount = countPerInstance;
                candidate.placementSource = static_cast<const uint8_t*>(
                    placements->source);
                candidate.placementBytes = placements->byteWidth;
                candidate.placementOffset =
                    static_cast<uint32_t>(placementFirstByte);
                candidate.instanceCount = instanceCount;
                candidate.positionSize = {
                    meshConstants[0] * 0.5f,
                    meshConstants[1] * 0.5f,
                    meshConstants[2] * 0.5f};
                candidate.positionMinimum = {
                    meshConstants[4] * 0.5f,
                    meshConstants[5] * 0.5f,
                    meshConstants[6] * 0.5f};
                candidate.blockMinimum = {
                    blockConstants[0], blockConstants[1], blockConstants[2]};
                candidate.blockStep = {
                    blockConstants[4], blockConstants[5], blockConstants[6]};
                const bool finite =
                    PhysicalContactFinite(candidate.positionSize) &&
                    PhysicalContactFinite(candidate.positionMinimum) &&
                    PhysicalContactFinite(candidate.blockMinimum) &&
                    PhysicalContactFinite(candidate.blockStep) &&
                    candidate.positionSize.x > 0.0f &&
                    candidate.positionSize.y > 0.0f &&
                    candidate.positionSize.z > 0.0f &&
                    candidate.positionSize.x <= 10.0f &&
                    candidate.positionSize.y <= 10.0f &&
                    candidate.positionSize.z <= 10.0f &&
                    candidate.blockStep.x > 0.0f &&
                    candidate.blockStep.y > 0.0f &&
                    candidate.blockStep.z > 0.0f;
                if (finite)
                {
                    H3DecoratorFrame& frame =
                        g_h3DecoratorFrames[g_h3DecoratorWriteFrame];
                    bool duplicate = false;
                    for (uint32_t draw = 0; draw < frame.drawCount; ++draw)
                    {
                        if (std::memcmp(
                                &frame.draws[draw], &candidate,
                                sizeof(candidate)) == 0)
                        {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate && frame.drawCount < kH3DecoratorFrameDraws)
                        frame.draws[frame.drawCount++] = candidate;
                }
            }
        }
        if (g_h3DecoratorDiagnosticEnabled)
        {
            const unsigned sample =
                g_h3ProbeDrawCount.fetch_add(1u, std::memory_order_relaxed);
            if (sample >= kH3ProbeDrawSlots)
                return;
            H3ProbeDrawRecord& record = g_h3ProbeDraws[sample].record;
            std::memcpy(record.vertexBuffers, g_h3ProbeBoundVertexBuffers,
                        sizeof(record.vertexBuffers));
            std::memcpy(record.vertexStrides, g_h3ProbeBoundVertexStrides,
                        sizeof(record.vertexStrides));
            std::memcpy(record.vertexOffsets, g_h3ProbeBoundVertexOffsets,
                        sizeof(record.vertexOffsets));
            record.indexBuffer = g_h3ProbeBoundIndexBuffer;
            record.indexFormat = g_h3ProbeBoundIndexFormat;
            record.indexOffset = g_h3ProbeBoundIndexOffset;
            record.indexCountPerInstance = countPerInstance;
            record.instanceCount = instanceCount;
            record.startIndexLocation = startLocation;
            record.baseVertexLocation = baseVertexLocation;
            record.startInstanceLocation = startInstanceLocation;
            record.placementSlot = placementSlot;
            record.kind = kind;
            record.indirectArgsBuffer = indirectArgsBuffer;
            record.indirectArgsOffset = indirectArgsOffset;
            record.inputLayout = g_h3ProbeBoundInputLayout;
            record.topology = g_h3ProbeBoundTopology;
            if constexpr (kEnableH3DecoratorShaderConstantProbe)
            {
                for (unsigned constantSlot = 0;
                     constantSlot < kH3ProbeConstantSlots; ++constantSlot)
                {
                    H3ProbeCaptureConstantSnapshot(
                        g_h3ProbeBoundVertexConstants[constantSlot],
                        record.vertexConstants[constantSlot]);
                }
            }
            g_h3ProbeDraws[sample].ready.store(
                true, std::memory_order_release);
        }
    }
}

static void H3DecoratorPublishFrame()
{
    if (!g_h3DecoratorContactCaptureEnabled)
        return;
    H3DecoratorFrame& published =
        g_h3DecoratorFrames[g_h3DecoratorWriteFrame];
    published.version.fetch_add(1u, std::memory_order_release);
    g_h3DecoratorPublishedFrame.store(
        g_h3DecoratorWriteFrame, std::memory_order_release);
    g_h3DecoratorWriteFrame = (g_h3DecoratorWriteFrame + 1u) %
        static_cast<unsigned>(_countof(g_h3DecoratorFrames));
    H3DecoratorFrame& next = g_h3DecoratorFrames[g_h3DecoratorWriteFrame];
    uint32_t version = next.version.load(std::memory_order_relaxed);
    if ((version & 1u) == 0u)
        ++version;
    next.version.store(version, std::memory_order_release);
    next.drawCount = 0;
}

static bool H3DecoratorSafeCopy(
    void* destination, const void* source, size_t bytes)
{
    if (!destination || !source || !bytes)
        return false;
    __try
    {
        std::memcpy(destination, source, bytes);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static float H3DecoratorMeshRadius(const PhysicalContactTriangleMesh& mesh)
{
    float radius = 0.0f;
    for (uint16_t triangle = 0; triangle < mesh.triangleCount; ++triangle)
        for (const PhysicalContactVec3 point : mesh.triangles[triangle].vertices)
            radius = std::max(radius, PhysicalContactLength(point));
    return radius;
}

static float H3DecoratorPointSegmentDistanceSquared(
    PhysicalContactVec3 point, PhysicalContactVec3 start,
    PhysicalContactVec3 end)
{
    const PhysicalContactVec3 delta = end - start;
    const float lengthSquared = PhysicalContactLengthSquared(delta);
    const float fraction = lengthSquared > 1.0e-10f
        ? std::clamp(
              PhysicalContactDot(point - start, delta) / lengthSquared,
              0.0f, 1.0f)
        : 0.0f;
    return PhysicalContactLengthSquared(point - (start + delta * fraction));
}

size_t D3D_Halo3DecoratorWallPlanes(
    const PhysicalContactTriangleMesh& weapon,
    const PhysicalContactTransform& previousWeapon,
    const PhysicalContactTransform& currentWeapon,
    float stepWorldUnits, float surfaceRadiusWorldUnits,
    float clearanceWorldUnits, PhysicalContactWallPlane* planes,
    size_t planeCapacity, uint32_t* testedInstances, uint32_t* solidDraws)
{
    if (testedInstances)
        *testedInstances = 0;
    if (solidDraws)
        *solidDraws = 0;
    if (!g_h3DecoratorContactCaptureEnabled || !planes || !planeCapacity ||
        !PhysicalContactTriangleMeshValid(weapon) ||
        !PhysicalContactTransformFinite(previousWeapon) ||
        !PhysicalContactTransformFinite(currentWeapon) ||
        !std::isfinite(stepWorldUnits) || stepWorldUnits <= 0.0f ||
        !std::isfinite(surfaceRadiusWorldUnits) ||
        surfaceRadiusWorldUnits < 0.0f ||
        !std::isfinite(clearanceWorldUnits) || clearanceWorldUnits < 0.0f)
        return 0;

    std::array<H3DecoratorFrameDraw, kH3DecoratorFrameDraws> draws{};
    uint32_t drawCount = 0;
    bool copied = false;
    for (unsigned attempt = 0; attempt < 3u && !copied; ++attempt)
    {
        const unsigned index =
            g_h3DecoratorPublishedFrame.load(std::memory_order_acquire);
        if (index >= _countof(g_h3DecoratorFrames))
            break;
        const H3DecoratorFrame& frame = g_h3DecoratorFrames[index];
        const uint32_t before = frame.version.load(std::memory_order_acquire);
        if ((before & 1u) != 0u)
            continue;
        drawCount = std::min(frame.drawCount, kH3DecoratorFrameDraws);
        std::memcpy(draws.data(), frame.draws,
                    static_cast<size_t>(drawCount) * sizeof(draws[0]));
        const uint32_t after = frame.version.load(std::memory_order_acquire);
        copied = before == after && (after & 1u) == 0u;
    }
    if (!copied)
        return 0;

    constexpr uint32_t kMaximumExactInstances = 16u;
    constexpr size_t kGeometryBytes =
        (PhysicalContactTriangleMesh::kMaximumTriangles + 2u) * 20u;
    std::array<uint8_t, kGeometryBytes> geometryBytes{};
    std::array<uint8_t, 4096u * 16u> placementBytes{};
    const float weaponRadius = H3DecoratorMeshRadius(weapon) *
        std::max(previousWeapon.scale, currentWeapon.scale);
    uint32_t exactInstances = 0;
    size_t planeCount = 0;
    for (uint32_t drawIndex = 0;
         drawIndex < drawCount && planeCount < planeCapacity &&
         exactInstances < kMaximumExactInstances; ++drawIndex)
    {
        const H3DecoratorFrameDraw& draw = draws[drawIndex];
        const PhysicalContactVec3 blockMaximum{
            draw.blockMinimum.x + draw.blockStep.x * 65535.0f,
            draw.blockMinimum.y + draw.blockStep.y * 65535.0f,
            draw.blockMinimum.z + draw.blockStep.z * 65535.0f};
        const PhysicalContactVec3 localMaximum{
            std::max(
                std::fabs(draw.positionMinimum.x),
                std::fabs(draw.positionMinimum.x + draw.positionSize.x)),
            std::max(
                std::fabs(draw.positionMinimum.y),
                std::fabs(draw.positionMinimum.y + draw.positionSize.y)),
            std::max(
                std::fabs(draw.positionMinimum.z),
                std::fabs(draw.positionMinimum.z + draw.positionSize.z))};
        const float maximumTargetRadius =
            PhysicalContactLength(localMaximum) * std::sqrt(2.10f);
        const bool selfTestPending = g_h3DecoratorSelfTestEnabled &&
            g_h3DecoratorSelfTestState.load(std::memory_order_relaxed) == 0u;
        if (!PhysicalContactFinite(blockMaximum) ||
            !std::isfinite(maximumTargetRadius) ||
            (!selfTestPending && !PhysicalContactSegmentIntersectsExpandedAabb(
                previousWeapon.position, currentWeapon.position,
                draw.blockMinimum, blockMaximum,
                weaponRadius + maximumTargetRadius +
                    surfaceRadiusWorldUnits + clearanceWorldUnits)))
            continue;
        const size_t geometryOffset =
            static_cast<size_t>(draw.startVertex) * 20u;
        const size_t geometrySize =
            static_cast<size_t>(draw.vertexCount) * 20u;
        if (!draw.geometrySource || geometrySize > geometryBytes.size() ||
            geometryOffset > draw.geometryBytes ||
            geometrySize > draw.geometryBytes - geometryOffset ||
            !H3DecoratorSafeCopy(
                geometryBytes.data(), draw.geometrySource + geometryOffset,
                geometrySize))
            continue;
        PhysicalContactTriangleMesh target{};
        if (!PhysicalContactDecodeH3DecoratorTriangleStrip(
                geometryBytes.data(), geometrySize, 0u, draw.vertexCount,
                draw.positionMinimum, draw.positionSize, target) ||
            !PhysicalContactH3DecoratorMeshIsSolid(target))
            continue;
        if (solidDraws)
            ++*solidDraws;

        const size_t placementSize =
            static_cast<size_t>(draw.instanceCount) * 16u;
        if (!draw.placementSource || placementSize > placementBytes.size() ||
            draw.placementOffset > draw.placementBytes ||
            placementSize > draw.placementBytes - draw.placementOffset ||
            !H3DecoratorSafeCopy(
                placementBytes.data(),
                draw.placementSource + draw.placementOffset,
                placementSize))
            continue;

        for (uint32_t instance = 0;
             instance < draw.instanceCount && planeCount < planeCapacity &&
             exactInstances < kMaximumExactInstances; ++instance)
        {
            PhysicalContactTransform targetTransform{};
            if (!PhysicalContactDecodeH3DecoratorPlacement(
                    placementBytes.data() +
                        static_cast<size_t>(instance) * 16u,
                    draw.blockMinimum, draw.blockStep,
                    targetTransform))
                continue;
            const float targetRadius = target.groups[0].boundRadius *
                targetTransform.scale;
            if (selfTestPending &&
                g_h3DecoratorSelfTestState.load(
                    std::memory_order_relaxed) == 0u)
            {
                const uint32_t attempt =
                    g_h3DecoratorSelfTestAttempts.fetch_add(
                        1u, std::memory_order_relaxed) + 1u;
                if (attempt <= 8u)
                {
                    const PhysicalContactVec3 targetCentre =
                        PhysicalContactTransformPoint(
                            targetTransform, target.groups[0].centre);
                    const PhysicalContactVec3 axes[3] = {
                        targetTransform.forward, targetTransform.left,
                        targetTransform.up};
                    const float distance = std::max(
                        weaponRadius + targetRadius +
                            surfaceRadiusWorldUnits + clearanceWorldUnits,
                        stepWorldUnits * 4.0f);
                    for (const PhysicalContactVec3 rawAxis : axes)
                    {
                        const PhysicalContactVec3 axis =
                            PhysicalContactNormalize(rawAxis, {1.0f, 0.0f, 0.0f});
                        PhysicalContactTransform from = currentWeapon;
                        PhysicalContactTransform to = currentWeapon;
                        from.position = targetCentre - axis * distance;
                        to.position = targetCentre + axis * distance;
                        const float selfTestStep = std::max(
                            stepWorldUnits, distance * (2.0f / 128.0f));
                        if (PhysicalContactSweepTriangleMeshes(
                                weapon, from, to, target, targetTransform,
                                selfTestStep, surfaceRadiusWorldUnits).hit)
                        {
                            g_h3DecoratorSelfTestState.store(
                                1u, std::memory_order_release);
                            break;
                        }
                    }
                }
                if (attempt >= 8u &&
                    g_h3DecoratorSelfTestState.load(
                        std::memory_order_relaxed) == 0u)
                    g_h3DecoratorSelfTestState.store(
                        2u, std::memory_order_release);
            }
            const float broadphaseRadius = weaponRadius + targetRadius +
                surfaceRadiusWorldUnits + clearanceWorldUnits;
            const PhysicalContactVec3 targetCentre =
                PhysicalContactTransformPoint(
                    targetTransform, target.groups[0].centre);
            if (H3DecoratorPointSegmentDistanceSquared(
                    targetCentre, previousWeapon.position,
                    currentWeapon.position) >
                broadphaseRadius * broadphaseRadius)
                continue;

            ++exactInstances;
            if (testedInstances)
                ++*testedInstances;
            PhysicalContactTriangleMeshHit hit =
                PhysicalContactSweepTriangleMeshes(
                    weapon, previousWeapon, currentWeapon, target,
                    targetTransform, stepWorldUnits,
                    surfaceRadiusWorldUnits);
            if (!hit.hit || !PhysicalContactFinite(hit.normal) ||
                !PhysicalContactFinite(hit.targetPoint))
                continue;
            PhysicalContactVec3 normal = PhysicalContactNormalize(
                hit.normal, previousWeapon.position - hit.targetPoint);
            if (PhysicalContactDot(
                    previousWeapon.position - hit.targetPoint, normal) < 0.0f)
                normal = normal * -1.0f;
            const PhysicalContactVec3 weaponPoint =
                PhysicalContactTriangleMeshSupport(
                    weapon, currentWeapon, normal * -1.0f, 0.0f);
            const PhysicalContactVec3 targetPoint =
                PhysicalContactTriangleMeshSupport(
                    target, targetTransform, normal, 0.0f);
            if (!PhysicalContactFinite(weaponPoint) ||
                !PhysicalContactFinite(targetPoint))
                continue;
            planes[planeCount++] = {
                weaponPoint, targetPoint, normal, clearanceWorldUnits};
        }
    }
    return planeCount;
}

uint32_t D3D_Halo3DecoratorSelfTestState()
{
    return g_h3DecoratorSelfTestState.load(std::memory_order_acquire);
}

static void STDMETHODCALLTYPE H3ProbeDrawIndexedInstancedHook(
    ID3D11DeviceContext* context, UINT indexCountPerInstance,
    UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation,
    UINT startInstanceLocation)
{
    H3ProbeCaptureDraw(
        0u, indexCountPerInstance, instanceCount, startIndexLocation,
        baseVertexLocation, startInstanceLocation);
    g_origDrawIndexedInstanced(
        context, indexCountPerInstance, instanceCount, startIndexLocation,
        baseVertexLocation, startInstanceLocation);
}

static void STDMETHODCALLTYPE H3ProbeDrawIndexedHook(
    ID3D11DeviceContext* context, UINT indexCount, UINT startIndexLocation,
    INT baseVertexLocation)
{
    H3ProbeCaptureDraw(
        1u, indexCount, 1u, startIndexLocation, baseVertexLocation, 0u);
    g_origH3ProbeDrawIndexed(
        context, indexCount, startIndexLocation, baseVertexLocation);
}

static void STDMETHODCALLTYPE H3ProbeDrawHook(
    ID3D11DeviceContext* context, UINT vertexCount, UINT startVertexLocation)
{
    H3ProbeCaptureDraw(
        2u, vertexCount, 1u, startVertexLocation, 0, 0u);
    g_origH3ProbeDraw(context, vertexCount, startVertexLocation);
}

static void STDMETHODCALLTYPE H3ProbeDrawInstancedHook(
    ID3D11DeviceContext* context, UINT vertexCountPerInstance,
    UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
{
    H3ProbeCaptureDraw(
        3u, vertexCountPerInstance, instanceCount, startVertexLocation, 0,
        startInstanceLocation);
    g_origH3ProbeDrawInstanced(
        context, vertexCountPerInstance, instanceCount, startVertexLocation,
        startInstanceLocation);
}

static void STDMETHODCALLTYPE H3ProbeDrawIndexedInstancedIndirectHook(
    ID3D11DeviceContext* context, ID3D11Buffer* argsBuffer,
    UINT alignedByteOffsetForArgs)
{
    H3ProbeCaptureDraw(
        4u, 0u, 0u, 0u, 0, 0u, argsBuffer,
        alignedByteOffsetForArgs);
    g_origH3ProbeDrawIndexedInstancedIndirect(
        context, argsBuffer, alignedByteOffsetForArgs);
}

static void STDMETHODCALLTYPE H3ProbeDrawInstancedIndirectHook(
    ID3D11DeviceContext* context, ID3D11Buffer* argsBuffer,
    UINT alignedByteOffsetForArgs)
{
    H3ProbeCaptureDraw(
        5u, 0u, 0u, 0u, 0, 0u, argsBuffer,
        alignedByteOffsetForArgs);
    g_origH3ProbeDrawInstancedIndirect(
        context, argsBuffer, alignedByteOffsetForArgs);
}

static void H3ProbeLogBuffer(
    unsigned draw, const char* kind, unsigned slot, ID3D11Buffer* buffer,
    UINT stride, UINT offset)
{
    const H3ProbeBufferMetadata* metadata = H3ProbeFindBuffer(buffer);
    if (!metadata)
    {
        LOG("H3DECORDRAW%s[%u]: slot=%u buffer=%p untracked stride=%u offset=%u",
            kind, draw, slot, buffer, stride, offset);
        return;
    }
    uint32_t words[16]{};
    bool readable = false;
    if (metadata->source && metadata->byteWidth >= sizeof(words))
    {
        __try
        {
            std::memcpy(words, metadata->source, sizeof(words));
            readable = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            readable = false;
        }
    }
    LOG("H3DECORDRAW%s[%u]: slot=%u buffer=%p bytes=%u bind=0x%X misc=0x%X "
        "structureStride=%u stride=%u offset=%u placement=%d source=%p "
        "creatorHalo3Rva=0x%llX creatorMccRva=0x%llX readable=%d head="
        "%08X,%08X,%08X,%08X|%08X,%08X,%08X,%08X|"
        "%08X,%08X,%08X,%08X|%08X,%08X,%08X,%08X",
        kind, draw, slot, buffer, metadata->byteWidth, metadata->bindFlags,
        metadata->miscFlags, metadata->structureStride, stride, offset,
        metadata->decoratorPlacement ? 1 : 0, metadata->source,
        static_cast<unsigned long long>(metadata->halo3CreatorRva),
        static_cast<unsigned long long>(metadata->mccCreatorRva),
        readable ? 1 : 0, words[0], words[1], words[2], words[3],
        words[4], words[5], words[6], words[7], words[8], words[9],
        words[10], words[11], words[12], words[13], words[14], words[15]);
}

static void H3ProbeLogVertexConstants(
    unsigned draw, const H3ProbeDrawRecord& record)
{
    for (unsigned slot = 0; slot < kH3ProbeConstantSlots; ++slot)
    {
        const H3ProbeConstantSnapshot& snapshot =
            record.vertexConstants[slot];
        if (!snapshot.buffer || snapshot.dataBytes == 0u)
            continue;
        LOG("H3DECORCB[%u]: vsSlot=%u buffer=%p bytes=%u captured=%u "
            "sequence=%u",
            draw, slot, snapshot.buffer, snapshot.byteWidth,
            snapshot.dataBytes, snapshot.sequence);
        for (unsigned offset = 0; offset < snapshot.dataBytes;
             offset += 64u)
        {
            uint32_t words[16]{};
            const unsigned bytes = std::min(
                64u, snapshot.dataBytes - offset);
            std::memcpy(words, snapshot.data + offset, bytes);
            LOG("H3DECORCBDATA[%u]: vsSlot=%u offset=0x%03X words="
                "%08X,%08X,%08X,%08X|%08X,%08X,%08X,%08X|"
                "%08X,%08X,%08X,%08X|%08X,%08X,%08X,%08X",
                draw, slot, offset, words[0], words[1], words[2], words[3],
                words[4], words[5], words[6], words[7], words[8], words[9],
                words[10], words[11], words[12], words[13], words[14],
                words[15]);
        }
    }
}

static DWORD WINAPI H3ProbeDrawLoggerThread(void*)
{
    unsigned next = 0;
    ID3D11Buffer* loggedGeometry[64]{};
    unsigned loggedGeometryCount = 0u;
    while (g_h3DecoratorDrawProbeEnabled.load(std::memory_order_acquire))
    {
        const unsigned published = std::min(
            g_h3ProbeDrawCount.load(std::memory_order_acquire),
            kH3ProbeDrawSlots);
        while (next < published)
        {
            H3ProbeDrawSlot& slot = g_h3ProbeDraws[next];
            if (!slot.ready.load(std::memory_order_acquire))
                break;
            const H3ProbeDrawRecord& record = slot.record;
            LOG("H3DECORDRAW[%u]: kind=%u indexCount=%u instances=%u startIndex=%u "
                "baseVertex=%d startInstance=%u placementSlot=%u "
                "indexBuffer=%p format=%u indexOffset=%u argsBuffer=%p "
                "argsOffset=%u topology=%u inputLayout=%p",
                next, record.kind, record.indexCountPerInstance, record.instanceCount,
                record.startIndexLocation, record.baseVertexLocation,
                record.startInstanceLocation, record.placementSlot,
                record.indexBuffer, static_cast<unsigned>(record.indexFormat),
                record.indexOffset, record.indirectArgsBuffer,
                record.indirectArgsOffset,
                static_cast<unsigned>(record.topology), record.inputLayout);
            const H3ProbeInputLayoutMetadata* inputLayout =
                H3ProbeFindInputLayout(record.inputLayout);
            if (inputLayout)
            {
                for (UINT element = 0; element < inputLayout->elementCount;
                     ++element)
                {
                    const H3ProbeInputElement& value =
                        inputLayout->elements[element];
                    LOG("H3DECORDRAWLAYOUT[%u]: element=%u semantic=%s%u "
                        "format=%u slot=%u offset=%u class=%u step=%u",
                        next, element, value.semantic, value.semanticIndex,
                        static_cast<unsigned>(value.format), value.inputSlot,
                        value.alignedByteOffset,
                        static_cast<unsigned>(value.inputSlotClass),
                        value.instanceDataStepRate);
                }
            }
            else
            {
                LOG("H3DECORDRAWLAYOUT[%u]: layout=%p untracked",
                    next, record.inputLayout);
            }
            ID3D11Buffer* geometry = record.vertexBuffers[0];
            bool newGeometry = geometry && loggedGeometryCount <
                static_cast<unsigned>(_countof(loggedGeometry));
            for (unsigned i = 0; newGeometry && i < loggedGeometryCount; ++i)
            {
                if (loggedGeometry[i] == geometry)
                    newGeometry = false;
            }
            if (newGeometry)
            {
                loggedGeometry[loggedGeometryCount++] = geometry;
                H3ProbeLogVertexConstants(next, record);
            }
            for (unsigned vertexSlot = 0;
                 vertexSlot < kH3ProbeVertexSlots; ++vertexSlot)
            {
                if (!record.vertexBuffers[vertexSlot])
                    continue;
                H3ProbeLogBuffer(
                    next, "VB", vertexSlot,
                    record.vertexBuffers[vertexSlot],
                    record.vertexStrides[vertexSlot],
                    record.vertexOffsets[vertexSlot]);
            }
            H3ProbeLogBuffer(
                next, "IB", UINT_MAX, record.indexBuffer, 0u,
                record.indexOffset);
            if (record.indirectArgsBuffer)
            {
                H3ProbeLogBuffer(
                    next, "ARGS", UINT_MAX, record.indirectArgsBuffer, 0u,
                    record.indirectArgsOffset);
            }
            ++next;
        }
        Sleep(10u);
    }
    return 0;
}

// Environment-only discovery probe. Halo's decorator instance data is packed
// into 16-byte records and uploaded while a map loads. Reading immutable
// initial data here avoids the synchronous GPU readback that made the retired
// draw-hook diagnostics unsafe. The hook is not installed in normal play.
static void FormatProbeRecords(
    const uint8_t* bytes, unsigned byteWidth, unsigned firstRecord,
    char* output, size_t outputSize)
{
    if (!bytes || !output || outputSize == 0)
        return;
    output[0] = '\0';
    const size_t firstByte = static_cast<size_t>(firstRecord) * 16u;
    if (firstByte >= byteWidth)
        return;
    const unsigned available = byteWidth - static_cast<unsigned>(firstByte);
    const unsigned recordBytes = std::min(available, 16u * 4u);
    size_t used = 0;
    for (unsigned i = 0; i < recordBytes && used + 4u < outputSize; ++i)
    {
        const int wrote = _snprintf_s(
            output + used, outputSize - used, _TRUNCATE,
            "%02X%s", bytes[firstByte + i], ((i + 1u) % 16u) ? " " : "|");
        if (wrote <= 0)
            break;
        used += static_cast<size_t>(wrote);
    }
}

static uintptr_t ProbeModuleRva(const void* address, HMODULE module)
{
    if (!address || !module)
        return UINTPTR_MAX;
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return UINTPTR_MAX;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + static_cast<size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return UINTPTR_MAX;
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(base);
    const uintptr_t end = begin + nt->OptionalHeader.SizeOfImage;
    return value >= begin && value < end ? value - begin : UINTPTR_MAX;
}

static void FormatProbeStack(char* output, size_t outputSize)
{
    if (!output || outputSize == 0)
        return;
    output[0] = '\0';
    void* frames[12]{};
    const USHORT count = RtlCaptureStackBackTrace(
        1, static_cast<ULONG>(_countof(frames)), frames, nullptr);
    const HMODULE halo3 = GetModuleHandleW(L"halo3.dll");
    const HMODULE mcc = GetModuleHandleW(nullptr);
    size_t used = 0;
    for (USHORT i = 0; i < count && used + 24u < outputSize; ++i)
    {
        const uintptr_t halo3Rva = ProbeModuleRva(frames[i], halo3);
        const uintptr_t mccRva = ProbeModuleRva(frames[i], mcc);
        const char* module = halo3Rva != UINTPTR_MAX ? "h3" :
            (mccRva != UINTPTR_MAX ? "mcc" : "other");
        const uintptr_t rva = halo3Rva != UINTPTR_MAX ? halo3Rva :
            (mccRva != UINTPTR_MAX ? mccRva : 0u);
        const int wrote = _snprintf_s(
            output + used, outputSize - used, _TRUNCATE,
            "%s%s+0x%llX", i ? " " : "", module,
            static_cast<unsigned long long>(rva));
        if (wrote <= 0)
            break;
        used += static_cast<size_t>(wrote);
    }
}

static void __fastcall H3ResourceFixupHook(void* context)
{
    uint8_t before[11u][16u]{};
    const uint8_t* resourceBase = nullptr;
    const uint8_t* fixups = nullptr;
    int32_t fixupCount = 0;
    bool decoratorResource = false;
    if (context)
    {
        __try
        {
            const uint8_t* bytes = static_cast<const uint8_t*>(context);
            std::memcpy(&resourceBase, bytes, sizeof(resourceBase));
            std::memcpy(&fixupCount, bytes + 0x40u, sizeof(fixupCount));
            std::memcpy(&fixups, bytes + 0x48u, sizeof(fixups));
            decoratorResource = resourceBase && fixups && fixupCount == 11;
            for (unsigned i = 0; decoratorResource && i < 11u; ++i)
            {
                uint32_t encoded = 0;
                int32_t kind = -1;
                std::memcpy(&encoded, fixups + static_cast<size_t>(i) * 8u,
                            sizeof(encoded));
                std::memcpy(&kind,
                            fixups + static_cast<size_t>(i) * 8u + 4u,
                            sizeof(kind));
                const uint32_t expectedOffset = 0x134u + i * 0x0Cu;
                if ((encoded >> 29u) != 1u ||
                    (encoded & 0x1FFFFFFFu) != expectedOffset || kind != 0)
                {
                    decoratorResource = false;
                    break;
                }
                std::memcpy(before[i], resourceBase + expectedOffset,
                            sizeof(before[i]));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            decoratorResource = false;
        }
    }

    void* previous = g_h3CurrentResourceContext;
    g_h3CurrentResourceContext = context;
    g_origH3ResourceFixup(context);
    g_h3CurrentResourceContext = previous;

    if (decoratorResource)
    {
        __try
        {
            uintptr_t packedAddressBase = 0;
            if (g_h3PackedAddressBaseLocation)
                std::memcpy(&packedAddressBase,
                            g_h3PackedAddressBaseLocation,
                            sizeof(packedAddressBase));
            LOG("H3DECOROWNER: context=%p base=%p fixups=%p count=%d",
                context, resourceBase, fixups, fixupCount);
            for (unsigned i = 0; i < 11u; ++i)
            {
                const uint32_t offset = 0x134u + i * 0x0Cu;
                const uint8_t* after = resourceBase + offset;
                uint32_t beforeWords[4]{};
                uint32_t afterWords[4]{};
                std::memcpy(beforeWords, before[i], sizeof(beforeWords));
                std::memcpy(afterWords, after, sizeof(afterWords));
                LOG("H3DECOROWNERSET[%u]: field=%p offset=0x%X "
                    "before=%08X,%08X,%08X,%08X "
                    "after=%08X,%08X,%08X,%08X",
                    i, after, offset,
                    beforeWords[0], beforeWords[1], beforeWords[2],
                    beforeWords[3], afterWords[0], afterWords[1],
                    afterWords[2], afterWords[3]);
                const uintptr_t resolved = afterWords[1] && packedAddressBase
                    ? packedAddressBase + static_cast<uintptr_t>(afterWords[1]) * 4u
                    : 0u;
                if (resolved)
                {
                    uint64_t head[16]{};
                    std::memcpy(head, reinterpret_cast<const void*>(resolved),
                                sizeof(head));
                    LOG("H3DECOROWNERRESOLVED[%u]: packedBase=%p handle=0x%08X "
                        "pointer=%p head="
                        "%016llX,%016llX,%016llX,%016llX|"
                        "%016llX,%016llX,%016llX,%016llX|"
                        "%016llX,%016llX,%016llX,%016llX|"
                        "%016llX,%016llX,%016llX,%016llX",
                        i, reinterpret_cast<const void*>(packedAddressBase),
                        afterWords[1], reinterpret_cast<const void*>(resolved),
                        static_cast<unsigned long long>(head[0]),
                        static_cast<unsigned long long>(head[1]),
                        static_cast<unsigned long long>(head[2]),
                        static_cast<unsigned long long>(head[3]),
                        static_cast<unsigned long long>(head[4]),
                        static_cast<unsigned long long>(head[5]),
                        static_cast<unsigned long long>(head[6]),
                        static_cast<unsigned long long>(head[7]),
                        static_cast<unsigned long long>(head[8]),
                        static_cast<unsigned long long>(head[9]),
                        static_cast<unsigned long long>(head[10]),
                        static_cast<unsigned long long>(head[11]),
                        static_cast<unsigned long long>(head[12]),
                        static_cast<unsigned long long>(head[13]),
                        static_cast<unsigned long long>(head[14]),
                        static_cast<unsigned long long>(head[15]));

                    // The resolved handles are spaced 0x28 bytes apart.  The
                    // first three qwords in each entry look like a packed
                    // placement-count value followed by two source pointers.
                    // Snapshot both pointed records here so the H3EK placement
                    // layout can prove their meaning before production code
                    // relies on either one.  This remains environment-only.
                    const uintptr_t authoredPlacements =
                        static_cast<uintptr_t>(head[1]);
                    const uintptr_t packedPlacements =
                        static_cast<uintptr_t>(head[2]);
                    if (authoredPlacements && packedPlacements)
                    {
                        uint32_t authoredWords[16]{};
                        uint32_t packedWords[16]{};
                        std::memcpy(
                            authoredWords,
                            reinterpret_cast<const void*>(authoredPlacements),
                            sizeof(authoredWords));
                        std::memcpy(
                            packedWords,
                            reinterpret_cast<const void*>(packedPlacements),
                            sizeof(packedWords));
                        float authoredPosition[3]{};
                        std::memcpy(authoredPosition, authoredWords,
                                    sizeof(authoredPosition));
                        LOG("H3DECOROWNERDATA[%u]: descriptor=%016llX "
                            "authored=%p firstPosition=%.6f,%.6f,%.6f "
                            "authoredWords="
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X "
                            "packed=%p packedWords="
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X|"
                            "%08X,%08X,%08X,%08X",
                            i, static_cast<unsigned long long>(head[0]),
                            reinterpret_cast<const void*>(authoredPlacements),
                            authoredPosition[0], authoredPosition[1],
                            authoredPosition[2],
                            authoredWords[0], authoredWords[1],
                            authoredWords[2], authoredWords[3],
                            authoredWords[4], authoredWords[5],
                            authoredWords[6], authoredWords[7],
                            authoredWords[8], authoredWords[9],
                            authoredWords[10], authoredWords[11],
                            authoredWords[12], authoredWords[13],
                            authoredWords[14], authoredWords[15],
                            reinterpret_cast<const void*>(packedPlacements),
                            packedWords[0], packedWords[1], packedWords[2],
                            packedWords[3], packedWords[4], packedWords[5],
                            packedWords[6], packedWords[7], packedWords[8],
                            packedWords[9], packedWords[10], packedWords[11],
                            packedWords[12], packedWords[13], packedWords[14],
                            packedWords[15]);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            LOG("H3DECOROWNER: post-fixup field read faulted");
        }
    }
}

static void TryInstallH3ResourceFixupProbe()
{
    if (g_origH3ResourceFixup || !GetModuleHandleW(L"halo3.dll"))
        return;
    bool expected = false;
    if (!g_h3ResourceProbeInstallStarted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return;

    static constexpr char kH3ResourceFixupSignature[] =
        "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 "
        "41 56 41 57 48 8D 68 A1 48 81 EC A0 00 00 00 0F 10 41 30 83 "
        "65 FF 00";
    uintptr_t moduleBase = 0;
    size_t moduleSize = 0;
    uintptr_t fixup = 0;
    uintptr_t secondFixup = 0;
    if (sig::ModuleRange(L"halo3.dll", moduleBase, moduleSize))
    {
        fixup = sig::Find(moduleBase, moduleSize, kH3ResourceFixupSignature);
        if (fixup && fixup + 1u < moduleBase + moduleSize)
            secondFixup = sig::Find(
                fixup + 1u, moduleBase + moduleSize - fixup - 1u,
                kH3ResourceFixupSignature);
    }
    if (fixup && !secondFixup &&
        MH_CreateHook(reinterpret_cast<void*>(fixup),
                      (void*)&H3ResourceFixupHook,
                      (void**)&g_origH3ResourceFixup) == MH_OK &&
        MH_EnableHook(reinterpret_cast<void*>(fixup)) == MH_OK)
    {
        const uint8_t* packedBaseInstruction =
            reinterpret_cast<const uint8_t*>(fixup + 0xE9u);
        if (packedBaseInstruction[0] == 0x48u &&
            packedBaseInstruction[1] == 0x2Bu &&
            packedBaseInstruction[2] == 0x05u)
        {
            int32_t displacement = 0;
            std::memcpy(&displacement, packedBaseInstruction + 3u,
                        sizeof(displacement));
            g_h3PackedAddressBaseLocation =
                reinterpret_cast<const uintptr_t*>(fixup + 0xF0u + displacement);
        }
        LOG("H3DECORBUF: resource-owner probe installed after Halo 3 load "
            "(environment-only; owner=+0x%llX packedBaseLocation=%p)",
            static_cast<unsigned long long>(fixup - moduleBase),
            g_h3PackedAddressBaseLocation);
        return;
    }
    if (fixup)
        MH_RemoveHook(reinterpret_cast<void*>(fixup));
    g_origH3ResourceFixup = nullptr;
    LOG("H3DECORBUF: resource-owner signature missing, ambiguous, or hook "
        "failed; buffer-only probe remains active");
}

static void LogProbeResourceContext(const void* source, unsigned records)
{
    MEMORY_BASIC_INFORMATION sourceInfo{};
    VirtualQuery(source, &sourceInfo, sizeof(sourceInfo));
    const uint8_t* context = static_cast<const uint8_t*>(
        g_h3CurrentResourceContext);
    if (!context)
    {
        LOG("H3DECORCTX: records=%u source=%p allocation=%p region=%p+0x%llX "
            "protect=0x%X context=null",
            records, source, sourceInfo.AllocationBase, sourceInfo.BaseAddress,
            static_cast<unsigned long long>(sourceInfo.RegionSize),
            static_cast<unsigned>(sourceInfo.Protect));
        return;
    }

    int32_t fixupCount = 0;
    const uint8_t* fixups = nullptr;
    std::memcpy(&fixupCount, context + 0x40u, sizeof(fixupCount));
    std::memcpy(&fixups, context + 0x48u, sizeof(fixups));
    char head[0x60u * 3u + 1u]{};
    size_t used = 0;
    for (unsigned i = 0; i < 0x60u && used + 4u < sizeof(head); ++i)
    {
        const int wrote = _snprintf_s(
            head + used, sizeof(head) - used, _TRUNCATE,
            "%02X%s", context[i], ((i + 1u) % 16u) ? " " : "|");
        if (wrote <= 0)
            break;
        used += static_cast<size_t>(wrote);
    }
    LOG("H3DECORCTX: records=%u source=%p allocation=%p region=%p+0x%llX "
        "protect=0x%X context=%p fixupCount=%d fixups=%p head=%s",
        records, source, sourceInfo.AllocationBase, sourceInfo.BaseAddress,
        static_cast<unsigned long long>(sourceInfo.RegionSize),
        static_cast<unsigned>(sourceInfo.Protect), context, fixupCount, fixups,
        head);

    if (fixupCount <= 0 || fixupCount > 4096 || !fixups)
        return;
    const unsigned sampleCount = std::min(static_cast<unsigned>(fixupCount), 96u);
    for (unsigned i = 0; i < sampleCount; ++i)
    {
        uint32_t encoded = 0;
        int32_t kind = -1;
        std::memcpy(&encoded, fixups + static_cast<size_t>(i) * 8u, sizeof(encoded));
        std::memcpy(&kind, fixups + static_cast<size_t>(i) * 8u + 4u, sizeof(kind));
        LOG("H3DECORFIXUP[%u/%u]: encoded=0x%08X addressKind=%u offset=0x%X "
            "fixupKind=%d",
            i, sampleCount, encoded, encoded >> 29u,
            encoded & 0x1FFFFFFFu, kind);
    }
}

static bool IsProbeReadableProtection(DWORD protect)
{
    if ((protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
        return false;
    switch (protect & 0xFFu)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

// Valhalla-only discovery filter. The bounds below come from the official H3EK
// riverworld decorator resource and are deliberately not used by normal game
// behavior. Run this while the 956-record placement stream is being uploaded:
// the CPU-side 0x3c-byte decode blocks have already been released by the time a
// post-load process scan can run.
static void ScanProbeDecoratorBlocks(const void* source, unsigned records)
{
    if (records != 956u || !source)
        return;
    bool expected = false;
    if (!g_h3DecoratorLiveBlockScanStarted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return;

    MEMORY_BASIC_INFORMATION sourceInfo{};
    if (VirtualQuery(source, &sourceInfo, sizeof(sourceInfo)) == 0 ||
        !sourceInfo.AllocationBase)
    {
        LOG("H3DECORLIVE: source query failed");
        return;
    }

    constexpr size_t kBlockBytes = 0x3Cu;
    constexpr size_t kMaximumAllocationSpan = 2ull * 1024ull * 1024ull * 1024ull;
    const uintptr_t allocation =
        reinterpret_cast<uintptr_t>(sourceInfo.AllocationBase);
    uintptr_t cursor = allocation;
    size_t allocationSpan = 0;
    size_t readableBytes = 0;
    unsigned candidates = 0;
    unsigned faults = 0;
    uintptr_t candidateAddresses[512]{};
    unsigned storedCandidates = 0;

    while (allocationSpan < kMaximumAllocationSpan)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                         sizeof(info)) == 0 ||
            reinterpret_cast<uintptr_t>(info.AllocationBase) != allocation)
            break;

        const uintptr_t base = reinterpret_cast<uintptr_t>(info.BaseAddress);
        const size_t regionSize = info.RegionSize;
        if (regionSize == 0 || base > UINTPTR_MAX - regionSize)
            break;
        const uintptr_t end = base + regionSize;
        allocationSpan = static_cast<size_t>(end - allocation);

        if (info.State == MEM_COMMIT && IsProbeReadableProtection(info.Protect) &&
            regionSize >= kBlockBytes)
        {
            readableBytes += regionSize;
            __try
            {
                const uintptr_t aligned = (base + 3u) & ~uintptr_t{3u};
                for (uintptr_t address = aligned;
                     address <= end - kBlockBytes; address += 4u)
                {
                    const uint8_t* block = reinterpret_cast<const uint8_t*>(address);
                    uint16_t count = 0;
                    uint32_t start = 0;
                    float minimum[3]{};
                    float step[3]{};
                    std::memcpy(&count, block, sizeof(count));
                    if (count == 0u || count > records || block[2] >= 11u ||
                        block[3] >= 11u)
                        continue;
                    std::memcpy(&start, block + 4u, sizeof(start));
                    if ((start % 16u) != 0u || start / 16u > records ||
                        count > records - start / 16u)
                        continue;
                    std::memcpy(minimum, block + 8u, sizeof(minimum));
                    std::memcpy(step, block + 0x18u, sizeof(step));
                    if (!std::isfinite(minimum[0]) ||
                        !std::isfinite(minimum[1]) ||
                        !std::isfinite(minimum[2]) ||
                        !std::isfinite(step[0]) ||
                        !std::isfinite(step[1]) ||
                        !std::isfinite(step[2]) ||
                        minimum[0] < 40.0f || minimum[0] > 110.0f ||
                        minimum[1] < -170.0f || minimum[1] > -35.0f ||
                        minimum[2] < -10.0f || minimum[2] > 10.0f ||
                        step[0] < 1.0e-7f || step[0] > 0.01f ||
                        step[1] < 1.0e-7f || step[1] > 0.01f ||
                        step[2] < 1.0e-7f || step[2] > 0.01f)
                        continue;
                    const float maximum[3] = {
                        minimum[0] + 65535.0f * step[0],
                        minimum[1] + 65535.0f * step[1],
                        minimum[2] + 65535.0f * step[2],
                    };
                    if (maximum[0] < 40.0f || maximum[0] > 110.0f ||
                        maximum[1] < -170.0f || maximum[1] > -35.0f ||
                        maximum[2] < -10.0f || maximum[2] > 10.0f)
                        continue;

                    uint32_t tail30 = 0;
                    uint32_t tail34 = 0;
                    std::memcpy(&tail30, block + 0x30u, sizeof(tail30));
                    std::memcpy(&tail34, block + 0x34u, sizeof(tail34));
                    ++candidates;
                    if (storedCandidates < _countof(candidateAddresses))
                        candidateAddresses[storedCandidates++] = address;
                    if (candidates <= 256u)
                    {
                        LOG("H3DECORLIVEBLOCK[%u]: address=%p count=%u set=%u "
                            "buffer=%u start=%u min=%.6f,%.6f,%.6f "
                            "step=%.9f,%.9f,%.9f max=%.6f,%.6f,%.6f "
                            "tail30=0x%08X tail34=0x%08X",
                            candidates, block, count,
                            static_cast<unsigned>(block[2]),
                            static_cast<unsigned>(block[3]), start,
                            minimum[0], minimum[1], minimum[2],
                            step[0], step[1], step[2], maximum[0], maximum[1],
                            maximum[2], tail30, tail34);
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                ++faults;
            }
        }
        cursor = end;
    }

    unsigned sequences = 0;
    for (unsigned i = 0; i < storedCandidates;)
    {
        const unsigned first = i;
        while (i + 1u < storedCandidates &&
               candidateAddresses[i + 1u] ==
                   candidateAddresses[i] + kBlockBytes)
            ++i;
        ++sequences;
        if (sequences <= 128u)
        {
            LOG("H3DECORLIVESEQ[%u]: first=%p blocks=%u bytes=0x%llX",
                sequences, reinterpret_cast<const void*>(candidateAddresses[first]),
                i - first + 1u,
                static_cast<unsigned long long>((i - first + 1u) * kBlockBytes));
        }
        ++i;
    }

    // Look for an owning pointer to the discovered table. Exact production
    // capture must follow that owner; scanning a 2 GiB resource reservation is
    // acceptable for this one-shot probe but never for normal map loading.
    unsigned contextRefs = 0;
    unsigned allocationRefs = 0;
    if (storedCandidates != 0u)
    {
        const uintptr_t firstCandidate = candidateAddresses[0];
        const uintptr_t lastCandidate =
            candidateAddresses[storedCandidates - 1u] + kBlockBytes;
        const uint8_t* context =
            static_cast<const uint8_t*>(g_h3CurrentResourceContext);
        if (context)
        {
            __try
            {
                for (unsigned offset = 0; offset + sizeof(uintptr_t) <= 0x200u;
                     offset += 8u)
                {
                    uintptr_t value = 0;
                    std::memcpy(&value, context + offset, sizeof(value));
                    if (value >= firstCandidate && value < lastCandidate)
                    {
                        ++contextRefs;
                        LOG("H3DECORLIVEREFCTX[%u]: context=%p offset=0x%X "
                            "value=%p delta=0x%llX",
                            contextRefs, context, offset,
                            reinterpret_cast<const void*>(value),
                            static_cast<unsigned long long>(value - firstCandidate));
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                ++faults;
            }
        }

        cursor = allocation;
        size_t referenceSpan = 0;
        while (referenceSpan < kMaximumAllocationSpan)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                             sizeof(info)) == 0 ||
                reinterpret_cast<uintptr_t>(info.AllocationBase) != allocation)
                break;
            const uintptr_t base = reinterpret_cast<uintptr_t>(info.BaseAddress);
            const size_t regionSize = info.RegionSize;
            if (regionSize == 0 || base > UINTPTR_MAX - regionSize)
                break;
            const uintptr_t end = base + regionSize;
            referenceSpan = static_cast<size_t>(end - allocation);
            if (info.State == MEM_COMMIT &&
                IsProbeReadableProtection(info.Protect) &&
                regionSize >= sizeof(uintptr_t))
            {
                __try
                {
                    const uintptr_t aligned = (base + 7u) & ~uintptr_t{7u};
                    for (uintptr_t address = aligned;
                         address <= end - sizeof(uintptr_t); address += 8u)
                    {
                        uintptr_t value = 0;
                        std::memcpy(&value,
                                    reinterpret_cast<const void*>(address),
                                    sizeof(value));
                        if (value < firstCandidate || value >= lastCandidate)
                            continue;
                        ++allocationRefs;
                        if (allocationRefs <= 256u)
                        {
                            LOG("H3DECORLIVEREF[%u]: address=%p value=%p "
                                "delta=0x%llX",
                                allocationRefs,
                                reinterpret_cast<const void*>(address),
                                reinterpret_cast<const void*>(value),
                                static_cast<unsigned long long>(
                                    value - firstCandidate));
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    ++faults;
                }
            }
            cursor = end;
        }
    }
    LOG("H3DECORLIVE: allocation=%p span=0x%llX readable=0x%llX "
        "candidates=%u stored=%u sequences=%u contextRefs=%u "
        "allocationRefs=%u faults=%u",
        sourceInfo.AllocationBase,
        static_cast<unsigned long long>(allocationSpan),
        static_cast<unsigned long long>(readableBytes), candidates,
        storedCandidates, sequences, contextRefs, allocationRefs, faults);
}

static HRESULT STDMETHODCALLTYPE CreateBufferHook(
    ID3D11Device* device, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initialData, ID3D11Buffer** buffer)
{
    if (g_h3DecoratorDiagnosticEnabled)
        TryInstallH3ResourceFixupProbe();
    bool decoratorPlacement = false;
    const void* caller = _ReturnAddress();
    const uintptr_t halo3Rva = ProbeModuleRva(
        caller, GetModuleHandleW(L"halo3.dll"));
    const uintptr_t mccRva = ProbeModuleRva(
        caller, GetModuleHandleW(nullptr));
    if (halo3Rva != UINTPTR_MAX && desc && initialData &&
        initialData->pSysMem && desc->ByteWidth >= 16u * 256u &&
        (desc->ByteWidth % 16u) == 0)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(initialData->pSysMem);
        const unsigned records = desc->ByteWidth / 16u;
        // Official H3EK resource packing gives these invariant suffixes for
        // Valhalla's first four rock placements. XYZ occupies the six bytes
        // before each suffix and depends on the runtime block bounds. Matching
        // ten fixed bytes avoids confusing ordinary 8-byte mesh vertices with
        // 16-byte decorator placements.
        static constexpr uint8_t kRockSuffixes[][10] = {
            {0x00, 0x02, 0x88, 0x8E, 0xD1, 0xA4, 0xFF, 0xFF, 0xFF, 0x00},
            {0x00, 0x02, 0x83, 0x93, 0xCF, 0xC7, 0xFF, 0xFF, 0xFF, 0x00},
            {0x00, 0x00, 0x80, 0x7F, 0x92, 0x25, 0xFF, 0xFF, 0xFF, 0x00},
            {0x00, 0x02, 0x7E, 0x80, 0xCD, 0xC2, 0xFF, 0xFF, 0xFF, 0x00},
            {0x00, 0x02, 0x09, 0x0F, 0x52, 0x25, 0xFF, 0xFF, 0xFF, 0x00},
        };
        if (g_h3DecoratorDiagnosticEnabled)
        for (unsigned record = 0; record < records; ++record)
        {
            const uint8_t* suffix = bytes + static_cast<size_t>(record) * 16u + 6u;
            unsigned matchedSuffix = 0;
            for (unsigned i = 0; i < _countof(kRockSuffixes); ++i)
            {
                if (std::memcmp(suffix, kRockSuffixes[i], sizeof(kRockSuffixes[i])) == 0)
                {
                    matchedSuffix = i + 1u;
                    break;
                }
            }
            if (!matchedSuffix)
                continue;
            const unsigned sample =
                g_h3DecoratorExactProbeSamples.fetch_add(
                    1, std::memory_order_relaxed) + 1u;
            if (sample <= 32u)
            {
                const unsigned firstRecord = record > 0 ? record - 1u : 0u;
                char around[16u * 4u * 3u + 1u]{};
                FormatProbeRecords(
                    bytes, desc->ByteWidth, firstRecord, around, sizeof(around));
                LOG("H3DECOREXACT[%u]: suffix=%u record=%u/%u bytes=%u "
                    "usage=%u bind=0x%X cpu=0x%X misc=0x%X stride=%u "
                    "halo3Rva=0x%llX mccRva=0x%llX around=%s",
                    sample, matchedSuffix, record, records, desc->ByteWidth,
                    static_cast<unsigned>(desc->Usage), desc->BindFlags,
                    desc->CPUAccessFlags, desc->MiscFlags,
                    desc->StructureByteStride,
                    static_cast<unsigned long long>(halo3Rva),
                    static_cast<unsigned long long>(mccRva), around);
            }
        }

        if ((desc->BindFlags & D3D11_BIND_VERTEX_BUFFER) == 0)
            return H3ProbeCreateBuffer(
                device, desc, initialData, buffer, decoratorPlacement,
                halo3Rva, mccRva);

        const unsigned sampledRecords = std::min(records, 4096u);
        unsigned smallPartIndices = 0;
        unsigned nonzeroPartIndices = 0;
        unsigned nonzeroColors = 0;
        unsigned validScaledQuaternions = 0;
        for (unsigned i = 0; i < sampledRecords; ++i)
        {
            const size_t offset = static_cast<size_t>(i) * 16u;
            const uint8_t part = bytes[offset + 7u];
            if (part < 16u)
                ++smallPartIndices;
            if (part != 0u)
                ++nonzeroPartIndices;
            if (bytes[offset + 12u] != 0u && bytes[offset + 13u] != 0u &&
                bytes[offset + 14u] != 0u)
                ++nonzeroColors;
            constexpr float kQuaternionStep = 1.41421356237f / 127.0f;
            float scale = 0.0f;
            for (unsigned component = 0; component < 4u; ++component)
            {
                const float value =
                    (static_cast<float>(bytes[offset + 8u + component]) -
                     127.0f) * kQuaternionStep;
                scale += value * value;
            }
            if (std::isfinite(scale) && scale >= 0.01f && scale <= 2.10f)
                ++validScaledQuaternions;
        }
        const bool placementLike = sampledRecords >= 256u &&
            smallPartIndices * 100u >= sampledRecords * 99u &&
            nonzeroPartIndices * 100u >= sampledRecords &&
            validScaledQuaternions * 100u >= sampledRecords * 99u &&
            nonzeroColors * 100u >= sampledRecords * 50u;
        if (placementLike)
        {
            decoratorPlacement = true;
            const unsigned sample =
                g_h3DecoratorBufferProbeSamples.fetch_add(
                    1, std::memory_order_relaxed) + 1;
            if (g_h3DecoratorDiagnosticEnabled && sample <= 128u)
            {
                char head[16u * 4u * 3u + 1u]{};
                char stack[512]{};
                FormatProbeRecords(bytes, desc->ByteWidth, 0u, head, sizeof(head));
                FormatProbeStack(stack, sizeof(stack));
                LOG("H3DECORBUF[%u]: bytes=%u records=%u smallPart=%u/%u "
                    "nonzeroPart=%u validQuaternion=%u nonzeroColor=%u "
                    "usage=%u bind=0x%X cpu=0x%X "
                    "misc=0x%X stride=%u halo3Rva=0x%llX mccRva=0x%llX "
                    "stack=[%s] head=%s",
                    sample, desc->ByteWidth, records, smallPartIndices,
                    sampledRecords, nonzeroPartIndices,
                    validScaledQuaternions, nonzeroColors,
                    static_cast<unsigned>(desc->Usage), desc->BindFlags,
                    desc->CPUAccessFlags, desc->MiscFlags, desc->StructureByteStride,
                    static_cast<unsigned long long>(halo3Rva),
                    static_cast<unsigned long long>(mccRva), stack, head);
                LogProbeResourceContext(initialData->pSysMem, records);
                ScanProbeDecoratorBlocks(initialData->pSysMem, records);
            }
        }
    }

    if (g_h3DecoratorDiagnosticEnabled && desc && initialData && initialData->pSysMem &&
        desc->ByteWidth >= 0x3Cu * 4u && (desc->ByteWidth % 0x3Cu) == 0)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(initialData->pSysMem);
        const unsigned blocks = desc->ByteWidth / 0x3Cu;
        const unsigned sampledBlocks = std::min(blocks, 1024u);
        unsigned validBlocks = 0;
        for (unsigned i = 0; i < sampledBlocks; ++i)
        {
            const uint8_t* block = bytes + static_cast<size_t>(i) * 0x3Cu;
            uint16_t count = 0;
            uint32_t start = 0;
            float minimum[3]{};
            float step[3]{};
            std::memcpy(&count, block, sizeof(count));
            std::memcpy(&start, block + 4u, sizeof(start));
            std::memcpy(minimum, block + 8u, sizeof(minimum));
            std::memcpy(step, block + 0x18u, sizeof(step));
            const bool finite =
                std::isfinite(minimum[0]) && std::isfinite(minimum[1]) &&
                std::isfinite(minimum[2]) && std::isfinite(step[0]) &&
                std::isfinite(step[1]) && std::isfinite(step[2]);
            if (count > 0u && count <= 4096u && block[2] < 64u &&
                start < (1u << 24u) && finite &&
                std::fabs(minimum[0]) < 100000.0f &&
                std::fabs(minimum[1]) < 100000.0f &&
                std::fabs(minimum[2]) < 100000.0f &&
                step[0] > 0.0f && step[0] < 1000.0f &&
                step[1] > 0.0f && step[1] < 1000.0f &&
                step[2] > 0.0f && step[2] < 1000.0f)
                ++validBlocks;
        }
        if (sampledBlocks >= 4u &&
            validBlocks * 100u >= sampledBlocks * 90u)
        {
            const unsigned sample =
                g_h3DecoratorBlockProbeSamples.fetch_add(
                    1, std::memory_order_relaxed) + 1u;
            if (sample <= 32u)
            {
                const uint8_t* first = bytes;
                uint16_t count = 0;
                uint32_t start = 0;
                float minimum[3]{};
                float step[3]{};
                std::memcpy(&count, first, sizeof(count));
                std::memcpy(&start, first + 4u, sizeof(start));
                std::memcpy(minimum, first + 8u, sizeof(minimum));
                std::memcpy(step, first + 0x18u, sizeof(step));
                const void* caller = _ReturnAddress();
                const uintptr_t halo3Rva = ProbeModuleRva(
                    caller, GetModuleHandleW(L"halo3.dll"));
                const uintptr_t mccRva = ProbeModuleRva(
                    caller, GetModuleHandleW(nullptr));
                LOG("H3DECORBLOCK[%u]: bytes=%u blocks=%u valid=%u/%u usage=%u "
                    "bind=0x%X cpu=0x%X misc=0x%X stride=%u halo3Rva=0x%llX "
                    "mccRva=0x%llX first=(count=%u set=%u remap=%u start=%u "
                    "min=%.6f,%.6f,%.6f step=%.9f,%.9f,%.9f)",
                    sample, desc->ByteWidth, blocks, validBlocks, sampledBlocks,
                    static_cast<unsigned>(desc->Usage), desc->BindFlags,
                    desc->CPUAccessFlags, desc->MiscFlags,
                    desc->StructureByteStride,
                    static_cast<unsigned long long>(halo3Rva),
                    static_cast<unsigned long long>(mccRva), count,
                    static_cast<unsigned>(first[2]),
                    static_cast<unsigned>(first[3]), start,
                    minimum[0], minimum[1], minimum[2],
                    step[0], step[1], step[2]);
            }
        }
    }
    return H3ProbeCreateBuffer(
        device, desc, initialData, buffer, decoratorPlacement,
        halo3Rva, mccRva);
}

// --- Desktop-window fit (config.fit_desktop_window) -----------------------
// resolution_scale sizes the render the headset captures. On a monitor smaller
// than that render, MCC's window overflows the screen and its menu buttons fall
// off the edge. With the fit on we keep MCC drawing the FULL render (so the
// headset picture and the gun alignment never change) while shrinking only the
// visible window to fit the monitor (menu.cpp); the GPU downscales the full
// backbuffer into the small window on present (flip-model + DXGI_SCALING_STRETCH)
// for free -- no second pass. The hazard is the reverse of the earlier attempt:
// if MCC learns the window shrank it draws a small frame into the corner of the
// big backbuffer (the black-border crop). So we force the backbuffer full at
// creation and keep MCC believing its client is still full-size through its own
// resize handling -- WITHOUT lying to DXGI's present-time client query, which
// must still see the true small window to downscale correctly. Everything below
// is gated on g_config.fit_desktop_window; with it off, none of it is installed.
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwndFn)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
    IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainFn)(IDXGIFactory*, IUnknown*,
    DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
typedef BOOL(WINAPI* GetClientRectFn)(HWND, LPRECT);
typedef BOOL(WINAPI* GetWindowRectFn)(HWND, LPRECT);
static CreateSwapChainForHwndFn g_origCreateSwapChainForHwnd = nullptr;
static CreateSwapChainFn g_origCreateSwapChain = nullptr;
static GetClientRectFn g_origGetClientRect = nullptr;
static GetWindowRectFn g_origGetWindowRect = nullptr;
static UINT g_forcedRenderW = 0;
static UINT g_forcedRenderH = 0;
static bool g_forcedMainSwapchain = false; // only force the game's own (first) swapchain
static HWND g_gameHwnd = nullptr;          // captured at swapchain creation
static bool g_fitActive = false;           // set once at startup: fit on AND its hooks installed
// Set on the game's UI thread ONLY while it synchronously processes a WM_SIZE we
// rewrote to the full render size, so GetClientRectHook feeds MCC's own resize
// code the full size on exactly that call stack -- never on the render thread's
// present-time query, which must keep getting the true small client size.
static thread_local bool g_lieClientToGame = false;

void D3D_GetForcedRenderSize(unsigned& width, unsigned& height)
{
    width = g_forcedRenderW;
    height = g_forcedRenderH;
}

bool D3D_FitActive()
{
    return g_fitActive;
}

void D3D_SetForcedClientLie(bool on)
{
    g_lieClientToGame = on;
}

static void InitForcedRenderSize()
{
    wchar_t buf[32];
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_W", buf, 32) > 0)
        g_forcedRenderW = (UINT)_wtoi(buf);
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_H", buf, 32) > 0)
        g_forcedRenderH = (UINT)_wtoi(buf);
    if (g_forcedRenderW == 0 || g_forcedRenderH == 0)
    {
        // DLL run without the current launcher: replicate the launcher's
        // ScaleEven so the two can never disagree. Config is loaded before this
        // (dllmain InitThread order).
        const float s = g_config.resolution_scale;
        auto even = [](int base, float sc) -> UINT {
            int v = (int)((float)base * sc + 0.5f);
            if (v & 1) ++v;
            return (UINT)v;
        };
        g_forcedRenderW = even(kNativeRenderWidth, s);
        g_forcedRenderH = even(kNativeRenderHeight, s);
    }
    LOG("fit_desktop_window ON: forcing MCC backbuffer to %ux%u (full headset "
        "render); the desktop window is shrunk to fit the monitor separately",
        g_forcedRenderW, g_forcedRenderH);
}

// Game-executable image range, used to scope the client-rect lie and cursor
// remap to game-side callers (never DXGI/DWM or our own overlay). Populated by
// InitExeRange() at hook install; the accessors are defined with the cursor
// hooks below.
static const BYTE* g_exeBase = nullptr;
static const BYTE* g_exeEnd = nullptr;
static inline bool CallerInExe(const void* ret); // defined with the cursor hooks

// Feed the game the full render size for the game window whenever the caller is
// game-side: MCC's resize code (so it keeps drawing full) AND its menu hit-test
// (which clamps the cursor against the client size -- if that returns the true
// small window while our cursor is scaled up to render space, only client/render
// of the menu is reachable, i.e. the top-left ~31% dead-zone). DXGI/DWM query
// the client at present time from system DLLs (not the game EXE), so they still
// get the real small size and keep downscaling the full frame into the window.
static BOOL WINAPI GetClientRectHook(HWND hwnd, LPRECT rc)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetClientRect(hwnd, rc);
    if (ok && rc && hwnd == g_gameHwnd && g_forcedRenderW && g_forcedRenderH &&
        (g_lieClientToGame || CallerInExe(caller)))
    {
        rc->left = 0;
        rc->top = 0;
        rc->right = (LONG)g_forcedRenderW;
        rc->bottom = (LONG)g_forcedRenderH;
        static int s_log = 0;
        static const void* s_lastCaller = nullptr;
        if (!g_lieClientToGame && s_log < 16 && caller != s_lastCaller &&
            g_exeBase)
        {
            ++s_log;
            s_lastCaller = caller;
            LOG("fit: GetClientRect game-side +0x%llX -> forced %ux%u",
                (unsigned long long)((const BYTE*)caller - g_exeBase),
                g_forcedRenderW, g_forcedRenderH);
        }
    }
    return ok;
}

// UI input/focus layers commonly bound their hittable/navigable region to the
// window rect. Ours is the true small window (e.g. 141,0..1139,720), so MCC
// culls every widget below/right of it -> the menu is fully DRAWN but the lower
// items are dead to mouse, keyboard AND d-pad (the reported symptom). For
// game-side callers, report a window rect at the real top-left but sized to the
// full render, so MCC's input region matches its full-size layout and cursor.
// DXGI/DWM/system callers are untouched, so the physical window and the
// present-time downscale are unchanged.
static BOOL WINAPI GetWindowRectHook(HWND hwnd, LPRECT rc)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetWindowRect(hwnd, rc);
    if (ok && rc && hwnd == g_gameHwnd && g_forcedRenderW && g_forcedRenderH &&
        CallerInExe(caller))
    {
        // Keep the true top-left; extend to the full render extent.
        rc->right = rc->left + (LONG)g_forcedRenderW;
        rc->bottom = rc->top + (LONG)g_forcedRenderH;
        static int s_log = 0;
        static const void* s_lastCaller = nullptr;
        if (s_log < 16 && caller != s_lastCaller && g_exeBase)
        {
            ++s_log;
            s_lastCaller = caller;
            LOG("fit: GetWindowRect game-side +0x%llX -> (%ld,%ld,%ld,%ld)",
                (unsigned long long)((const BYTE*)caller - g_exeBase),
                rc->left, rc->top, rc->right, rc->bottom);
        }
    }
    return ok;
}

// --- Fitted-menu cursor coordinate remap -----------------------------------
// With the fit on, MCC DRAWS the full render (e.g. 3204x2310) but the visible
// window is shrunk to the monitor and the GPU downscales it on present. MCC
// lays out and hit-tests its native shell / pause menu in that full render
// space, yet the OS cursor that drives EVERY selection -- the mouse, and the
// gamepad/keyboard "virtual cursor" MCC's console-style shell moves for you --
// is confined to the small physical window. So only the top-left window-sized
// slice of the menu is reachable: the pointer only responds top-left, and
// keyboard/controller focus "moves but stops short" at the window edge. That is
// the exact reported symptom.
//
// Fix: make MCC's OS-cursor coordinate space match the fitted window in BOTH
// directions, and touch only calls that come from the game executable.
//   * GetCursorPos (MCC reads the cursor): scale the physical, window-confined
//     point UP into full render space, so the hit-test lands on the widget
//     directly under the visibly-downscaled cursor.
//   * SetCursorPos (MCC moves the cursor for gamepad/keyboard nav): scale its
//     render-space target back DOWN so the OS cursor stays inside the window
//     and the round-trip through GetCursorPos is exact.
//   * WindowFromPoint MUST keep seeing the TRUE physical point. MCC calls it
//     right after GetCursorPos to confirm the cursor is over its window; if it
//     saw our scaled-up point (which lands outside the small window) it decides
//     the cursor left and drops the input. That silent detail is what broke the
//     earlier broad GetCursorPos rewrite. We undo the remap for exactly the
//     value we last handed out, so no game address needs to be hardcoded.
typedef BOOL(WINAPI* GetCursorPosFn)(LPPOINT);
typedef BOOL(WINAPI* SetCursorPosFn)(int, int);
typedef HWND(WINAPI* WindowFromPointFn)(POINT);
typedef BOOL(WINAPI* ClipCursorFn)(const RECT*);
typedef int(WINAPI* GetSystemMetricsFn)(int);
typedef BOOL(WINAPI* GetMonitorInfoWFn)(HMONITOR, LPMONITORINFO);
static GetCursorPosFn g_origGetCursorPos = nullptr;
static SetCursorPosFn g_origSetCursorPos = nullptr;
static WindowFromPointFn g_origWindowFromPoint = nullptr;
static ClipCursorFn g_origClipCursor = nullptr;
static GetSystemMetricsFn g_origGetSystemMetrics = nullptr;
static GetMonitorInfoWFn g_origGetMonitorInfoW = nullptr;

// The shell/pause cursor consumers live in the game executable (RE-notes /
// RESOLUTION-FSR-INVESTIGATION static analysis). We only remap calls whose
// return address is inside that image, so our own ImGui overlay and any system
// DLL are never touched. (g_exeBase/g_exeEnd are declared above GetClientRectHook.)
static void InitExeRange()
{
    HMODULE h = GetModuleHandleW(nullptr);
    if (!h)
        return;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(h);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const BYTE*>(h) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return;
    g_exeBase = reinterpret_cast<const BYTE*>(h);
    g_exeEnd = g_exeBase + nt->OptionalHeader.SizeOfImage;
}
static inline bool CallerInExe(const void* ret)
{
    return g_exeBase && ret >= static_cast<const void*>(g_exeBase) &&
           ret < static_cast<const void*>(g_exeEnd);
}

// True (unlied) fitted-window client origin (in screen space) and size. Uses
// the ORIGINAL GetClientRect so the WM_SIZE full-size lie can never leak in.
static bool FitClientMetrics(POINT& originScreen, LONG& clientW, LONG& clientH)
{
    if (!g_gameHwnd || !g_origGetClientRect)
        return false;
    RECT rc{};
    if (!g_origGetClientRect(g_gameHwnd, &rc))
        return false;
    clientW = rc.right - rc.left;
    clientH = rc.bottom - rc.top;
    if (clientW <= 0 || clientH <= 0)
        return false;
    originScreen.x = 0;
    originScreen.y = 0;
    return ClientToScreen(g_gameHwnd, &originScreen) != FALSE;
}

static BOOL WINAPI GetCursorPosHook(LPPOINT p)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetCursorPos(p);
    if (!ok || !p || !g_fitActive || !g_forcedRenderW || !g_forcedRenderH ||
        !CallerInExe(caller))
        return ok;
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return ok;
    // Only remap while the cursor is actually over the fitted window.
    if (p->x < origin.x || p->y < origin.y ||
        p->x >= origin.x + cw || p->y >= origin.y + ch)
        return ok;
    const POINT phys = *p;
    POINT mapped;
    mapped.x = origin.x +
               (LONG)llround((double)(phys.x - origin.x) * g_forcedRenderW / cw);
    mapped.y = origin.y +
               (LONG)llround((double)(phys.y - origin.y) * g_forcedRenderH / ch);
    *p = mapped;
    // Log only when the physical cursor actually MOVES (and only once the
    // window is shrunk), so the budget records real navigation instead of 40
    // copies of one idle frame.
    static int s_log = 0;
    static POINT s_lastLogged{LONG_MIN, LONG_MIN};
    if (cw < (LONG)g_forcedRenderW && s_log < 60 &&
        (phys.x != s_lastLogged.x || phys.y != s_lastLogged.y))
    {
        ++s_log;
        s_lastLogged = phys;
        LOG("fit: menu cursor read +0x%llX phys(%ld,%ld) -> render(%ld,%ld) "
            "client %ldx%ld",
            (unsigned long long)((const BYTE*)caller - g_exeBase),
            phys.x, phys.y, mapped.x, mapped.y, cw, ch);
    }
    return ok;
}

static BOOL WINAPI SetCursorPosHook(int X, int Y)
{
    const void* caller = _ReturnAddress();
    if (!g_fitActive || !g_forcedRenderW || !g_forcedRenderH ||
        !CallerInExe(caller))
        return g_origSetCursorPos(X, Y);
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return g_origSetCursorPos(X, Y);
    // MCC targets the cursor in its own (full render) client space. Convert
    // points that fall inside that render rectangle back into the small window
    // so the OS cursor lands where MCC intends; leave anything else untouched.
    const LONG relX = X - origin.x;
    const LONG relY = Y - origin.y;
    if (relX < 0 || relY < 0 ||
        relX > (LONG)g_forcedRenderW || relY > (LONG)g_forcedRenderH)
        return g_origSetCursorPos(X, Y);
    const int px = origin.x + (int)llround((double)relX * cw / g_forcedRenderW);
    const int py = origin.y + (int)llround((double)relY * ch / g_forcedRenderH);
    static int s_log = 0;
    static POINT s_lastLogged{LONG_MIN, LONG_MIN};
    if (cw < (LONG)g_forcedRenderW && s_log < 60 &&
        (X != s_lastLogged.x || Y != s_lastLogged.y))
    {
        ++s_log;
        s_lastLogged.x = X;
        s_lastLogged.y = Y;
        LOG("fit: menu cursor move +0x%llX render(%d,%d) -> phys(%d,%d)",
            (unsigned long long)((const BYTE*)caller - g_exeBase), X, Y, px, py);
    }
    return g_origSetCursorPos(px, py);
}

// If MCC confines the cursor (ClipCursor) to a rectangle expressed in its
// believed full-render space, the OS clips the physical cursor to that (often
// off-screen) rectangle and you can't move the pointer to the lower menu items
// at all. Fold any render-space clip rect back into the real window client so
// the cursor stays free across the whole fitted menu. Fail-open: on anything
// unexpected we pass the request through untouched.
static BOOL WINAPI ClipCursorHook(const RECT* rc)
{
    if (!g_fitActive || !g_forcedRenderW || !g_forcedRenderH || !rc)
        return g_origClipCursor(rc);
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return g_origClipCursor(rc);
    RECT mapped = *rc;
    auto foldX = [&](LONG v) {
        LONG r = v - origin.x;
        if (r < 0) r = 0;
        if (r > (LONG)g_forcedRenderW) r = (LONG)g_forcedRenderW;
        return origin.x + (LONG)llround((double)r * cw / g_forcedRenderW);
    };
    auto foldY = [&](LONG v) {
        LONG r = v - origin.y;
        if (r < 0) r = 0;
        if (r > (LONG)g_forcedRenderH) r = (LONG)g_forcedRenderH;
        return origin.y + (LONG)llround((double)r * ch / g_forcedRenderH);
    };
    mapped.left = foldX(rc->left);
    mapped.right = foldX(rc->right);
    mapped.top = foldY(rc->top);
    mapped.bottom = foldY(rc->bottom);
    static int s_log = 0;
    if (cw < (LONG)g_forcedRenderW && s_log < 12)
    {
        ++s_log;
        LOG("fit: menu cursor clip (%ld,%ld,%ld,%ld) -> (%ld,%ld,%ld,%ld)",
            rc->left, rc->top, rc->right, rc->bottom,
            mapped.left, mapped.top, mapped.right, mapped.bottom);
    }
    return g_origClipCursor(&mapped);
}

static HWND WINAPI WindowFromPointHook(POINT pt)
{
    // MCC hit-tests its menu in full render space, so it asks WindowFromPoint
    // about points spread across the *believed* 3204x2310 client -- most of
    // which fall OFF the real, shrunk window (and often off the monitor). Those
    // resolve to "not my window" and the menu item dies; only points that happen
    // to still land on the small top-left slice work. Map ANY point inside the
    // render rectangle back down into the real window client before the OS
    // answers, so every menu item resolves to the game window like it should.
    if (g_fitActive && g_forcedRenderW && g_forcedRenderH)
    {
        POINT origin{};
        LONG cw = 0, ch = 0;
        if (FitClientMetrics(origin, cw, ch))
        {
            const LONG rx = pt.x - origin.x;
            const LONG ry = pt.y - origin.y;
            if (rx >= 0 && ry >= 0 && rx <= (LONG)g_forcedRenderW &&
                ry <= (LONG)g_forcedRenderH)
            {
                pt.x = origin.x +
                       (LONG)llround((double)rx * cw / g_forcedRenderW);
                pt.y = origin.y +
                       (LONG)llround((double)ry * ch / g_forcedRenderH);
            }
        }
    }
    return g_origWindowFromPoint(pt);
}

// THE root cause of the "menu items are drawn but dead below a certain row"
// symptom (which happens with the fit ON *or* OFF, because it is driven by the
// desktop resolution, not our window): MCC lays out and clips its native shell /
// pause menu against the size it believes the SCREEN is. It asks Windows via
// GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) and GetMonitorInfo, and Windows
// truthfully answers the real desktop size (e.g. 1280x720). But MCC renders the
// menu across the full headset canvas (e.g. 3204x2310), so every widget below
// the real screen height is treated as off-screen and refuses focus -- for the
// mouse, the keyboard, AND the d-pad alike (they all obey this one clip).
//
// Fix: for game-side callers only, report the full render size as the screen /
// monitor size so MCC's layout-vs-viewport clip matches what it actually draws.
// The compositor, DXGI and our own fit math call these from system DLLs / this
// DLL (not the game EXE), so they keep the true desktop size and the display
// downscale is unchanged. Works at ANY desktop resolution or aspect ratio
// because we always hand back the current forced render extent, never a constant.
static int WINAPI GetSystemMetricsHook(int index)
{
    const void* caller = _ReturnAddress();
    const int real = g_origGetSystemMetrics(index);
    if (!g_forcedRenderW || !g_forcedRenderH || !CallerInExe(caller))
        return real;
    // Only the primary-screen extent metrics; every other index passes through.
    int lied = real;
    if (index == SM_CXSCREEN || index == SM_CXFULLSCREEN || index == SM_CXVIRTUALSCREEN)
        lied = (int)g_forcedRenderW;
    else if (index == SM_CYSCREEN || index == SM_CYFULLSCREEN || index == SM_CYVIRTUALSCREEN)
        lied = (int)g_forcedRenderH;
    else
        return real;
    static int s_log = 0;
    static const void* s_lastCaller = nullptr;
    if (s_log < 16 && caller != s_lastCaller && g_exeBase)
    {
        ++s_log;
        s_lastCaller = caller;
        LOG("fit: GetSystemMetrics(%d) game-side +0x%llX -> %d (real %d)",
            index, (unsigned long long)((const BYTE*)caller - g_exeBase), lied, real);
    }
    return lied;
}

static BOOL WINAPI GetMonitorInfoWHook(HMONITOR mon, LPMONITORINFO mi)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetMonitorInfoW(mon, mi);
    if (!ok || !mi || !g_forcedRenderW || !g_forcedRenderH || !CallerInExe(caller))
        return ok;
    // Only rewrite the monitor that hosts the game window; leave others intact so
    // multi-monitor geometry MCC may query for other reasons stays truthful.
    if (g_gameHwnd &&
        MonitorFromWindow(g_gameHwnd, MONITOR_DEFAULTTONEAREST) != mon)
        return ok;
    // Extend both the monitor and work rects from their real top-left to the full
    // render extent, so MCC's fullscreen/menu layout treats the screen as large as
    // what it draws. Keeps the true origin (multi-monitor offsets stay correct).
    mi->rcMonitor.right = mi->rcMonitor.left + (LONG)g_forcedRenderW;
    mi->rcMonitor.bottom = mi->rcMonitor.top + (LONG)g_forcedRenderH;
    mi->rcWork.right = mi->rcWork.left + (LONG)g_forcedRenderW;
    mi->rcWork.bottom = mi->rcWork.top + (LONG)g_forcedRenderH;
    static int s_log = 0;
    static const void* s_lastCaller = nullptr;
    if (s_log < 16 && caller != s_lastCaller && g_exeBase)
    {
        ++s_log;
        s_lastCaller = caller;
        LOG("fit: GetMonitorInfo game-side +0x%llX -> monitor %ldx%ld work %ldx%ld",
            (unsigned long long)((const BYTE*)caller - g_exeBase),
            mi->rcMonitor.right - mi->rcMonitor.left,
            mi->rcMonitor.bottom - mi->rcMonitor.top,
            mi->rcWork.right - mi->rcWork.left,
            mi->rcWork.bottom - mi->rcWork.top);
    }
    return ok;
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndHook(IDXGIFactory2* self, IUnknown* device,
    HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreen, IDXGIOutput* restrictOut,
    IDXGISwapChain1** ppSwapChain)
{
    if (pDesc && g_forcedRenderW && g_forcedRenderH && !g_forcedMainSwapchain)
    {
        g_forcedMainSwapchain = true;
        g_gameHwnd = hwnd;
        DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
        LOG("fit: CreateSwapChainForHwnd MCC requested %ux%u scaling=%d hwnd=%p "
            "-> forcing backbuffer %ux%u STRETCH",
            pDesc->Width, pDesc->Height, (int)pDesc->Scaling, (void*)hwnd,
            g_forcedRenderW, g_forcedRenderH);
        desc.Width = g_forcedRenderW;
        desc.Height = g_forcedRenderH;
        desc.Scaling = DXGI_SCALING_STRETCH;
        const HRESULT hr = g_origCreateSwapChainForHwnd(self, device, hwnd, &desc,
                                                        pFullscreen, restrictOut, ppSwapChain);
        if (FAILED(hr))
            LOG("fit: forced CreateSwapChainForHwnd FAILED (hr=0x%08X); the fit did "
                "NOT apply on this machine", (unsigned)hr);
        return hr;
    }
    return g_origCreateSwapChainForHwnd(self, device, hwnd, pDesc, pFullscreen,
                                        restrictOut, ppSwapChain);
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainHook(IDXGIFactory* self, IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
    if (pDesc && g_forcedRenderW && g_forcedRenderH && !g_forcedMainSwapchain)
    {
        g_forcedMainSwapchain = true;
        g_gameHwnd = pDesc->OutputWindow;
        DXGI_SWAP_CHAIN_DESC desc = *pDesc;
        LOG("fit: CreateSwapChain(legacy) MCC requested %ux%u -> forcing %ux%u",
            pDesc->BufferDesc.Width, pDesc->BufferDesc.Height,
            g_forcedRenderW, g_forcedRenderH);
        desc.BufferDesc.Width = g_forcedRenderW;
        desc.BufferDesc.Height = g_forcedRenderH;
        const HRESULT hr = g_origCreateSwapChain(self, device, &desc, ppSwapChain);
        if (FAILED(hr))
            LOG("fit: forced CreateSwapChain FAILED (hr=0x%08X); the fit did NOT "
                "apply on this machine", (unsigned)hr);
        return hr;
    }
    return g_origCreateSwapChain(self, device, pDesc, ppSwapChain);
}

// Broad probe paths deliberately NOT installed here; each was retired after
// theory. Re-adding any of them costs frame time for information we already
// have:
//   UpdateSubresource/Map/Unmap - constant census sagged fps ~25%; the
//     exact-match matrix matcher scored zero hits in a full session.
//   CopySubresourceRegion and frame-wide CopyResource learning - learned the
//     scene snapshot pairs; per-eye substitution of both sides left the ghost
//     unchanged. The learning also allocated a full-resolution shadow texture
//     per eye per pair (~25 MB each) and re-copied them every eye pass.
// CopyResource itself is now intercepted only for ODST's exact native-CHUD
// phase scope. It performs one retained-pointer comparison and may substitute
// only the source with the already-owned eye cache; it does no discovery.
//   PSSetShaderResources - cross-pass history discovery promoted 0 targets in
//     two sessions.
//   OMSetRenderTargetsAndUnorderedAccessViews - frame-level RTV discovery
//     promoted 0 targets.
//   PSSetShader/VSSetShader/Draw* - the CHUD steal-and-requad classifier
//     (2026-07-18): removed the native HUD from both eyes, never displayed
//     its hand quad, and its calibration retry loop cost ~30 fps.
// OMSetRenderTargets makes the two eye renders land in separate textures.

static void STDMETHODCALLTYPE OMSetRenderTargetsHook(ID3D11DeviceContext* context, UINT count,
    ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv)
{
    ID3D11RenderTargetView* redirected[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    if (count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT &&
        VR_RedirectRenderTargets(context, count, rtvs, redirected))
    {
        g_origOMSetRenderTargets(context, count, redirected, dsv);
        return;
    }
    g_origOMSetRenderTargets(context, count, rtvs, dsv);
}

#if HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
// Diagnostic only, and deliberately far more conservative than the retired
// RSSetViewports hook: this file's own history (see the OMSetRenderTargetsHook
// comment above) records that a prior PS/VS/Draw*-hooking attempt cost ~30 fps.
// This hook does the absolute minimum on every call - one relaxed atomic
// load and compare, nothing else - unless it is one of a HARD-CAPPED total
// number of samples for the whole process lifetime (not per-second: DrawIndexed
// can be called thousands of times a frame, so any per-frame or per-second
// throttle can still mean thousands of wasted checks). Once the budget is
// spent the hook is a single branch forever, so it cannot degrade a long
// session even if this candidate stays enabled by accident.
//
// Purpose: every static and memory-inspection method available tonight
// converged on the same result - the chud_globals curvature-info safe frame
// this mod writes is correct, stable, and read by nothing in the process
// (docs/RE-notes.md, the Reach HUD layout entry). If Reach's HUD is drawn as
// screen-space quads (proven: RSSetViewports never shows a HUD-sized rect),
// the quad's actual screen extent has to come from SOMEWHERE - vertex data,
// a constant buffer, or a shader immediate. Sampling the bound viewport,
// vertex buffer stride, and index/vertex counts at a real HUD-sized draw call
// is the most direct way left to find that path without disassembling more
// of a stripped optimized binary blind.
static std::atomic<int> g_reachDrawSamplesTaken{0};
constexpr int kReachDrawSampleBudget = 200;

// The first 64-sample pass (2026-07-26) found two distinct full-viewport
// index-count-6 shapes: stride 40 and stride 24, plus an unrelated stride-32
// mip/blur chain that shrinks through power-of-two sizes (not sampled here).
// The stride-40 shape was sampled next and its vertices proved to be an exact
// (0,0)-(1,1) UV quad spanning the FULL screen corner to corner in every
// sample - a full-screen post-process/compositor pass, not the HUD. This pass
// switches to the stride-24 shape, the other untried candidate from the same
// original sweep, and reads its real vertex bytes the same way.
static bool IsProvenHudQuadShape(
    UINT indexCount, UINT stride, const D3D11_VIEWPORT& vp, float backbufferW)
{
    return indexCount == 6 && stride == 24 &&
        vp.Width >= backbufferW - 1.0f;
}

static void DumpVertexBufferSample(
    ID3D11DeviceContext* context, ID3D11Buffer* vb, UINT stride, UINT sampleNo)
{
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    if (!device)
        return;
    D3D11_BUFFER_DESC srcDesc{};
    vb->GetDesc(&srcDesc);
    D3D11_BUFFER_DESC stagingDesc = srcDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    ID3D11Buffer* staging = nullptr;
    const HRESULT hr = device->CreateBuffer(&stagingDesc, nullptr, &staging);
    device->Release();
    if (FAILED(hr) || !staging)
        return;
    context->CopyResource(staging, vb);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context->Map(
            staging, 0, D3D11_MAP_READ, 0, &mapped)))
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(mapped.pData);
        // Log up to 4 vertices worth (our proven shape uses 4 unique
        // vertices per quad, 3 quads); stride 40 = 10 floats/vertex.
        const UINT vertsToShow = std::min(
            4u, srcDesc.ByteWidth / std::max(stride, 1u));
        for (UINT v = 0; v < vertsToShow; ++v)
        {
            const float* f = reinterpret_cast<const float*>(
                bytes + static_cast<size_t>(v) * stride);
            const UINT floatsToShow = std::min(stride / 4u, 10u);
            char line[256];
            int n = _snprintf_s(line, sizeof(line), _TRUNCATE,
                "REACHVTX[sample %u]: vertex %u:", sampleNo, v);
            for (UINT fi = 0; fi < floatsToShow && n > 0 &&
                 static_cast<size_t>(n) < sizeof(line); ++fi)
            {
                char piece[24];
                _snprintf_s(piece, sizeof(piece), _TRUNCATE,
                    " %.4f", f[fi]);
                strcat_s(line, sizeof(line), piece);
            }
            LOG("%s", line);
        }
        context->Unmap(staging, 0);
    }
    staging->Release();
}

static void LogReachHudDrawIfSmall(ID3D11DeviceContext* context,
    UINT indexCount, UINT startIndex, INT baseVertex)
{
    if (g_reachDrawSamplesTaken.load(std::memory_order_relaxed) >=
        kReachDrawSampleBudget)
        return;
    if (TitleAdapter_GetActiveTitle() != GameTitle::HaloReach)
        return;
    if (indexCount == 0 || indexCount > 64)
        return;

    UINT vpCount = 1;
    D3D11_VIEWPORT vp{};
    context->RSGetViewports(&vpCount, &vp);

    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    context->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    const bool hadVb = vb != nullptr;

    unsigned forcedBackbufferW = 0, forcedBackbufferH = 0;
    D3D_GetForcedRenderSize(forcedBackbufferW, forcedBackbufferH);
    const float backbufferW = forcedBackbufferW
        ? static_cast<float>(forcedBackbufferW) : 3786.0f;
    const bool provenShape =
        hadVb && IsProvenHudQuadShape(indexCount, stride, vp, backbufferW);

    if (!provenShape)
    {
        if (vb)
            vb->Release();
        return;
    }

    const int sampleNo =
        g_reachDrawSamplesTaken.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sampleNo > kReachDrawSampleBudget)
    {
        vb->Release();
        return;
    }
    LOG("REACHDRAW: DrawIndexed count=%u startIndex=%u baseVertex=%d "
        "vp=(%.1f,%.1f %.1fx%.1f) vbStride=%u [sample %d/%d]",
        indexCount, startIndex, baseVertex, vp.TopLeftX, vp.TopLeftY,
        vp.Width, vp.Height, stride, sampleNo, kReachDrawSampleBudget);
    DumpVertexBufferSample(
        context, vb, stride, static_cast<UINT>(sampleNo));
    vb->Release();
}

static void STDMETHODCALLTYPE DrawIndexedHook(ID3D11DeviceContext* context,
    UINT indexCount, UINT startIndex, INT baseVertex)
{
    LogReachHudDrawIfSmall(context, indexCount, startIndex, baseVertex);
    g_origDrawIndexed(context, indexCount, startIndex, baseVertex);
}
#endif

#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
static void STDMETHODCALLTYPE CopyResourceHook(ID3D11DeviceContext* context,
    ID3D11Resource* destination, ID3D11Resource* source)
{
    g_origCopyResource(context, destination,
                       VR_RedirectNativeHudCopySource(source));
}
#endif

#if HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
// RETIRED DIAGNOSTIC (2026-07-26): a live session proved every RSSetViewports
// call in a Reach frame is a world/shadow-cascade/mip-chain size (observed
// 3786x2730 down through 2x1 in one frame, never a small HUD-shaped rect).
// Reach's CHUD does not resize a distinct D3D11 viewport at all - it draws
// screen-space geometry inside the single full-frame viewport, positioned by
// vertex coordinates. This hook answered its question and is retired rather
// than left installed: with dozens of genuinely distinct viewports per frame,
// its "distinct shape" dedup could not throttle a hot path (proven: ~500k
// lines in one short session) and volume like that risks burying real
// signal. See docs/RE-notes.md, the Reach HUD layout entry, for what this
// ruled out and what is tried next.
#endif

// Log-only: record what MCC's swapchain actually is, plus how its backbuffer
// compares to the visible window and the monitor. On a monitor smaller than the
// requested render this shows whether MCC drew the full frame (backbuffer size)
// or a small one, the swap effect + scaling (whether an oversized backbuffer
// STRETCHes or CLIPs on present), and the real window client vs the monitor.
// That is exactly what tells us whether the fit is working. Fires once, then
// only when the backbuffer size changes, so it never spams the hot path.
static void LogSwapchainConfigOnce(IDXGISwapChain* sc)
{
    static UINT s_lastW = 0, s_lastH = 0;
    DXGI_SWAP_CHAIN_DESC d{};
    if (FAILED(sc->GetDesc(&d)))
        return;
    if (d.BufferDesc.Width == s_lastW && d.BufferDesc.Height == s_lastH)
        return;
    s_lastW = d.BufferDesc.Width;
    s_lastH = d.BufferDesc.Height;

    const char* swapEffect = "UNKNOWN";
    switch (d.SwapEffect)
    {
        case DXGI_SWAP_EFFECT_DISCARD: swapEffect = "DISCARD"; break;
        case DXGI_SWAP_EFFECT_SEQUENTIAL: swapEffect = "SEQUENTIAL"; break;
        case DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL: swapEffect = "FLIP_SEQUENTIAL"; break;
        case DXGI_SWAP_EFFECT_FLIP_DISCARD: swapEffect = "FLIP_DISCARD"; break;
        default: break;
    }

    const char* scaling = "n/a";
    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)))
    {
        DXGI_SWAP_CHAIN_DESC1 d1{};
        if (SUCCEEDED(sc1->GetDesc1(&d1)))
        {
            switch (d1.Scaling)
            {
                case DXGI_SCALING_STRETCH: scaling = "STRETCH"; break;
                case DXGI_SCALING_NONE: scaling = "NONE"; break;
                case DXGI_SCALING_ASPECT_RATIO_STRETCH: scaling = "ASPECT_RATIO_STRETCH"; break;
                default: break;
            }
        }
        sc1->Release();
    }

    int clientW = 0, clientH = 0;
    HWND hwnd = d.OutputWindow;
    RECT client{};
    if (hwnd && GetClientRect(hwnd, &client))
    {
        clientW = client.right - client.left;
        clientH = client.bottom - client.top;
    }
    int monW = 0, monH = 0, workW = 0, workH = 0;
    if (hwnd)
    {
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{sizeof(mi)};
        if (GetMonitorInfo(mon, &mi))
        {
            monW = mi.rcMonitor.right - mi.rcMonitor.left;
            monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
            workW = mi.rcWork.right - mi.rcWork.left;
            workH = mi.rcWork.bottom - mi.rcWork.top;
        }
    }

    LOG("swapchain: backbuffer %ux%u fmt=%d swapEffect=%s scaling=%s bufferCount=%u "
        "windowed=%d | window client %dx%d | monitor %dx%d work %dx%d | fit=%d",
        d.BufferDesc.Width, d.BufferDesc.Height, (int)d.BufferDesc.Format,
        swapEffect, scaling, d.BufferCount, d.Windowed ? 1 : 0,
        clientW, clientH, monW, monH, workW, workH,
        g_config.fit_desktop_window ? 1 : 0);
}

// Present1 can forward to Present internally; this depth counter makes sure
// we only run the VR frame once per game frame.
static thread_local int g_presentDepth = 0;

// --- Co-op drop probe -------------------------------------------------------
// Records, per frame, how long THIS MOD holds MCC's render thread inside
// Present. See coop_probe_logic.h for why that thread matters to co-op. The
// ring is fixed and written only from the game's render thread (Present has one
// caller), so the hot path stays allocation-free and lock-free: fill the slot,
// then publish the count with a release store.
// The investigation is complete; preserve the probe for a targeted future run
// while compiling it out of the normal Present path.
constexpr bool kEnableCoopPresentProbe = false;
namespace
{
    // ~34 s at 120 Hz, ~57 s at 72 Hz -- long enough to hold the whole run-up to
    // a kick at any headset rate we support.
    constexpr size_t kCoopProbeSamples = 4096;
    constexpr size_t kCoopProbeSeconds = 64;

    CoopProbeSample g_coopSamples[kCoopProbeSamples]{};
    std::atomic<uint64_t> g_coopWritten{ 0 };
    int64_t g_coopPrevHookStartQpc = 0;

    double CoopProbeQpcToMs()
    {
        static double s_toMs = [] {
            LARGE_INTEGER f{};
            QueryPerformanceFrequency(&f);
            return f.QuadPart ? 1000.0 / static_cast<double>(f.QuadPart) : 0.0;
        }();
        return s_toMs;
    }

    void CoopProbeNoteFrame(int64_t hookStartQpc, int64_t presentStartQpc,
                            int64_t presentEndQpc)
    {
        if constexpr (!kEnableCoopPresentProbe)
            return;
        if (!g_config.coop_probe)
            return;
        const double toMs = CoopProbeQpcToMs();
        if (toMs == 0.0)
            return;

        LARGE_INTEGER hookEnd{};
        QueryPerformanceCounter(&hookEnd);

        const double hookMs =
            static_cast<double>(hookEnd.QuadPart - hookStartQpc) * toMs;
        const double dxgiMs =
            static_cast<double>(presentEndQpc - presentStartQpc) * toMs;
        // What we ADD is the hook minus the game's own present. Clamp at zero:
        // the two timestamps are taken at slightly different points and a
        // negative "hold" would only ever be measurement noise.
        const double holdMs = hookMs > dxgiMs ? hookMs - dxgiMs : 0.0;
        const double intervalMs = g_coopPrevHookStartQpc
            ? static_cast<double>(hookStartQpc - g_coopPrevHookStartQpc) * toMs
            : 0.0;
        g_coopPrevHookStartQpc = hookStartQpc;

        const uint64_t written = g_coopWritten.load(std::memory_order_relaxed);
        CoopProbeSample& slot = g_coopSamples[written % kCoopProbeSamples];
        slot.tickMs = static_cast<uint32_t>(GetTickCount64());
        slot.holdMs = static_cast<float>(holdMs);
        slot.dxgiMs = static_cast<float>(dxgiMs);
        slot.intervalMs = static_cast<float>(intervalMs);
        g_coopWritten.store(written + 1, std::memory_order_release);
    }
}

void CoopProbe_DumpRunUp(const char* reason)
{
    if constexpr (!kEnableCoopPresentProbe)
        return;
    if (!g_config.coop_probe)
        return;
    const uint64_t written = g_coopWritten.load(std::memory_order_acquire);
    if (!written)
        return;

    // Dump path only -- never the hot path -- so these statics cost nothing per
    // frame and keep the probe allocation-free here too.
    static CoopProbeSample s_ordered[kCoopProbeSamples];
    static CoopProbeBucket s_buckets[kCoopProbeSeconds];

    const size_t count = static_cast<size_t>(
        written < kCoopProbeSamples ? written : kCoopProbeSamples);
    const size_t first =
        static_cast<size_t>((written - count) % kCoopProbeSamples);
    for (size_t i = 0; i < count; ++i)
        s_ordered[i] = g_coopSamples[(first + i) % kCoopProbeSamples];

    const uint32_t nowTick = static_cast<uint32_t>(GetTickCount64());
    const size_t bucketCount = CoopProbeSummarise(
        s_ordered, count, nowTick, s_buckets, kCoopProbeSeconds);

    LOG("COOPPROBE run-up to teardown from %s: %zu frames across %zu s. "
        "'hold' is the time THIS MOD adds inside MCC's Present, on MCC's own "
        "render thread -- the thread its co-op simulation is paced from. "
        "Rising hold or interval before a kick implicates the mod; flat means "
        "look elsewhere.",
        reason ? reason : "?", count, bucketCount);
    for (size_t age = bucketCount; age-- > 0;)
    {
        const CoopProbeBucket& b = s_buckets[age];
        if (!b.frames)
            continue;
        LOG("COOPPROBE t-%02zus: frames=%u hold avg=%.3f max=%.3f over4ms=%u "
            "over8ms=%u | dxgi avg=%.3f max=%.3f | interval avg=%.3f max=%.3f",
            age, b.frames, b.holdAvgMs, b.holdMaxMs, b.holdOver4Ms,
            b.holdOver8Ms, b.dxgiAvgMs, b.dxgiMaxMs, b.intervalAvgMs,
            b.intervalMaxMs);
    }
}

// The VR frame is submitted INSIDE the game's desktop present (below), so
// whatever throttles that present throttles the headset with it. MCC's V-Sync
// paces on the DESKTOP monitor's refresh, which has nothing to do with the
// headset's -- that is how a 60 Hz desktop ends up capping a 120 Hz headset.
// Once xrWaitFrame is driving our cadence, present the desktop mirror unlocked
// and let the runtime's reported display period be the only clock. No rate is
// assumed here: 72, 90, 120 and 144 Hz headsets all pace themselves.
static UINT PacedSyncInterval(UINT requested)
{
    if (!g_config.desktop_present_unlocked || !VR_IsFramePacingOwned())
        return requested;
    static UINT s_loggedRequested = UINT_MAX;
    if (requested != s_loggedRequested)
    {
        s_loggedRequested = requested;
        if (requested == 0)
            LOG("pacing: MCC already presents unlocked (syncInterval=0); the "
                "runtime app cadence is %.1fHz", VR_HeadsetRefreshHz());
        else
            LOG("pacing: MCC asked for syncInterval=%u (its V-Sync would cap the "
                "headset at the DESKTOP refresh); presenting unlocked so the "
                "runtime app cadence remains %.1fHz",
                requested, VR_HeadsetRefreshHz());
    }
    return 0;
}

static HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* sc, UINT syncInterval, UINT flags)
{
    const bool topLevel = (g_presentDepth++ == 0);
    const bool runVrFrame = topLevel && !(flags & DXGI_PRESENT_TEST);
    LARGE_INTEGER hookStart{};
    if (runVrFrame)
    {
        H3DecoratorPublishFrame();
        if constexpr (kEnableCoopPresentProbe)
            QueryPerformanceCounter(&hookStart);
        LogSwapchainConfigOnce(sc);
        VR_BeforePresent(sc);
    }
    const UINT pacedSyncInterval = runVrFrame
        ? PacedSyncInterval(syncInterval) : syncInterval;
    LARGE_INTEGER presentStart{}, presentEnd{};
    if (runVrFrame)
        QueryPerformanceCounter(&presentStart);
    HRESULT hr = g_origPresent(sc, pacedSyncInterval, flags);
    if (runVrFrame)
    {
        QueryPerformanceCounter(&presentEnd);
        VR_AfterPresent(sc, presentStart.QuadPart, presentEnd.QuadPart, hr);
        if constexpr (kEnableCoopPresentProbe)
            CoopProbeNoteFrame(hookStart.QuadPart, presentStart.QuadPart,
                               presentEnd.QuadPart);
    }
    g_presentDepth--;
    return hr;
}

static HRESULT STDMETHODCALLTYPE Present1Hook(IDXGISwapChain1* sc, UINT syncInterval, UINT flags,
                                              const DXGI_PRESENT_PARAMETERS* params)
{
    const bool topLevel = (g_presentDepth++ == 0);
    const bool runVrFrame = topLevel && !(flags & DXGI_PRESENT_TEST);
    LARGE_INTEGER hookStart{};
    if (runVrFrame)
    {
        H3DecoratorPublishFrame();
        if constexpr (kEnableCoopPresentProbe)
            QueryPerformanceCounter(&hookStart);
        LogSwapchainConfigOnce(sc);
        VR_BeforePresent(sc);
    }
    const UINT pacedSyncInterval = runVrFrame
        ? PacedSyncInterval(syncInterval) : syncInterval;
    LARGE_INTEGER presentStart{}, presentEnd{};
    if (runVrFrame)
        QueryPerformanceCounter(&presentStart);
    HRESULT hr = g_origPresent1(sc, pacedSyncInterval, flags, params);
    if (runVrFrame)
    {
        QueryPerformanceCounter(&presentEnd);
        VR_AfterPresent(sc, presentStart.QuadPart, presentEnd.QuadPart, hr);
        if constexpr (kEnableCoopPresentProbe)
            CoopProbeNoteFrame(hookStart.QuadPart, presentStart.QuadPart,
                               presentEnd.QuadPart);
    }
    g_presentDepth--;
    return hr;
}

static HRESULT STDMETHODCALLTYPE ResizeBuffersHook(IDXGISwapChain* sc, UINT bufferCount, UINT width,
                                                   UINT height, DXGI_FORMAT format, UINT flags)
{
    // With the fit on, keep the backbuffer pinned to the full launched render
    // size so a later resize (e.g. triggered when we shrink the visible window to
    // fit the monitor) can't clamp the surface the headset captures back down.
    // With the fit off, g_forcedRenderW/H are 0 and this passes the size through
    // unchanged -- exactly the previous behavior.
    UINT fw = width, fh = height;
    if (g_forcedRenderW && g_forcedRenderH)
    {
        fw = g_forcedRenderW;
        fh = g_forcedRenderH;
    }
    LOG("game resized its swapchain to %ux%u (using %ux%u)", width, height, fw, fh);
    VR_OnResizeBuffers(sc); // we must drop any references to the old backbuffer first
    const HRESULT result =
        g_origResizeBuffers(sc, bufferCount, fw, fh, format, flags);
    VR_AfterResizeBuffers(sc);
    return result;
}

bool InstallD3D11Hooks()
{
    // Config is loaded before this runs. Only when the desktop-window fit is on
    // do we compute the forced render size and install the swapchain-creation /
    // GetClientRect hooks below; with it off, none of that exists and the render
    // path is byte-for-byte the previous behavior.
    const bool fit = g_config.fit_desktop_window;
    if (fit)
        InitForcedRenderSize();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"halo3xr_dummy_window";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED,
                                0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
    {
        LOG("dummy window creation failed (%lu)", GetLastError());
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 64;
    sd.BufferDesc.Height = 64;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    if (FAILED(hr))
    {
        LOG("dummy D3D11 device creation failed (hr=0x%08X)", (unsigned)hr);
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtbl = *(void***)sc;
    void** deviceVtbl = *(void***)dev;
    void** contextVtbl = *(void***)ctx;
    bool ok = MH_CreateHook(vtbl[8], (void*)&PresentHook, (void**)&g_origPresent) == MH_OK &&
              MH_CreateHook(vtbl[13], (void*)&ResizeBuffersHook, (void**)&g_origResizeBuffers) == MH_OK &&
              MH_CreateHook(contextVtbl[33], (void*)&OMSetRenderTargetsHook,
                            (void**)&g_origOMSetRenderTargets) == MH_OK;
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
    ok = ok && MH_CreateHook(contextVtbl[47], (void*)&CopyResourceHook,
                             (void**)&g_origCopyResource) == MH_OK;
#endif
#if HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
    // ID3D11DeviceContext::DrawIndexed is vtable slot 12. The old HUD probe is
    // preserved above as evidence, but it must not intercept production draws.
    // This optional hook was never part of `ok`, so disabling it cannot gate
    // Present, render-target redirection, or any title camera core.
    if constexpr (kEnableReachDrawIndexedDiagnostic)
    {
        if (MH_CreateHook(contextVtbl[12], (void*)&DrawIndexedHook,
                          (void**)&g_origDrawIndexed) != MH_OK)
            LOG("REACHDRAW: DrawIndexed hook failed; draw sampling disabled");
    }
    else
    {
        LOG("REACHDRAW: DrawIndexed diagnostic hook intentionally disabled; "
            "no draw calls are intercepted");
    }
#endif

    wchar_t decoratorProbeValue[2]{};
    g_h3DecoratorDiagnosticEnabled = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_DECORATOR_BUFFER_PROBE",
        decoratorProbeValue, 2) > 0;
    wchar_t decoratorSelfTestValue[2]{};
    g_h3DecoratorSelfTestEnabled = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_DECORATOR_SELF_TEST",
        decoratorSelfTestValue, 2) > 0;
    const bool decoratorCaptureRequested =
        g_config.physical_weapon_contact || g_h3DecoratorDiagnosticEnabled;
    if (decoratorCaptureRequested)
    {
        const bool createBufferOk =
            MH_CreateHook(deviceVtbl[3], (void*)&CreateBufferHook,
                          (void**)&g_origCreateBuffer) == MH_OK;
        const bool vertexConstantBindingOk = createBufferOk &&
            MH_CreateHook(contextVtbl[7],
                          (void*)&H3ProbeVSSetConstantBuffersHook,
                          (void**)&g_origH3ProbeVSSetConstantBuffers) == MH_OK;
        const bool mapBindingOk = vertexConstantBindingOk &&
            MH_CreateHook(contextVtbl[14], (void*)&H3ProbeMapHook,
                          (void**)&g_origH3ProbeMap) == MH_OK;
        const bool unmapBindingOk = mapBindingOk &&
            MH_CreateHook(contextVtbl[15], (void*)&H3ProbeUnmapHook,
                          (void**)&g_origH3ProbeUnmap) == MH_OK;
        const bool updateBindingOk = unmapBindingOk &&
            MH_CreateHook(contextVtbl[48],
                          (void*)&H3ProbeUpdateSubresourceHook,
                          (void**)&g_origH3ProbeUpdateSubresource) == MH_OK;
        const bool vertexBindingOk = updateBindingOk &&
            MH_CreateHook(contextVtbl[18],
                          (void*)&H3ProbeIASetVertexBuffersHook,
                          (void**)&g_origIASetVertexBuffers) == MH_OK;
        const bool topologyBindingOk = vertexBindingOk &&
            MH_CreateHook(contextVtbl[24],
                          (void*)&H3ProbeIASetPrimitiveTopologyHook,
                          (void**)&g_origIASetPrimitiveTopology) == MH_OK;
        const bool instancedOk = topologyBindingOk &&
            MH_CreateHook(contextVtbl[21],
                          (void*)&H3ProbeDrawInstancedHook,
                          (void**)&g_origH3ProbeDrawInstanced) == MH_OK;
        if (instancedOk)
        {
            H3DecoratorFrame& first =
                g_h3DecoratorFrames[g_h3DecoratorWriteFrame];
            first.version.store(1u, std::memory_order_release);
            first.drawCount = 0;
            g_h3DecoratorContactCaptureEnabled =
                g_config.physical_weapon_contact;
            LOG("H3 decorator contact capture installed: contact=%u diagnostic=%u",
                g_h3DecoratorContactCaptureEnabled ? 1u : 0u,
                g_h3DecoratorDiagnosticEnabled ? 1u : 0u);
        }
        else
        {
            g_h3DecoratorContactCaptureEnabled = false;
            LOG("H3 decorator contact capture binding failed; physical contact "
                "continues without decorator walls");
        }

        if (g_h3DecoratorDiagnosticEnabled && instancedOk)
        {
            const bool createInputLayoutOk =
                MH_CreateHook(deviceVtbl[11],
                              (void*)&H3ProbeCreateInputLayoutHook,
                              (void**)&g_origCreateInputLayout) == MH_OK;
            const bool inputLayoutOk = createInputLayoutOk &&
                MH_CreateHook(contextVtbl[17],
                              (void*)&H3ProbeIASetInputLayoutHook,
                              (void**)&g_origIASetInputLayout) == MH_OK;
            const bool indexOk = inputLayoutOk &&
                MH_CreateHook(contextVtbl[19],
                              (void*)&H3ProbeIASetIndexBufferHook,
                              (void**)&g_origIASetIndexBuffer) == MH_OK;
            const bool drawIndexedOk = indexOk &&
                MH_CreateHook(contextVtbl[12],
                              (void*)&H3ProbeDrawIndexedHook,
                              (void**)&g_origH3ProbeDrawIndexed) == MH_OK;
            const bool drawOk = drawIndexedOk &&
                MH_CreateHook(contextVtbl[13],
                              (void*)&H3ProbeDrawHook,
                              (void**)&g_origH3ProbeDraw) == MH_OK;
            const bool indexedInstancedOk = drawOk &&
                MH_CreateHook(contextVtbl[20],
                              (void*)&H3ProbeDrawIndexedInstancedHook,
                              (void**)&g_origDrawIndexedInstanced) == MH_OK;
            const bool indexedIndirectOk = indexedInstancedOk &&
                MH_CreateHook(contextVtbl[39],
                              (void*)&H3ProbeDrawIndexedInstancedIndirectHook,
                              (void**)&g_origH3ProbeDrawIndexedInstancedIndirect) == MH_OK;
            const bool diagnosticOk = indexedIndirectOk &&
                MH_CreateHook(contextVtbl[40],
                              (void*)&H3ProbeDrawInstancedIndirectHook,
                              (void**)&g_origH3ProbeDrawInstancedIndirect) == MH_OK;
            if (diagnosticOk)
            {
                g_h3DecoratorDrawProbeEnabled.store(
                    true, std::memory_order_release);
                HANDLE thread = CreateThread(
                    nullptr, 0, &H3ProbeDrawLoggerThread, nullptr, 0, nullptr);
                if (thread)
                {
                    CloseHandle(thread);
                    LOG("H3DECORDRAW: diagnostic tracer installed");
                }
                else
                    g_h3DecoratorDrawProbeEnabled.store(
                        false, std::memory_order_release);
            }
            else
                LOG("H3DECORDRAW: optional diagnostic binding failed");
        }
    }

    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)))
    {
        void** vtbl1 = *(void***)sc1;
        if (MH_CreateHook(vtbl1[22], (void*)&Present1Hook, (void**)&g_origPresent1) != MH_OK)
            LOG("warning: Present1 hook failed; flip-model swapchains may not be captured");
        sc1->Release();
    }

    if (fit)
    {
        bool forceHookOk = false;
        bool clientRectHookOk = false;

        // Hook the DXGI factory's swapchain-creation entry points so we can force
        // MCC's backbuffer to the full launched render size. All factories in the
        // process share the same vtable implementation, so hooking the dummy
        // device's factory hooks MCC's real creation call -- exactly how the
        // Present/ResizeBuffers vtables above are taken.
        // CreateSwapChain=vtbl[10], CreateSwapChainForHwnd=vtbl[15] (IDXGIFactory2).
        IDXGIDevice* dxgiDev = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory2 = nullptr;
        if (SUCCEEDED(dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev)) &&
            SUCCEEDED(dxgiDev->GetAdapter(&adapter)) &&
            SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory2)))
        {
            void** facVtbl = *(void***)factory2;
            if (MH_CreateHook(facVtbl[15], (void*)&CreateSwapChainForHwndHook,
                              (void**)&g_origCreateSwapChainForHwnd) == MH_OK)
                forceHookOk = true;
            else
                LOG("warning: CreateSwapChainForHwnd hook failed; desktop fit inactive");
            if (MH_CreateHook(facVtbl[10], (void*)&CreateSwapChainHook,
                              (void**)&g_origCreateSwapChain) != MH_OK)
                LOG("warning: CreateSwapChain hook failed; legacy desktop fit inactive");
        }
        else
        {
            LOG("warning: could not reach the DXGI factory; desktop fit inactive");
        }
        if (factory2) factory2->Release();
        if (adapter) adapter->Release();
        if (dxgiDev) dxgiDev->Release();

        // Hook user32!GetClientRect so MCC's own resize code, while it handles the
        // WM_SIZE we rewrite, sees the full render size and keeps drawing full.
        // The hook only lies on that thread-local call stack (see the header note),
        // so DXGI's present-time query still downscales the full frame into the
        // real, smaller window.
        if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
        {
            // Cache the game-EXE image range first: GetClientRect/GetWindowRect
            // and the cursor hooks all scope their game-side behavior on it.
            InitExeRange();

            if (void* pGetClientRect = (void*)GetProcAddress(user32, "GetClientRect"))
            {
                if (MH_CreateHook(pGetClientRect, (void*)&GetClientRectHook,
                                  (void**)&g_origGetClientRect) == MH_OK)
                    clientRectHookOk = true;
                else
                    LOG("warning: GetClientRect hook failed; the fit may crop on "
                        "resize-polling titles");
            }

            // GetWindowRect lie (game-side only): make MCC's input/focus region
            // match its full-size layout so the fitted menu's lower items stay
            // navigable. Independent of the render force; warn-only on failure.
            if (void* pGetWindowRect = (void*)GetProcAddress(user32, "GetWindowRect"))
            {
                if (MH_CreateHook(pGetWindowRect, (void*)&GetWindowRectHook,
                                  (void**)&g_origGetWindowRect) != MH_OK)
                    LOG("warning: GetWindowRect hook failed; fitted-menu lower "
                        "items may stay unreachable if MCC bounds input to the "
                        "window rect");
            }

            // Cursor coordinate remap so MCC's native shell / pause menu is
            // navigable in the fitted window (see the block above the hooks).
            // These are supplementary: if any fails, the display fit still works,
            // the menu is just no more navigable than before -- so they do NOT
            // gate g_fitActive. (g_exeBase is already cached above.)
            void* pGetCursorPos = (void*)GetProcAddress(user32, "GetCursorPos");
            void* pSetCursorPos = (void*)GetProcAddress(user32, "SetCursorPos");
            void* pWindowFromPoint = (void*)GetProcAddress(user32, "WindowFromPoint");
            const bool cursorHooksOk =
                g_exeBase &&
                pGetCursorPos &&
                MH_CreateHook(pGetCursorPos, (void*)&GetCursorPosHook,
                              (void**)&g_origGetCursorPos) == MH_OK &&
                pSetCursorPos &&
                MH_CreateHook(pSetCursorPos, (void*)&SetCursorPosHook,
                              (void**)&g_origSetCursorPos) == MH_OK &&
                pWindowFromPoint &&
                MH_CreateHook(pWindowFromPoint, (void*)&WindowFromPointHook,
                              (void**)&g_origWindowFromPoint) == MH_OK;
            if (!cursorHooksOk)
                LOG("warning: fitted-menu cursor remap hooks failed; the native "
                    "shell/pause menu may not be fully navigable in the fitted window");

            // ClipCursor correction is independent of the remap trio; warn-only.
            if (void* pClipCursor = (void*)GetProcAddress(user32, "ClipCursor"))
            {
                if (MH_CreateHook(pClipCursor, (void*)&ClipCursorHook,
                                  (void**)&g_origClipCursor) != MH_OK)
                    LOG("warning: ClipCursor hook failed; a fitted-menu cursor "
                        "clip could still confine the pointer to the wrong rect");
            }

            // Screen-size lie (game-side only): THE fix for the menu row-clip that
            // happens whether the fit is on or off, because MCC clips its menu
            // layout against the real desktop size (SM_CYSCREEN / GetMonitorInfo)
            // while drawing at full render. Report the full render extent to
            // MCC's own code so every drawn item stays navigable. System/DXGI/our
            // own callers keep the true desktop size. Warn-only; the display fit
            // does not depend on these.
            if (void* pGetSysMetrics = (void*)GetProcAddress(user32, "GetSystemMetrics"))
            {
                if (MH_CreateHook(pGetSysMetrics, (void*)&GetSystemMetricsHook,
                                  (void**)&g_origGetSystemMetrics) != MH_OK)
                    LOG("warning: GetSystemMetrics hook failed; the native menu's "
                        "lower rows may stay unreachable (MCC clips layout to the "
                        "real screen height)");
            }
            if (void* pGetMonInfo = (void*)GetProcAddress(user32, "GetMonitorInfoW"))
            {
                if (MH_CreateHook(pGetMonInfo, (void*)&GetMonitorInfoWHook,
                                  (void**)&g_origGetMonitorInfoW) != MH_OK)
                    LOG("warning: GetMonitorInfoW hook failed; the native menu may "
                        "still clip layout to the real monitor size");
            }
        }

        // The desktop fit only engages if BOTH levers that keep MCC drawing full
        // are in place. If either failed, we leave the window at full size (the
        // previous overflow behavior) rather than risk shrinking it into a crop.
        g_fitActive = forceHookOk && clientRectHookOk;
        if (!g_fitActive)
            LOG("fit_desktop_window ON but a required hook is missing; leaving the "
                "desktop window full-size (no shrink) to avoid a cropped render");
    }

    sc->Release();
    ctx->Release();
    dev->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (!ok)
    {
        LOG("MinHook could not hook the required D3D render path");
        return false;
    }
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}
