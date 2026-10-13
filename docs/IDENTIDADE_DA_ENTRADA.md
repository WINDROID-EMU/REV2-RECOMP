# Identidade Exata da Entrada (Regra 2 — AGENTE REGRAS.md)

**Jogo:** Resident Evil: Revelations 2 (Biohazard Revelations 2)  
**Projeto:** Recompilação Nativa Independente (Xbox 360 → C++23 / Vulkan)  
**Data de Registro:** 13 de Outubro de 2026  

---

## 1. Identificação Geral da Versão

| Campo | Valor |
| :--- | :--- |
| **Título Oficial** | Resident Evil - Revelations 2 |
| **Região** | USA, Europe (Multirregião) |
| **Versão da Mídia** | Retail XGD3 ISO (8.738.846.720 bytes) |
| **Title ID** | `5841144C` (ou `0x5841144C`) |
| **Media ID** | `639D55D4` |
| **Title ID do Disco Bootstrapper** | `43430840` |
| **Versão do Jogo** | `0.0.0.3` (Episódio 1 Retail + Updates) |
| **Motor de Jogo** | Capcom MT Framework |
| **Idiomas Suportados** | Inglês, Japonês, Francês, Alemão, Espanhol, Italiano, Português (Brasil), Chinês, Coreano, Polonês, Russo |

---

## 2. Hashes Criptográficos (SHA-256)

| Arquivo | Tamanho (Bytes) | Hash SHA-256 |
| :--- | :--- | :--- |
| `extracted/default.xex` (Retail) | 27.135.488 bytes | `faa11015d660a73a2a43388e5ddf29ff81754f3003d1b962257ac0a397caac80` |
| `cache/shaders/shareable/5841144C.xsh` | 485.504 bytes | `3aa45a80fa9ca65f9a6501a4e12c1b75383a15c898ca7be8fba2fc5d3d4bb980` |
| `cache/shaders/shareable/5841144C.fbo.vk.xpso` | 39.084 bytes | `9b36d013bc0d98ca7da114ad044b76c8c4a169b0fa658d844c5a0833156cfd7e` |

---

## 3. Ambiente e Toolchain

- **Compilador Host:** Clang 18 (C++23) / Android NDK r27b
- **ReXGlue SDK:** v0.10.0-dev
- **GPU Backend:** Vulkan 1.2 / 1.3 (Adreno Turnip / Vulkan Host)
- **Shader Pipeline:** 584 Shaders Xenos traduzidos com correspondência de 1:1 nos Pipeline State Objects (584/584 entries).
- **Isolamento de Projeto:** 100% isolado e autônomo, específico e exclusivo de Resident Evil: Revelations 2 (Title ID `5841144C`), sem compartilhamento de shaders ou dependências de outros jogos.
