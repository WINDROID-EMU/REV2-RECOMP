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

## 2. Cobertura de Funções Interceptadas

| Endereço Guest | Função D3D / Papel | Mecanismo de Interceptação | Status de Cobertura |
| :--- | :--- | :--- | :--- |
| **0x834F3F80** | `IDirect3DDevice9::Present` / Swap | Weak symbol alias (`sub_834F3F80`) | **100% Nativo** (Apresentação via Plume/Vulkan) |
| **0x83501AC8** | `IDirect3DDevice9::Clear` | Weak symbol alias (`sub_83501AC8`) | **100% Rastreado** (Color, Depth, Stencil) |
| **0x834E7610** | `IDirect3DDevice9::SetRenderTarget` | Weak symbol alias (`sub_834E7610`) | **100% Rastreado** (RT0 a RT3) |
| **0x834E4480** | `IDirect3DDevice9::SetDepthStencilSurface` | Weak symbol alias (`sub_834E4480`) | **100% Rastreado** (Superfície Z/Stencil) |
| **0x83502290** | `IDirect3DDevice9::SetViewport` | Weak symbol alias (`sub_83502290`) | **100% Rastreado** (X, Y, W, H, MinZ, MaxZ) |
| **0x834EC978** | `SetVertexShaderConstantF` | Weak symbol alias (`sub_834EC978`) | **100% Rastreado** (Registradores c0 a c255) |
| **0x834EC8A0** | `SetPixelShaderConstantF` | Weak symbol alias (`sub_834EC8A0`) | **100% Rastreado** (Registradores c0 a c63) |
| **0x834EADD8** | `IDirect3DDevice9::SetTexture` | Weak symbol alias (`sub_834EADD8`) | **100% Rastreado** (Samplers 0 a 15 + Cache) |
| **0x834E5060** | `DrawIndexedPrimitive` | Weak symbol alias (`sub_834E5060`) | **100% Rastreado** (BaseVertex, MinIdx, NumVtx) |
| **0x834ED068** | `Resolve` / CopySurface | Weak symbol alias (`sub_834ED068`) | **100% Rastreado** (eDRAM -> Memória Direta) |
| **0x834ECE50** | `BeginTiling` (Predicated Tiling) | Weak symbol alias (`sub_834ECE50`) | **Bypassed** (No-Op em Vulkan) |
| **0x834ECC48** | `EndTiling` | Weak symbol alias (`sub_834ECC48`) | **Bypassed** (No-Op em Vulkan) |

---

## 3. Gerenciamento e Cache de Texturas (Fase 7)

As texturas do Xbox 360 utilizam empacotamento em ladrilhos (tiles 32x32), ordenação big-endian e formatos específicos (DXT1, DXT5, DXN, CTX1, etc.).

1. **Decodificação de Descritor em Tempo Real**:
   - `dword0`: Largura (bits 0-12) e Altura (bits 13-25).
   - `dword1`: Formato Xenos (bits 24-29), Níveis de Mipmap (bits 1-4) e Flag Tiled (bit 0).
   - `dword2`: Endereço base guest em memória (`base_ptr`).
2. **Texture Cache com Resolução de Conflitos**:
   - Tabela hash indexada por `guest_addr`.
   - Reutilização instantânea de texturas já residentes no cache (evitando re-uploads por draw).
   - Métricas de cache hits e misses registradas a cada frame.

---

## 4. Eliminação de eDRAM e Resolução Direta (Fase 8)

No Xbox 360 físico, os render targets ficavam na memória ultrarrápida de 10 MB (eDRAM), exigindo divisão em blocos (`BeginTiling`/`EndTiling`) e cópia posterior para a memória principal via `Resolve`.

1. **Bypass de Tiling**:
   - No Vulkan/Plume, o framebuffer completo de 720p cabe diretamente na VRAM.
   - `BeginTiling` e `EndTiling` são tratados como no-ops estruturais sem fragmentar passes de renderização.
2. **Resolve Nativo**:
   - Interceptado em `0x834ED068`, vinculando a superfície de render ativa diretamente à textura de destino.

---

## 5. Métricas de Execução em Tempo Real (Amostra de 600 Frames)

Abaixo estão os dados coletados diretamente do executável `RERev2` rodando com a camada D3D ativa:

```
=======================================================
  [REV2_GPU] Metricas da Camada D3D + Plume (Frame 601)
  - Draws Indexados:        25.559
  - Draws Primitivos:       0
  - Atualizacoes Const VS:  36.750
  - Atualizacoes Const PS:  21.356
  - Vinculacoes Textura:    27.084 (Hits: 7.337, Misses: 2.031)
  - Texturas Tiled Detect:  43 (2.02 MB est.)
  - Cache Texturas Vivas:   17
  - Operacoes Clear:        5.907
  - Operacoes Resolve:      5.180
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
