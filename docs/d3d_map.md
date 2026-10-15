# Mapa da Camada D3D (REV2-RECOMP)

Mapeamento da região D3D (0x834E0000 - 0x83520000) obtido por tracing em tempo de execução no Linux host amd64 (amostragem por 300 frames no menu inicial e gameplay).

## 1. Top 40 Funções Mais Chamadas (Por 300 frames)

| Endereço | Chamadas (300f) | Papel Identificado / Provável | Argumentos (PPC ABI) | Detalhes Técnicos |
| :--- | :--- | :--- | :--- | :--- |
| **0x83502050** | 118.592 | `FlushDirtyRegisters` / PM4 State Commit | r3: Device, r4: Mask, r5: Base | Itera por dirty bitmask (`cntlzd`), calcula offset (`rlwinm`) e emite PM4 packets. |
| **0x834EC978** | 116.402 | `SetVertexShaderConstantF` | r3: Device, r4: StartReg, r5: pData, r6: Count | Multiplica `(r4 + 376)` por 16 bytes (`sizeof(float4)`), copia blocos de constantes. |
| **0x834EADD8** | 65.145 | `SetTexture` | r3: Device, r4: Sampler, r5: TexturePtr | Mapeia descritor de textura (dimensões, tiling, formato, base física) no sampler slot. |
| **0x834EC8A0** | 47.313 | `SetPixelShaderConstantF` | r3: Device, r4: StartReg, r5: pData, r6: Count | Multiplica `(r4 + 120)` por 16 bytes (`sizeof(float4)`), copia constantes do pixel shader. |
| **0x83502F90** | 36.542 | `SetRenderState` (Rasterizer/Blend/Depth) | r3: Device, r4: State, r5: Value | Atualiza o estado em cache no device e enfileira dword de estado no ring buffer. |
| **0x834E5060** | 34.500 | `DrawIndexedPrimitive` | r3: Device, r4: PrimType, r5: BaseVtx, r6: MinIdx, r7: NumVtx | Função primária de desenho indexado; valida bounds e grava pacote de draw PM4. |
| **0x835023E8** | 34.497 | `SetStreamSource` | r3: Device, r4: Stream, r5: VtxBuf, r6: Offset, r7: Stride | Vincula vertex buffer, offset e stride de vértices para os draws subsequentes. |
| **0x834F4980** | 31.220 | `SetPixelShader` / `SetVertexShader` | r3: Device, r4: ShaderPtr | Armazena o ponteiro do shader no device (`offset 13084/13092`) e atualiza dirty flags. |
| **0x834E8E80** | 24.600 | `SetSamplerState` | r3: Device, r4: Sampler, r5: Type, r6: Value | Configura filtragem (min/mag/mip), endereçamento (clamp/wrap) e LOD bias. |
| **0x834E7708** | 23.949 | `SetIndices` | r3: Device, r4: IndexBufferPtr | Configura o ponteiro do index buffer e endianness para `DrawIndexedPrimitive`. |
| **0x83503180** | 22.068 | `SetVertexDeclaration` | r3: Device, r4: VtxDeclPtr | Especifica o layout dos atributos de vértice (pos, norm, uv, color). |
| **0x83502290** | 21.777 | `SetViewport` | r3: Device, r4: pViewport | Viewport x, y, width, height, minZ, maxZ traduzidos para registradores Xenos. |
| **0x83503040** | 20.943 | `SetScissorRect` | r3: Device, r4: pRect | Retângulo de corte scissor test. |
| **0x834E7610** | 20.400 | `SetRenderTarget` | r3: Device, r4: Index, r5: pSurface | Define superfície de render target (cor 0 a 3). |
| **0x834E4480** | 20.400 | `SetDepthStencilSurface` | r3: Device, r4: pZSurface | Define buffer de profundidade e stencil na memória física / eDRAM. |
| **0x83501AC8** | 20.268 | `Clear` | r3: Device, r4: Count, r5: Flags, r6: Color, r7: Z, r8: Stencil | Limpeza de tela (cor, Z e stencil). |
| **0x834E7CB8** | 20.100 | `BeginTiling` / `BeginPass` | r3: Device, r4: Flags, r5: Surface | Início do bloco de renderização (predicated tiling). |
| **0x834E6BE8** | 18.600 | `EndTiling` / `EndPass` | r3: Device, r4: Flags, r5: pResolveDst | Fim do bloco de renderização e preparação de resolve. |
| **0x834ED068** | 17.043 | `Resolve` / CopySurface | r3: Device, r4: Flags, r5: SrcSurface, r6: DstTex | Cópia/resolve da eDRAM para textura de destino na RAM. |
| **0x834ECE50** | 17.043 | `ResolveParameters` | r3: Device, r4: Config | Ajusta retângulo e escala do resolve. |
| **0x834ECC48** | 16.799 | `DrawPrimitive` | r3: Device, r4: PrimType, r5: StartVtx, r6: PrimCount | Desenho de primitivas sem indexação. |
| **0x834F8650** | 15.900 | `SetBlendState` | r3: Device, r4: SrcBlend, r5: DstBlend, r6: Op | Configura fatores e equações de blending. |
| **0x834F8278** | 15.900 | `SetDepthStencilState` | r3: Device, r4: DepthEnable, r5: ZFunc | Ativa/desativa Z-buffer e função de comparação. |
| **0x834F8AE0** | 15.300 | `SetAlphaTestState` | r3: Device, r4: AlphaRef, r5: AlphaFunc | Alpha test legado (pode ser emulado via pixel shader ou pipeline). |
| **0x834E6E90** | 14.700 | `SetColorWriteMask` | r3: Device, r4: RTIndex, r5: Mask (RGBA) | Máscara de canais de escrita de cor. |
| **0x834F3F80** | 300 | `Present` / `VdSwap` | r3: Device, r4: Flags | Âncora central de swapchain e apresentação de frame na tela. |
| **0x834FA028** | Inicialização | `VdInitializeRingBuffer` | r3: Device, r4: Buffer, r5: Size | Inicialização do ring buffer de pacotes PM4 da GPU. |
| **0x83501160** | Inicialização | `VdInitializeEngines` | r3: Config | Criação e inicialização do device D3D. |

## 2. Padrões Arquiteturais e Registradores
- **Ponteiro do Device (`r3`)**: Praticamente todas as funções recebem `r3` como o ponteiro base da estrutura `D3DDevice` (tamanho ~14 KB).
- **Offsets Críticos do Device**:
  - `offset 48`: Ponteiro do Command Buffer / Ring Buffer write pointer.
  - `offset 56`: Command Buffer limit pointer (dispara flush quando alcançado).
  - `offset 120`: Base array de constantes de Pixel Shader (`float4`).
  - `offset 376`: Base array de constantes de Vertex Shader (`float4`).
  - `offset 11068`: Flags de dirty state e tipos de shader vinculados.
  - `offset 13084`: Handle do Vertex Shader atual.
  - `offset 13092`: Handle do Pixel Shader atual.
