# Limitações e Recomendações Técnicas (Regra 8 — AGENTE REGRAS.md)

**Jogo:** Resident Evil: Revelations 2 (`5841144C`)  
**Backend:** Vulkan / ReXGlue  

---

## 1. Classificação de Recursos

- **Resolução de Thunks PowerPC:** **OK** (Despachante dinâmico de 32-bit trata saltos indiretos `bctr`).
- **Shader Pipeline PSO Cache:** **OK** (584 shaders do microcódigo Xenos emparelhados em `5841144C.fbo.vk.xpso`).
- **Prevenção de Stuttering (Frametime Drops):** **OK** (`vulkan_async_skip_incomplete_frames = true` e `vulkan_pipeline_creation_threads = 2`).
- **Isolamento de Cache de Shaders:** **OK** (Qualquer arquivo ou cache legado que não pertença estritamente ao Title ID `5841144C` é automaticamente expurgado pelo app).
- **Consumo de Memória de Texturas:** **ATENÇÃO** (Em dispositivos com 4 GB ou 6 GB de RAM física, manter a resolução em 720p ou 540p e limitar o cache de texturas para 384 MB ou 512 MB para evitar acionamento do LMK do Android).
- **Driver Gráfico Vulkan (Adreno):** **ATENÇÃO** (Em processadores Snapdragon Adreno 6xx/7xx, o uso de drivers proprietários antigos da Qualcomm pode apresentar artefatos visuais; recomenda-se a instalação de Turnip drivers via menu de configurações).
