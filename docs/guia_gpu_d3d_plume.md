# Guia passo a passo: trocar a GPU Xenos por camada D3D + plume no REV2-RECOMP

Projeto: Resident Evil Revelations 2 (Xbox 360) recompilado com o ReXGlue SDK 0.10.0 (repositório WINDROID-EMU/REV2-RECOMP).

Aviso de método: este guia foi montado lendo o repositório (código, headers e símbolos das bibliotecas). Nada foi compilado nem executado. Os pontos marcados com \[VERIFICAR\] dependem de uma confirmação sua antes de seguir.

## 0. Ideia central

**Hoje:** jogo, depois o D3D do XDK (que está ligado dentro do XEX), depois um ring buffer de pacotes PM4, depois o command processor da Xenos (plugin do SDK), depois o Vulkan. Tudo isso acontece em tempo de execução: interpretar PM4, traduzir microcódigo de shader, emular a eDRAM de 10 MB, converter texturas.

**Depois:** jogo, depois as funções D3D interceptadas (C++ nativo), depois a plume (Vulkan). Os shaders já chegam convertidos pelo XenosRecomp, e a eDRAM deixa de existir como conceito: cada resolve vira uma cópia de render target.

O ganho vem de trocar emulação de hardware por tradução direta de API. O custo é que o trabalho é específico deste jogo.

## 1. O que já existe no repositório

- `CMakeLists.txt` chama `rexglue_setup_target(RERev2 GPU_PLUGINS xenos)`. O código recompilado fica em `generated/default/` (cerca de 402 MB, arquivos `RERev2_recomp.N.cpp`).
- `RERev2.toml` usa `gpu_plugin = xenos` com backend Vulkan e várias opções de desempenho (540p, sem oclusão, `readback_resolve = none`). Os caminhos `game_data_root`, `user_data_root` e `cache_root` apontam para um disco do seu desktop e precisam ser ajustados por máquina.
- `src/main.cpp` já sobrescreve uma função recompilada à mão (`sub_82F57288`, getter do MT Framework) através de `ResolveIndirectFunction`. É a prova de que substituir funções do jogo por C++ nativo funciona neste projeto.
- O SDK Android (`android/rexglue-sdk`) traz `librexgpu-xenos.so` e `librexgpu-plume.so`, mais os headers em `include/rex/`.
- `hook.h` oferece `REX_HOOK`, `REX_HOOK_RAW` e `REX_STUB` para criar funções nativas que se encaixam na convenção das funções recompiladas.
- `build_arm64.sh` e `build_apk.sh` fazem o build para ARM64 e o APK.

### O plugin plume do SDK ainda não resolve sozinho

O header `pm4_plume_transpiler.h` descreve a plume como fase 1: ela apenas observa pacotes PM4 e conta estatísticas, e o backend Xenos antigo continua sendo quem executa. Trocar `xenos` por `plume` hoje não traz ganho. O que a plume já oferece e vamos reaproveitar é o device, o swapchain e a apresentação (`PlumeGraphicsSystem::plume_swapchain()` e `Present()`).

### Pontos de extensão encontrados

1. **Plugin de GPU próprio:** a classe `rex::graphics::GraphicsSystem` tem os virtuais `CreateProvider(bool)` e `CreateCommandProcessor()`. Serve para trocar o backend, mas você continuaria consumindo PM4.
2. **Interceptar o D3D do jogo (caminho recomendado):** substituir funções recompiladas por código nativo, sem passar por PM4.

### Onde está o D3D dentro do jogo

O jogo importa `VdInitializeEngines`, `VdInitializeRingBuffer`, `VdSwap`, `VdGetSystemCommandBuffer` e `VdSetGraphicsInterruptCallback`, o que indica o runtime D3D do XDK ligado estaticamente. Âncoras encontradas no código gerado:

- `sub_834F3F80` chama `VdSwap` e `VdGetSystemCommandBuffer`. Provável swap/present.
- `sub_834FA028` chama `VdInitializeRingBuffer`. Provável inicialização do ring buffer.
- `sub_83501160` chama `VdInitializeEngines` e `VdSetGraphicsInterruptCallback`. Provável criação do device.
- `sub_83501338` também chama `VdSetGraphicsInterruptCallback`.

A região em torno de `0x834E0000` a `0x83520000` tem cerca de 876 funções e é onde a biblioteca D3D deve estar. Os papéis acima são inferência pelos imports chamados, não nomes confirmados.

## 2. Fase 0: ambiente e baseline

Objetivo: ter um build que você reproduz e números para comparar depois.

1. Desenvolva no host Linux amd64 (há `runtime/linux-amd64`) e só então gere o ARM64 com `build_arm64.sh`. Depurar direto no Android é lento demais para esta etapa.
2. Ajuste os três caminhos do `RERev2.toml` para a sua máquina. Mantenha uma cópia do TOML por ambiente em vez de editar o do repositório.
3. Rode `cmake --build . --target RERev2_codegen` uma vez e confirme que gera os mesmos arquivos de `generated/default/`.
4. Compile e rode com a Xenos até chegar ao menu (se ainda não chega, corrija isso antes de mexer em GPU). Anote: FPS no menu, FPS em uma cena 3D fixa, uso de CPU por thread, e número de draws por frame.
5. Para o número de draws, ligue o `Pm4PlumeTranspiler` se houver cvar ou opção para isso \[VERIFICAR\] e leia `stats().draw_packets` por frame.
6. Se quiser perfil por função, compile com `REXGLUE_ENABLE_PROFILING` para ter as zonas do Tracy (as libs `libTracyClient.so` já estão em `runtime/`).

Critério de pronto: tabela com FPS, draws por frame e hot spots, salva junto do commit usado.

## 3. Fase 1: provar o mecanismo de substituição de função

O `sub_82F57288` do `main.cpp` só é substituído em chamadas indiretas (via `ResolveIndirectFunction`). Para o D3D você precisa interceptar também as chamadas diretas entre funções recompiladas, e o código gerado define cada função com `DEFINE_REX_FUNC(sub_XXXXXXXX)`. Há duas formas.

**Forma A: o codegen tem suporte nativo.** \[VERIFICAR\] Rode `rexglue --help` e procure no binário por palavras como hook, skip, override ou replace (`strings android/rexglue-sdk/bin/rexglue`, ou o binário do host). Veja também as chaves aceitas em `[functions.\"0x...\"]` no `default_functions.toml`. Se existir uma chave para dizer que a função é fornecida por você, use ela: é a forma mais limpa.

**Forma B: renomear a definição gerada.** Um script pós-codegen troca `DEFINE_REX_FUNC(sub_X)` por `DEFINE_REX_FUNC(sub_X__orig)`. Como as chamadas dos outros arquivos usam o símbolo `sub_X`, a sua função com esse nome passa a valer, e a original continua acessível como `sub_X__orig` para fallback. \[VERIFICAR\] se a declaração `DECLARE_REX_FUNC` e o linkage (`extern C`) batem com a sua definição.

Esqueleto de uma função substituta (nomes de macro como no `hook.h` e no `main.cpp`):

```cpp
// src/rev2_hooks.cpp
#include <rex/hook.h>

DECLARE_REX_FUNC(sub_834F3F80__orig);   // original renomeada pelo script

REX_HOOK_RAW(sub_834F3F80) {
  // argumentos do jogo: r3, r4, r5... (ABI PowerPC); retorno em r3
  // leitura de memória guest: REX_LOAD_U32(endereco)
  // fallback: sub_834F3F80__orig(ctx, base);
}
```

Critério de pronto: você substitui uma função trivial, o jogo continua rodando, e um log confirma que a sua versão foi chamada tanto por chamada direta quanto indireta.

## 4. Fase 2: medir quem é chamado (tracing da região D3D)

Antes de substituir qualquer coisa, descubra o que o jogo realmente chama por frame. A forma que não depende do mecanismo da fase 1 é injetar um contador no início das funções da região.

`src/rev2_trace.h`:

```cpp
#pragma once
#include <cstdint>
void Rev2TraceHit(uint32_t addr);
void Rev2TraceDump();
#define REV2_TRACE(addr) Rev2TraceHit(addr)
```

`src/rev2_trace.cpp`:

```cpp
#include \"rev2_trace.h\"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <vector>

static constexpr uint32_t kBase = 0x834E0000, kEnd = 0x83520000;
static std::atomic<uint32_t> g_hits[(kEnd - kBase) / 4];
static std::atomic<uint32_t> g_swaps{0};

void Rev2TraceHit(uint32_t a) {
  if (a < kBase || a >= kEnd) return;
  g_hits[(a - kBase) >> 2].fetch_add(1, std::memory_order_relaxed);
  if (a == 0x834F3F80 && (g_swaps.fetch_add(1) % 300) == 299) Rev2TraceDump();
}

void Rev2TraceDump() {
  std::vector<std::pair<uint32_t, uint32_t>> v;
  for (uint32_t i = 0; i < (kEnd - kBase) / 4; ++i) {
    uint32_t h = g_hits[i].exchange(0);
    if (h) v.push_back({h, kBase + i * 4});
  }
  std::sort(v.rbegin(), v.rend());
  for (size_t i = 0; i < v.size() && i < 60; ++i)
    std::printf(\"%08X  %u\\n\", v[i].second, v[i].first);
}
```

`tools/patch_generated.py` (rode depois de cada codegen):

```python
import re, glob

PAT = re.compile(r'DEFINE_REX_FUNC[(](sub_83(?:4[EF]|5[01])[0-9A-F]{4})[)] [{]')

def add_trace(m):
    return m.group(0) + ' REV2_TRACE(0x' + m.group(1)[4:] + ');'

for path in glob.glob('generated/default/RERev2_recomp.*.cpp'):
    src = open(path).read()
    out = PAT.sub(add_trace, src)
    if out != src:
        open(path, 'w').write(out)
```

No `CMakeLists.txt`, depois do `rexglue_setup_target`, inclua o header em todos os arquivos recompilados e adicione `src/rev2_trace.cpp` aos fontes:

```cmake
if(TARGET RERev2_recomp)
  target_compile_options(RERev2_recomp PRIVATE -include ${CMAKE_SOURCE_DIR}/src/rev2_trace.h)
endif()
```

A contagem de chamadas por 300 frames mostra o perfil do D3D. Espere ver no topo as funções de state (SetRenderState, SetTexture, SetShaderConstants), de draw e de swap. Salve a lista: ela é o seu backlog ordenado por importância.

Critério de pronto: lista das 60 funções mais chamadas, com um palpite de papel para cada uma (use o padrão de chamadas e o tamanho da função, e compare com o que se conhece da API D3D9 do Xbox 360).

## 5. Fase 3: mapear a camada D3D

O código gerado não tem símbolos. Você vai nomear as funções por evidência.

1. Comece pelas âncoras da seção 1 e siga os chamadores para cima, até chegar nas funções públicas que o motor da Capcom usa.
2. Para cada função, anote: endereço, nome provável, argumentos (r3 costuma ser o ponteiro do device), o que escreve no ring buffer, e se há estado em cache dentro do objeto device.
3. Procure padrões típicos: funções que escrevem uma sequência fixa de dwords no ring buffer (pacotes PM4) são setters de estado. As que escrevem um pacote de draw (índices e tipo de primitiva) são os draws. As que chamam `VdSwap` são o swap.
4. Procure também se o motor tem uma camada própria de render acima do D3D (o MT Framework costuma ter). Se tiver, pode ser mais fácil interceptar nela, em um nível mais alto e mais estável. \[VERIFICAR\]
5. Marque os pontos que escapam do D3D: as poucas referências a `0x7FC...` nos arquivos `recomp.15`, `16` e `21` e qualquer uso de memexport. Esses precisam de tratamento manual.

Mantenha uma planilha ou arquivo `docs/d3d_map.md` no repositório com essa tabela. Ela vai ser consultada por todas as fases seguintes.

Critério de pronto: os 30 a 40 endereços necessários para desenhar um frame estão identificados com confiança razoável.

## 6. Fase 4: shaders

O XenosRecomp converte microcódigo de shader da Xenos para HLSL de forma offline. Em runtime só se procura o shader pelo hash.

1. **Capturar os shaders.** Intercepte (ou observe) a criação de vertex e pixel shaders do D3D, onde o jogo entrega o blob com microcódigo, e grave cada blob único em disco com um hash do conteúdo. Alternativa: pegar do cache de shaders que a Xenos atual já guarda em `cache_root`. \[VERIFICAR\] o formato.
2. **Rodar o XenosRecomp** sobre os blobs. Leia o README dele para o formato de entrada esperado, porque ele define o que o seu dump precisa conter.
3. **Compilar o HLSL** para SPIR-V (por exemplo com dxc) e embutir o resultado em uma tabela hash para blob de SPIR-V, que o backend lê em runtime.
4. **Não commite** os dumps nem os shaders convertidos: são derivados do jogo. Versione apenas as ferramentas e a tabela de hashes se isso for permitido no seu caso.

Pontos a decidir aqui:

- Como o vertex fetch (`vfetch`) é tratado: ler os dados de vértice em buffers de storage dentro do shader (mais geral e simples de começar) ou converter para atributos de vértice padrão ao criar a vertex declaration (mais rápido, mais trabalho). Comece pelo primeiro.
- Constantes: a Xenos usa registradores de float constants, mais constantes de bool e int por estágio. Defina um layout de uniform/storage buffer e preencha a partir dos setters de constantes interceptados.
- Guarde a variante certa por combinação de shader e estado que mude a compilação (por exemplo, formato dos vértices).

Critério de pronto: todos os shaders vistos em menu e em uma fase compilam para SPIR-V sem erro.

## 7. Fase 5: backend plume

1. Reaproveite o device e o swapchain da `PlumeGraphicsSystem` que já existe no SDK. Mantenha o plugin plume como o plugin de GPU para não reescrever apresentação e o ciclo de vida do device.
2. Crie o módulo `rev2_gpu` (por exemplo `src/rev2_gpu/`) com: gerenciador de buffers e texturas, cache de pipelines (chave: shaders, formato dos vértices, blend, depth, rasterização, formatos dos render targets), e um ciclo por frame.
3. Mantenha um comando por frame: o D3D do jogo chama muitas funções, você grava num command buffer da plume e só envia no Present.
4. Faça o cache de pipelines ser persistente em disco (em `cache_root`) e compile pipelines de forma assíncrona, para evitar travadas no Android.
5. Se quiser inspecionar frames, o SDK traz `renderdoc_app.h`; capture em Vulkan e compare com o que a Xenos produz.

Critério de pronto: um frame vazio é apresentado pela sua camada com a cor de fundo certa.

## 8. Fase 6: substituir funções, de baixo para cima

A regra: substitua um conjunto pequeno, teste, e só então avance. Mantenha uma opção (cvar ou variável de ambiente, por exemplo `rev2_native_gpu`) para voltar à Xenos.

**Atenção importante:** as funções de state do D3D do XDK escrevem pacotes PM4 diretamente no ring buffer e também atualizam o estado em cache do device. Se você interceptar só os draws, a Xenos não receberia mais o estado certo. Portanto, ao ligar o caminho nativo, o ideal é interceptar todo o conjunto de state que o jogo usa e deixar a Xenos receber só o mínimo para manter o kernel feliz (ou desligá-la nesse modo). Teste isso cedo.

Ordem sugerida:

1. **Present/Swap** (`sub_834F3F80` e vizinhas): apresenta o frame pela plume. O jogo ainda não desenha nada, mas já deve rodar com tela limpa.
2. **Clear**: preenche render target e depth.
3. **Render targets e depth**: `SetRenderTarget` e `SetDepthStencilSurface` (nomes prováveis). Cada superfície do jogo vira uma textura plume, com um mapa de endereço guest para recurso.
4. **Viewport, scissor e render states**: blend, depth, stencil, cull, máscaras de cor.
5. **Shaders e constantes**: `SetVertexShader`, `SetPixelShader`, setters de constantes.
6. **Vertex declaration, streams e índices**: `SetVertexDeclaration`, `SetStreamSource`, `SetIndices`. Dados de vértice e índice do Xbox 360 estão em big-endian, então precisam de byte swap por elemento no upload (ou na leitura do shader).
7. **Draws**: `DrawPrimitive`, `DrawIndexedPrimitive` e variantes (UP, instanciado). Tipos de primitiva da Xenos que o Vulkan não tem (rect list, quad list, triangle fan) exigem conversão de índices ou tratamento no vertex shader. O `primitive_processor.h` do SDK pode ter essa lógica pronta para reaproveitar. \[VERIFICAR\]
8. **Texturas e samplers**: `SetTexture`, `SetSamplerState`. Veja a fase 9.
9. **Resolve**: veja a fase 10.

Critério de pronto por etapa: o jogo continua rodando, e você consegue comparar o frame com o da Xenos em uma cena fixa.

## 9. Fase 7: texturas

Texturas da Xenos têm layout em tiles, são big-endian e usam formatos próprios (DXT1/3/5, DXN, CTX1, 8888, 565, 5551, 4444, 16F, 32F, entre outros).

1. Ao interceptar a criação ou o uso de texturas, leia o descritor (endereço guest, largura, altura, profundidade, formato, mip levels, tiling, endian).
2. Faça o untile e o byte swap no upload. O SDK tem utilitários de formato em `include/rex/graphics/format` e `util` que podem cobrir isso. \[VERIFICAR\]
3. Para o Android, considere fazer o untile em compute shader ou em worker threads para não pesar na thread do jogo.
4. Cache por endereço e por versão do conteúdo: recarregar a textura inteira a cada draw destrói o desempenho. Descubra como o jogo sinaliza que uma textura foi modificada.
5. Formatos que o Vulkan não suporta direto precisam de conversão na CPU ou no shader.

Critério de pronto: texturas do menu e de uma fase aparecem corretas, sem cores trocadas nem inversão.

## 10. Fase 8: resolve e eDRAM

No hardware real, o jogo renderiza na eDRAM e depois faz um resolve para memória. Na sua camada não existe eDRAM.

1. Intercepte a função de resolve (nome provável `Resolve`) e trate como cópia do render target atual para a textura de destino, com o formato e a região pedidos.
2. Intercepte `BeginTiling` e `EndTiling` (predicated tiling) e trate como no-op.
3. Pós-processamento, sombras e HDR costumam depender de resolves em cadeia: valide cada passe separadamente com capturas do RenderDoc.
4. Se alguma textura é lida de volta pela CPU (readback), identifique onde e trate de forma explícita.

Critério de pronto: efeitos de pós-processamento e sombras aparecem corretos.

## 11. Fase 9: Android e ARM64

1. Mire no mínimo comum entre Adreno e Mali: evite recursos opcionais do Vulkan nas primeiras versões e cheque limites (tamanho de storage buffer, número de descritores).
2. Persista o cache de pipelines e use criação assíncrona para evitar travadas.
3. Compilar 400 MB de código gerado é lento: use compilação cruzada no host com cache (ccache) e evite recompilar tudo a cada mudança. Mantenha o código novo em arquivos separados de `generated/`.
4. Cuidado com ordenação de memória entre threads: PowerPC e ARM64 têm modelos fracos, mas com diferenças. Bugs intermitentes nesse ponto aparecem sob carga.
5. Meça de novo com a mesma tabela da fase 0 no aparelho real.

## 12. Fase 10: validação e entrega

- Teste de regressão visual: três cenas fixas (menu, interior, exterior), screenshots da Xenos como referência e as da sua camada, com diferença por imagem.
- Mantenha o fallback para a Xenos até a sua camada cobrir o jogo inteiro.
- Documente no repositório: `docs/d3d_map.md` (mapa de funções), `docs/state_coverage.md` (o que está coberto) e o procedimento de build.

## 13. Riscos principais

- **Camada própria de render do motor:** muda onde interceptar. A fase 3 responde isso cedo.
- **State e PM4 acoplados:** interceptar só parte das funções deixa o estado inconsistente. A fase 6 explica a regra.
- **Memexport, tessellation e escrita direta de registradores:** fogem do D3D e precisam de tratamento manual.
- **Streaming e formatos de arquivo da Capcom:** qualquer erro no sistema de arquivos emulado aparece como falha gráfica ou travamento.
- **Escopo:** é um trabalho grande. Planeje em marcos pequenos, cada um com critério de pronto claro.

## 14. Marcos

1. M0: baseline medido e salvo.
2. M1: mecanismo de substituição provado.
3. M2: top 60 funções da região D3D listadas.
4. M3: mapa D3D com 30 a 40 funções nomeadas.
5. M4: shaders do menu convertidos.
6. M5: Present e Clear pela plume.
7. M6: primeiro desenho (logo ou menu) pela camada nova.
8. M7: texturas corretas.
9. M8: cena 3D completa com estado correto.
10. M9: resolves e pós-processamento.
11. M10: otimização e validação no Android.

## 15. Referências

- UnleashedRecomp, XenosRecomp e plume (projetos do mesmo ecossistema de recompilação de Sonic Unleashed): modelo de arquitetura a imitar.
- Xenia: referência para formatos de textura da Xenos, tipos de primitiva e pacotes PM4.
- Headers do SDK: `rex/hook.h`, `rex/graphics/graphics_system.h`, `rex/graphics/pm4_plume_transpiler.h`, `rex/graphics/primitive_processor.h`.

Use apenas a sua própria cópia legítima do jogo e não distribua dados ou shaders extraídos dele.
