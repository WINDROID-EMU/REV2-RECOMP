#pragma once

#include "rev2_state.h"

namespace rev2::gpu {

void Initialize();

// Callbacks acionados pelas funções D3D interceptadas
void OnPresent(uint32_t flags);
void OnClear(uint32_t count, uint32_t flags, uint32_t color, float z, uint32_t stencil);
void OnSetRenderTarget(uint32_t index, uint32_t surface_addr);
void OnSetDepthStencilSurface(uint32_t surface_addr);
void OnSetViewport(float x, float y, float w, float h, float min_z, float max_z);
void OnSetScissorRect(int32_t l, int32_t t, int32_t r, int32_t b);

void OnSetVertexShader(uint32_t shader_addr);
void OnSetPixelShader(uint32_t shader_addr);
void OnSetVSConstantF(uint32_t start_reg, const float* data, uint32_t count);
void OnSetPSConstantF(uint32_t start_reg, const float* data, uint32_t count);

void OnSetTexture(uint32_t sampler, uint32_t texture_addr, const uint8_t* base);
void OnSetSamplerState(uint32_t sampler, uint32_t type, uint32_t value);
void OnSetRenderState(uint32_t state, uint32_t value);

void OnSetStreamSource(uint32_t stream, uint32_t buf_addr, uint32_t offset, uint32_t stride);
void OnSetIndices(uint32_t idx_buf_addr);
void OnSetVertexDeclaration(uint32_t decl_addr);

void OnDrawIndexedPrimitive(uint32_t type, int32_t base_vtx, uint32_t min_idx, uint32_t num_vtx, uint32_t start_idx, uint32_t prim_count);
void OnDrawPrimitive(uint32_t type, uint32_t start_vtx, uint32_t prim_count);

void OnResolve(uint32_t flags, uint32_t src_surface, uint32_t dst_texture);
void OnBeginTiling(uint32_t tile_count = 0);
void OnEndTiling();

void DumpFrameMetrics();

} // namespace rev2::gpu
