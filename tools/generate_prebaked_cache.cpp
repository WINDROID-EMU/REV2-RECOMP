#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <cstdint>
#include <chrono>
#include <rex/string/buffer.h>

// Offsets in linux-amd64/lib/librexgpu-xenos.so
constexpr uintptr_t kOff_SpirvShader_ctor       = 0x000c4c30;
constexpr uintptr_t kOff_SpirvShader_dtor       = 0x002ce780;
constexpr uintptr_t kOff_Shader_GetOrCreateTrans = 0x000931c0;
constexpr uintptr_t kOff_Shader_AnalyzeUcode     = 0x000bafc0;
constexpr uintptr_t kOff_ShaderTrans_ctor        = 0x000bedf0;
constexpr uintptr_t kOff_ShaderTrans_Translate   = 0x000bf080;
constexpr uintptr_t kOff_Features_ctor           = 0x000c4d20;
constexpr uintptr_t kOff_SpirvShaderTrans_vtable = 0x005439e0;

// Function types
using Fn_Features_ctor = void (*)(void* this_ptr, bool all);
using Fn_SpirvShader_ctor = void (*)(void* this_ptr, uint32_t shader_type, uint64_t ucode_hash, const uint32_t* ucode, size_t dwords, int endian);
using Fn_SpirvShader_dtor = void (*)(void* this_ptr);
using Fn_Shader_GetOrCreateTrans = void* (*)(void* this_ptr, uint64_t mod, bool* is_new);
using Fn_Shader_AnalyzeUcode = void (*)(void* this_ptr, rex::string::StringBuffer& string_buffer);
using Fn_ShaderTrans_ctor = void (*)(void* this_ptr);
using Fn_ShaderTrans_Translate = bool (*)(void* this_ptr, void* translation);

struct CacheEntry {
    uint64_t ucode_hash;
    uint32_t shader_type;
    std::vector<uint32_t> tex_bindings;
    std::vector<uint32_t> sam_bindings;
    std::vector<uint8_t> spirv;
};

int main(int argc, char** argv) {
    std::string xsh_path = ".android_tmp/5841144C.xsh";
    std::string out_path = "5841144C_prebaked.spvcache";
    if (argc >= 2) xsh_path = argv[1];
    if (argc >= 3) out_path = argv[2];

    std::cout << "============================================================" << std::endl;
    std::cout << "     REREV2 Offline AOT SPIR-V Prebake Compiler Tool        " << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "[*] Input XSH:  " << xsh_path << std::endl;
    std::cout << "[*] Output SPV: " << out_path << std::endl;

    void* handle = dlopen("linux-amd64/lib/librexgpu-xenos.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "[-] Error: dlopen failed: " << dlerror() << std::endl;
        return 1;
    }

    struct link_map* map = nullptr;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || !map) {
        std::cerr << "[-] Error: dlinfo failed: " << dlerror() << std::endl;
        return 1;
    }
    uintptr_t base = map->l_addr;
    std::cout << "[+] Loaded librexgpu-xenos.so at base 0x" << std::hex << base << std::dec << std::endl;

    auto fn_Features_ctor = reinterpret_cast<Fn_Features_ctor>(base + kOff_Features_ctor);
    auto fn_SpirvShader_ctor = reinterpret_cast<Fn_SpirvShader_ctor>(base + kOff_SpirvShader_ctor);
    auto fn_SpirvShader_dtor = reinterpret_cast<Fn_SpirvShader_dtor>(base + kOff_SpirvShader_dtor);
    auto fn_GetOrCreateTrans = reinterpret_cast<Fn_Shader_GetOrCreateTrans>(base + kOff_Shader_GetOrCreateTrans);
    auto fn_AnalyzeUcode = reinterpret_cast<Fn_Shader_AnalyzeUcode>(base + kOff_Shader_AnalyzeUcode);
    auto fn_ShaderTrans_ctor = reinterpret_cast<Fn_ShaderTrans_ctor>(base + kOff_ShaderTrans_ctor);
    auto fn_Translate = reinterpret_cast<Fn_ShaderTrans_Translate>(base + kOff_ShaderTrans_Translate);

    // Setup reusable SpirvShaderTranslator instance
    std::vector<uint8_t> trans_mem(0x400, 0);
    fn_ShaderTrans_ctor(trans_mem.data());
    *reinterpret_cast<uintptr_t*>(trans_mem.data()) = base + kOff_SpirvShaderTrans_vtable + 0x10;
    fn_Features_ctor(trans_mem.data() + 0x3c, true);
    // Force SPIR-V 1.3
    *reinterpret_cast<uint32_t*>(trans_mem.data() + 0x3c) = 0x00010300;
    trans_mem[0x50] = 0; // native_2x_msaa_with_attachments_ = false
    trans_mem[0x51] = 0; // native_2x_msaa_no_attachments_ = false
    trans_mem[0x52] = 0; // edram_fragment_shader_interlock_ = false
    *reinterpret_cast<uint32_t*>(trans_mem.data() + 0x54) = 1; // scale_x = 1
    *reinterpret_cast<uint32_t*>(trans_mem.data() + 0x58) = 1; // scale_y = 1

    // Read XSH file
    std::ifstream file(xsh_path, std::ios::binary);
    if (!file) {
        std::cerr << "[-] Error: Could not open " << xsh_path << std::endl;
        return 1;
    }
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(file_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        std::cerr << "[-] Error: Failed to read " << xsh_path << std::endl;
        return 1;
    }
    file.close();

    if (buffer.size() < 8 || std::memcmp(buffer.data(), "XESH  \x12\x19", 8) != 0) {
        std::cerr << "[-] Error: Invalid XSH magic header!" << std::endl;
        return 1;
    }

    std::vector<CacheEntry> entries;
    size_t offset = 8;
    int shader_idx = 0;
    int success_count = 0;
    int fail_count = 0;
    rex::string::StringBuffer disasm_buf(8192);

    auto t0 = std::chrono::high_resolution_clock::now();

    while (offset + 12 <= buffer.size()) {
        uint64_t ucode_hash = 0;
        uint32_t dword_info = 0;
        std::memcpy(&ucode_hash, buffer.data() + offset, 8);
        std::memcpy(&dword_info, buffer.data() + offset + 8, 4);

        uint32_t shader_type = (dword_info >> 31) ? 1 : 0;
        uint32_t dword_count = dword_info & 0x7FFFFFFF;
        size_t byte_count = dword_count * 4;

        if (offset + 12 + byte_count > buffer.size()) {
            std::cerr << "[-] Truncated shader entry at index " << shader_idx << std::endl;
            break;
        }

        const uint32_t* ucode_ptr = reinterpret_cast<const uint32_t*>(buffer.data() + offset + 12);

        try {
            std::vector<uint8_t> shader_mem(0x300, 0);
            fn_SpirvShader_ctor(shader_mem.data(), shader_type, ucode_hash, ucode_ptr, dword_count, 2 /* big endian */);

            disasm_buf.Reset();
            fn_AnalyzeUcode(shader_mem.data(), disasm_buf);

            bool is_new = false;
            void* translation = fn_GetOrCreateTrans(shader_mem.data(), 0, &is_new);

            if (translation && fn_Translate(trans_mem.data(), translation)) {
                bool is_valid = *reinterpret_cast<uint8_t*>((char*)translation + 0x18);
                bool is_translated = *reinterpret_cast<uint8_t*>((char*)translation + 0x19);

                uint8_t* bin_data = *reinterpret_cast<uint8_t**>((char*)translation + 0x38);
                uint8_t* bin_end  = *reinterpret_cast<uint8_t**>((char*)translation + 0x40);
                size_t bin_size = bin_end - bin_data;

                if (is_valid && is_translated && bin_size >= 20) {
                    CacheEntry entry;
                    entry.ucode_hash = ucode_hash;
                    entry.shader_type = shader_type;
                    entry.spirv.assign(bin_data, bin_end);

                    // Extract texture bindings
                    uint32_t* tex_begin = *reinterpret_cast<uint32_t**>((char*)shader_mem.data() + 0x1b8);
                    uint32_t* tex_end   = *reinterpret_cast<uint32_t**>((char*)shader_mem.data() + 0x1c0);
                    if (tex_end > tex_begin) {
                        entry.tex_bindings.assign(tex_begin, tex_end);
                    }

                    // Extract sampler bindings
                    uint32_t* sam_begin = *reinterpret_cast<uint32_t**>((char*)shader_mem.data() + 0x1d0);
                    uint32_t* sam_end   = *reinterpret_cast<uint32_t**>((char*)shader_mem.data() + 0x1d8);
                    if (sam_end > sam_begin) {
                        entry.sam_bindings.assign(sam_begin, sam_end);
                    }

                    entries.push_back(std::move(entry));
                    success_count++;
                } else {
                    fail_count++;
                }
            } else {
                fail_count++;
            }

            fn_SpirvShader_dtor(shader_mem.data());
        } catch (const std::exception& e) {
            std::cerr << "[-] Exception translating shader " << shader_idx << ": " << e.what() << std::endl;
            fail_count++;
        }

        offset += 12 + byte_count;
        shader_idx++;

        if (shader_idx % 100 == 0 || offset + 12 > buffer.size()) {
            std::cout << "[*] Processed " << shader_idx << " shaders (" << success_count << " compiled, " << fail_count << " failed)..." << std::endl;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "[+] Finished translation in " << elapsed_ms << " ms!" << std::endl;
    std::cout << "[+] Results: " << success_count << " compiled, " << fail_count << " failed." << std::endl;

    if (entries.empty()) {
        std::cerr << "[-] No shaders compiled successfully!" << std::endl;
        return 1;
    }

    // Write binary REXS prebaked spvcache format
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::cerr << "[-] Error opening output file: " << out_path << std::endl;
        return 1;
    }

    uint32_t magic = 0x53584552;      // "REXS"
    uint32_t version = 1;
    uint32_t title_id = 0x5841144C;   // Resident Evil Revelations 2
    uint32_t count = static_cast<uint32_t>(entries.size());

    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&title_id), 4);
    out.write(reinterpret_cast<const char*>(&count), 4);

    size_t total_spirv_bytes = 0;
    for (const auto& s : entries) {
        out.write(reinterpret_cast<const char*>(&s.ucode_hash), 8);
        out.write(reinterpret_cast<const char*>(&s.shader_type), 4);

        uint32_t tex_count = static_cast<uint32_t>(s.tex_bindings.size());
        out.write(reinterpret_cast<const char*>(&tex_count), 4);
        if (tex_count > 0) {
            out.write(reinterpret_cast<const char*>(s.tex_bindings.data()), tex_count * 4);
        }

        uint32_t sam_count = static_cast<uint32_t>(s.sam_bindings.size());
        out.write(reinterpret_cast<const char*>(&sam_count), 4);
        if (sam_count > 0) {
            out.write(reinterpret_cast<const char*>(s.sam_bindings.data()), sam_count * 4);
        }

        uint32_t spv_size = static_cast<uint32_t>(s.spirv.size());
        out.write(reinterpret_cast<const char*>(&spv_size), 4);
        out.write(reinterpret_cast<const char*>(s.spirv.data()), spv_size);
        total_spirv_bytes += spv_size;
    }
    out.close();

    std::cout << "[✓] Successfully generated " << out_path << " (" << count << " shaders, " << (total_spirv_bytes / 1024) << " KB SPIR-V)!" << std::endl;
    return 0;
}
