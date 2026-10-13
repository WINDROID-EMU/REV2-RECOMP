# Matriz de Validação Gráfica e de Execução (Regra 14 — AGENTE REGRAS.md)

**Título:** Resident Evil: Revelations 2  
**Title ID:** `5841144C`  
**Backend:** Vulkan (Xenos Emulation Pipeline)  

---

## 1. Tabela de Validação de Shaders e Pipelines

| Área | Cenário | Referência (X360) | Vulkan | Diferença | Estado | Observação |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **VS** | Menus / HUD / UI | OK | OK | Mínima | **OK** | 100% alinhamento de coordenadas de projeção |
| **VS** | Modelos de Personagens (Claire/Barry) | OK | OK | Mínima | **OK** | Vertex fetches e bone weights em paridade |
| **PS** | Materiais Opacos / Cenário | OK | OK | Mínima | **OK** | Normal maps e texturas PBR mapeados |
| **PS** | Transparências / Alpha Blending | OK | OK | Baixa | **OK** | Testes de alpha test e dual source blend ativos |
| **eDRAM** | Render Targets / Color0 (RTV) | OK | OK | Nenhuma | **OK** | FBO Host Resolve com performance máxima |
| **Pipeline** | Pré-carregamento de Shaders (`5841144C.fbo.vk.xpso`) | OK | OK | Nenhuma | **OK** | 584 de 584 pipelines indexados no PSO cache |
| **Pipeline** | Compilação Assíncrona de Shaders | OK | OK | Mínima | **OK** | `vulkan_async_skip_incomplete_frames = true` impede travamentos de thread |
| **Performance** | Threads de Pipeline Creation | OK | OK | Nenhuma | **OK** | Ajustado para 2 threads para evitar contenção de CPU |
| **Texturas** | Texture Cache Limit | OK | OK | Nenhuma | **OK** | Soft 512MB / Hard 1024MB impede thrashing de VRAM |
| **Áudio** | AAudio Worker & XMA Decoder | OK | OK | Nenhuma | **OK** | Estável em multi-threading |
