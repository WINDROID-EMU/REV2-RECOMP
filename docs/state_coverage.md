# Cobertura de Estado e Arquitetura da Camada Nativa D3D + Plume

Este documento detalha o status de implementação, cobertura de estado e validação da camada de tradução direta **D3D -> Plume / Vulkan** para o **Resident Evil: Revelations 2 (REV2-RECOMP)**, substituindo progressivamente a emulação da GPU Xbox 360 Xenos (intérprete de anel PM4, tradução de microcódigo e eDRAM).

---

## 1. Visão Geral da Arquitetura

```
+-------------------------------------------------------------+
|    Resident Evil: Revelations 2 (Código Recompilado C++)    |
+-------------------------------------------------------------+
                             |
                   Chamadas D3D9 / MT Framework
                             v
+-------------------------------------------------------------+
|     Camada Nativa REV2 D3D (`src/rev2_gpu`, `rev2_hooks`)    |
|  - Gerenciador de Estado do Pipeline (`rev2_state.h`)       |
|  - Cache e Decodificador de Texturas (`TextureCache`)       |
|  - Bypassing de eDRAM e Cópias Diretas de Superfície        |
|  - Shaders Precompilados SPIR-V (`5841144C_prebaked.spv`)   |
+-------------------------------------------------------------+
                             |
                     Comandos Vulkan / Plume
                             v
+-------------------------------------------------------------+
|           PlumeGraphicsSystem / Backend Vulkan Nativo       |
+-------------------------------------------------------------+
```

---

## 2. Cobertura de Funções Interceptadas (27 Pontos de Interceptação)

A camada intercepta 27 rotas centrais da API D3D do jogo, permitindo execução **100% nativa sem fallback para o plugin Xenos** (`REV2_FALLBACK_XENOS=0` por padrão):

| Endereço Guest | Função D3D / Papel | Mecanismo de Interceptação | Status de Cobertura Nativa |
| :--- | :--- | :--- | :--- |
| **0x834F3F80** | `IDirect3DDevice9::Present` / Swap | Weak symbol alias (`sub_834F3F80`) | **100% Nativo** (Apresentação via Plume/Vulkan sem PM4) |
| **0x83501AC8** | `IDirect3DDevice9::Clear` | Weak symbol alias (`sub_83501AC8`) | **100% Nativo** (Color, Depth, Stencil rastreados e despachados) |
| **0x834E7610** | `IDirect3DDevice9::SetRenderTarget` | Weak symbol alias (`sub_834E7610`) | **100% Rastreado** (RT0 a RT3) |
| **0x834E4480** | `IDirect3DDevice9::SetDepthStencilSurface` | Weak symbol alias (`sub_834E4480`) | **100% Rastreado** (Superfície Z/Stencil) |
| **0x83502290** | `IDirect3DDevice9::SetViewport` | Weak symbol alias (`sub_83502290`) | **100% Rastreado** (X, Y, W, H, MinZ, MaxZ dinâmicos) |
| **0x834EC978** | `SetVertexShaderConstantF` | Weak symbol alias (`sub_834EC978`) | **100% Rastreado** (Registradores c0 a c255 no UBO Set 1) |
| **0x834EC8A0** | `SetPixelShaderConstantF` | Weak symbol alias (`sub_834EC8A0`) | **100% Rastreado** (Registradores c0 a c63 no UBO Set 1) |
| **0x834EADD8** | `IDirect3DDevice9::SetTexture` | Weak symbol alias (`sub_834EADD8`) | **100% Rastreado** (Samplers 0 a 15 + TextureCache nativo) |
| **0x834E5060** | `DrawIndexedPrimitive` | Weak symbol alias (`sub_834E5060`) | **100% Nativo** (Captura de geometria e `vkCmdDrawIndexed` Plume) |
| **0x834ECC48** | `DrawPrimitive` | Weak symbol alias (`sub_834ECC48`) | **100% Nativo** (`vkCmdDraw` Plume sem indexação) |
| **0x834ED068** | `Resolve` / CopySurface | Weak symbol alias (`sub_834ED068`) | **100% Rastreado** (Bypass eDRAM -> Memória Direta) |
| **0x834ECE50** | `ResolveParameters` | Weak symbol alias (`sub_834ECE50`) | **100% Rastreado** (Configuração de escala e retângulos) |
| **0x834E7CB8** | `BeginTiling` (Predicated Tiling) | Weak symbol alias (`sub_834E7CB8`) | **Bypassed** (No-Op em Vulkan) |
| **0x834E6BE8** | `EndTiling` | Weak symbol alias (`sub_834E6BE8`) | **Bypassed** (No-Op em Vulkan) |
| **0x83502050** | `FlushDirtyRegisters` | Weak symbol alias (`sub_83502050`) | **Bypassed** (No-Op sem envio de pacotes legados PM4) |
| **0x835023E8** | `SetStreamSource` | Weak symbol alias (`sub_835023E8`) | **100% Rastreado** (Ponteiro, offset e stride do vertex buffer) |
| **0x834E7708** | `SetIndices` | Weak symbol alias (`sub_834E7708`) | **100% Rastreado** (Ponteiro guest do index buffer 16/32-bit) |
| **0x83503180** | `SetVertexDeclaration` | Weak symbol alias (`sub_83503180`) | **100% Rastreado** (Layout dos atributos de vértice) |
| **0x834F4980** | `SetVertexShader` | Weak symbol alias (`sub_834F4980`) | **100% Rastreado** (Mapeamento para shaders SPIR-V) |
| **0x834F4AF0** | `SetPixelShader` | Weak symbol alias (`sub_834F4AF0`) | **100% Rastreado** (Mapeamento para shaders SPIR-V) |
| **0x83502F90** | `SetRenderState` | Weak symbol alias (`sub_83502F90`) | **100% Rastreado** (Blend, Z-test, Cull, WriteMask) |
| **0x834E8E80** | `SetSamplerState` | Weak symbol alias (`sub_834E8E80`) | **100% Rastreado** (Filtros min/mag/mip, endereçamento U/V/W) |
| **0x83503040** | `SetScissorRect` | Weak symbol alias (`sub_83503040`) | **100% Rastreado** (Retângulo dinâmico de corte Vulkan) |
| **0x834F8650** | `SetBlendState` | Weak symbol alias (`sub_834F8650`) | **100% Rastreado** (Src/Dst blend factors e BlendOp) |
| **0x834F8278** | `SetDepthStencilState` | Weak symbol alias (`sub_834F8278`) | **100% Rastreado** (Depth enable e Z-compare op) |
| **0x834F8AE0** | `SetAlphaTestState` | Weak symbol alias (`sub_834F8AE0`) | **100% Rastreado** (Alpha ref e Alpha func) |
| **0x834E6E90** | `SetColorWriteMask` | Weak symbol alias (`sub_834E6E90`) | **100% Rastreado** (Máscara RGBA por RT) |

---

## 3. Gerenciamento e Cache de Texturas (Fase 7)

As texturas do Xbox 360 utilizam empacotamento em ladrilhos (tiles 32x32), ordenação big-endian e formatos específicos (DXT1, DXT5, DXN, CTX1, etc.).

1. **Decodificação de Descritor em Tempo Real**:
   - `dword0`: Largura (bits 0-12) e Altura (bits 13-25).
   - `dword1`: Formato Xenos (bits 24-29), Níveis de Mipmap (bits 1-4) e Flag Tiled (bit 0).
   - `dword2`: Endereço base guest em memória (`base_ptr`).
2. **Texture Cache com Resolução de Conflitos e Untiling Nativo (Fase 7)**:
   - Tabela hash indexada por `guest_addr`.
   - Reutilização instantânea de texturas já residentes no cache (evitando re-uploads por draw).
   - Motor de untile micro-tile / macro-tile (`UntileTexture2D`) integrando `rex::graphics::texture_util::GetTiledOffset2D` e `FormatInfo` para desempacotar texturas tiled Xenos em memória linear host.
   - Métricas de cache hits e misses registradas a cada frame.

---

## 3b. Gerenciamento de Pipelines, Buffers e Shaders SPIR-V (Marco M6)

1. **ShaderManager**:
   - Carregamento de 584 shaders prebaked SPIR-V de `cache/shaders/shareable/5841144C_prebaked.spvcache`.
   - Shaders de fallback nativos (`kFallbackVertexSpv` e `kFallbackPixelSpv`) embarcados diretamente em `rev2_shaders.cpp`, garantindo que draw calls sem hash correspondente nunca falhem.
   - Criação e cache de `VkShaderModule` sobre o device Vulkan ativo.
2. **PipelineManager e Arquitetura de 4 Descriptor Sets**:
   - Compatibilidade total com a interface de shaders `SpirvShaderTranslator`:
     - **Set 0**: Storage Buffer SSBO (`xe_shared_memory`) para suporte a instruções `vfetch`.
     - **Set 1**: Uniform Buffers para Constantes do Sistema (binding 0), Constantes Float do VS (binding 1), Constantes Float do PS (binding 2), Constantes Bool/Loop (binding 3) e Constantes de Fetch (binding 4).
     - **Set 2**: Layout reservado.
     - **Set 3**: 16 Sampled Images (bindings 0..15) e 16 Samplers (bindings 16..31).
3. **Pipeline de Buffers de Geometria Dinâmica**:
   - `s_vk_index_buf` e `s_vk_vertex_buf` alocados dinamicamente em memória `HOST_VISIBLE | HOST_COHERENT`.
   - Conversão em tempo de execução de índices e atributos de vértice de Big-Endian (Xbox 360) para formato nativo Host Little-Endian.
   - Atualização e vinculação direta durante `OnPresent` via `vkCmdBindVertexBuffers`, `vkCmdBindIndexBuffer` e `vkCmdDrawIndexed`.

---

## 4. Eliminação de eDRAM e Resolução Direta (Fase 8)

No Xbox 360 físico, os render targets ficavam na memória ultrarrápida de 10 MB (eDRAM), exigindo divisão em blocos (`BeginTiling`/`EndTiling`) e cópia posterior para a memória principal via `Resolve`.

1. **Bypass de Tiling**:
   - No Vulkan/Plume, o framebuffer completo de 720p cabe diretamente na VRAM.
   - `BeginTiling` e `EndTiling` são tratados como no-ops estruturais sem fragmentar passes de renderização.
2. **Resolve Nativo**:
   - Interceptado em `0x834ED068`, vinculando a superfície de render ativa diretamente à textura de destino.

---

## 5. Emissão Direta de vkCmdDrawIndexed na Plume

1. **Mapeamento Topológico de Primitivas**:
   - Conversão de primitivas D3D (`POINTLIST`, `LINELIST`, `LINESTRIP`, `TRIANGLELIST`, `TRIANGLESTRIP`, `TRIANGLEFAN`, `RECTLIST`) para cálculo exato de `index_count`.
2. **Despacho Vulkan**:
   - `VkDrawIndexedCommand` estruturado com `(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)`.
   - Registro em buffer de comandos de quadro (`frame_draw_list`) e sincronização na fila gráfica Vulkan.
3. **Métricas de Execução em Tempo Real (Amostra de 1800 Frames)**:

```
=======================================================
  [REV2_GPU] Metricas da Camada D3D + Plume (Frame 1801)
  - Draws Indexados (D3D):  77.405
  - vkCmdDrawIndexed Plume: 77.405 (100% Paridade)
  - Draws Primitivos:       0
  - Atualizacoes Const VS:  123.458
  - Atualizacoes Const PS:  185.805
  - Vinculacoes Textura:    91.263 (Hits: 30.734, Misses: 6.995)
  - Texturas Tiled Detect:  165 (6.28 MB est.)
  - Cache Texturas Vivas:   19
  - Operacoes Clear:        28.664
  - Operacoes Resolve:      20.610
=======================================================
```

---

## 6. Validação Visual e Regressão

| Referência Xenos (Baseline) | Camada Nativa D3D + Plume Validada |
| :---: | :---: |
| ![Baseline](file:///home/amd/.gemini/antigravity-ide/brain/2c22497a-9ff2-4578-90c5-d96a438304e5/baseline_menu.png) | ![D3D Nativo](file:///home/amd/.gemini/antigravity-ide/brain/2c22497a-9ff2-4578-90c5-d96a438304e5/final_validated_d3d_menu.png) |
| Menu principal inicial do jogo renderizado pelo emulador Xenos original. | Menu principal renderizado com a nova camada interceptando e rastreando D3D nativamente. |

---

## 7. Procedimentos de Build

### Host Linux x86_64
```bash
cd recomp_project
cmake -B build -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./run.sh
```

### Linux ARM64 (aarch64)
```bash
cd recomp_project
./build_arm64.sh
```

### Android (APK)
```bash
cd recomp_project
./build_apk.sh release
```
