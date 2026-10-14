#pragma once

#include <cstdint>
#include <cstdio>
#include <bit>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

namespace rex::runtime {

inline PPCRegister& GetGPR(PPCContext& ctx, int r) {
  switch (r) {
    case 0: return ctx.r0;
    case 1: return ctx.r1;
    case 2: return ctx.r2;
    case 3: return ctx.r3;
    case 4: return ctx.r4;
    case 5: return ctx.r5;
    case 6: return ctx.r6;
    case 7: return ctx.r7;
    case 8: return ctx.r8;
    case 9: return ctx.r9;
    case 10: return ctx.r10;
    case 11: return ctx.r11;
    case 12: return ctx.r12;
    case 13: return ctx.r13;
    case 14: return ctx.r14;
    case 15: return ctx.r15;
    case 16: return ctx.r16;
    case 17: return ctx.r17;
    case 18: return ctx.r18;
    case 19: return ctx.r19;
    case 20: return ctx.r20;
    case 21: return ctx.r21;
    case 22: return ctx.r22;
    case 23: return ctx.r23;
    case 24: return ctx.r24;
    case 25: return ctx.r25;
    case 26: return ctx.r26;
    case 27: return ctx.r27;
    case 28: return ctx.r28;
    case 29: return ctx.r29;
    case 30: return ctx.r30;
    case 31: return ctx.r31;
    default: return ctx.r0;
  }
}

inline PPCRegister& GetFPR(PPCContext& ctx, int f) {
  switch (f) {
    case 0: return ctx.f0;
    case 1: return ctx.f1;
    case 2: return ctx.f2;
    case 3: return ctx.f3;
    case 4: return ctx.f4;
    case 5: return ctx.f5;
    case 6: return ctx.f6;
    case 7: return ctx.f7;
    case 8: return ctx.f8;
    case 9: return ctx.f9;
    case 10: return ctx.f10;
    case 11: return ctx.f11;
    case 12: return ctx.f12;
    case 13: return ctx.f13;
    case 14: return ctx.f14;
    case 15: return ctx.f15;
    case 16: return ctx.f16;
    case 17: return ctx.f17;
    case 18: return ctx.f18;
    case 19: return ctx.f19;
    case 20: return ctx.f20;
    case 21: return ctx.f21;
    case 22: return ctx.f22;
    case 23: return ctx.f23;
    case 24: return ctx.f24;
    case 25: return ctx.f25;
    case 26: return ctx.f26;
    case 27: return ctx.f27;
    case 28: return ctx.f28;
    case 29: return ctx.f29;
    case 30: return ctx.f30;
    case 31: return ctx.f31;
    default: return ctx.f0;
  }
}

inline PPCCRRegister& GetCR(PPCContext& ctx, int crf) {
  switch (crf) {
    case 0: return ctx.cr0;
    case 1: return ctx.cr1;
    case 2: return ctx.cr2;
    case 3: return ctx.cr3;
    case 4: return ctx.cr4;
    case 5: return ctx.cr5;
    case 6: return ctx.cr6;
    case 7: return ctx.cr7;
    default: return ctx.cr0;
  }
}

inline bool GetCRBit(PPCContext& ctx, int bi) {
  int crf = (bi >> 2) & 7;
  int bit = bi & 3;
  auto& cr = GetCR(ctx, crf);
  if (bit == 0) return cr.lt != 0;
  if (bit == 1) return cr.gt != 0;
  if (bit == 2) return cr.eq != 0;
  return cr.so != 0;
}

template <typename T>
inline void SetCRField(PPCContext& ctx, int crf, T left, T right, bool is_signed) {
  auto& cr = GetCR(ctx, crf);
  if (is_signed) {
    int64_t sl = (int64_t)(int32_t)left;
    int64_t sr = (int64_t)(int32_t)right;
    cr.lt = sl < sr;
    cr.gt = sl > sr;
    cr.eq = sl == sr;
  } else {
    uint64_t ul = (uint64_t)(uint32_t)left;
    uint64_t ur = (uint64_t)(uint32_t)right;
    cr.lt = ul < ur;
    cr.gt = ul > ur;
    cr.eq = ul == ur;
  }
  cr.so = ctx.xer.so;
}

inline void UpdateCR0(PPCContext& ctx, uint32_t val) {
  int32_t s = (int32_t)val;
  ctx.cr0.lt = s < 0;
  ctx.cr0.gt = s > 0;
  ctx.cr0.eq = s == 0;
  ctx.cr0.so = ctx.xer.so;
}

void ExecutePpcThunk(PPCContext& ctx, uint8_t* base) {
  uint32_t pc = ctx.last_indirect_target;

  for (int step = 0; step < 256; ++step) {
    uint32_t op = REX_LOAD_U32(pc);
    uint32_t opcd = (op >> 26) & 0x3F;
    uint32_t rD = (op >> 21) & 0x1F;
    uint32_t rA = (op >> 16) & 0x1F;
    uint32_t rB = (op >> 11) & 0x1F;
    int16_t simm = (int16_t)(op & 0xFFFF);
    uint16_t uimm = (uint16_t)(op & 0xFFFF);

    switch (opcd) {
      case 7: { // mulli
        GetGPR(ctx, rD).u64 = (uint32_t)((int32_t)GetGPR(ctx, rA).u32 * (int32_t)simm);
        pc += 4;
        continue;
      }

      case 8: { // subfic
        uint32_t valA = GetGPR(ctx, rA).u32;
        int32_t res = (int32_t)simm - (int32_t)valA;
        GetGPR(ctx, rD).u64 = (uint32_t)res;
        ctx.xer.ca = ((uint32_t)simm >= valA);
        pc += 4;
        continue;
      }

      case 14: { // addi / li / subi
        if (rA == 0) {
          GetGPR(ctx, rD).u64 = (uint32_t)(int32_t)simm;
        } else {
          GetGPR(ctx, rD).u64 = (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        }
        pc += 4;
        continue;
      }

      case 15: { // addis / lis
        uint32_t val = (uint32_t)simm << 16;
        if (rA == 0) {
          GetGPR(ctx, rD).u64 = val;
        } else {
          GetGPR(ctx, rD).u64 = (uint32_t)(GetGPR(ctx, rA).u32 + val);
        }
        pc += 4;
        continue;
      }

      case 32: { // lwz
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        GetGPR(ctx, rD).u64 = REX_LOAD_U32(ea);
        pc += 4;
        continue;
      }

      case 34: { // lbz
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        GetGPR(ctx, rD).u64 = REX_LOAD_U8(ea);
        pc += 4;
        continue;
      }

      case 40: { // lhz
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        GetGPR(ctx, rD).u64 = REX_LOAD_U16(ea);
        pc += 4;
        continue;
      }

      case 36: { // stw
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        REX_STORE_U32(ea, GetGPR(ctx, rD).u32);
        pc += 4;
        continue;
      }

      case 38: { // stb
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        REX_STORE_U8(ea, GetGPR(ctx, rD).u8);
        pc += 4;
        continue;
      }

      case 44: { // sth
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        REX_STORE_U16(ea, GetGPR(ctx, rD).u16);
        pc += 4;
        continue;
      }

      case 24: { // ori (nop when rA=0, rS=0, uimm=0)
        GetGPR(ctx, rA).u64 = GetGPR(ctx, rD).u64 | uimm;
        pc += 4;
        continue;
      }

      case 28: { // andi.
        GetGPR(ctx, rA).u64 = GetGPR(ctx, rD).u64 & uimm;
        UpdateCR0(ctx, GetGPR(ctx, rA).u32);
        pc += 4;
        continue;
      }

      case 10: { // cmplwi
        int crf = (rD >> 2) & 7;
        SetCRField(ctx, crf, GetGPR(ctx, rA).u32, (uint32_t)uimm, false);
        pc += 4;
        continue;
      }

      case 11: { // cmpwi
        int crf = (rD >> 2) & 7;
        SetCRField(ctx, crf, (int32_t)GetGPR(ctx, rA).u32, (int32_t)simm, true);
        pc += 4;
        continue;
      }

      case 21: { // rlwinm
        uint32_t sh = (op >> 11) & 0x1F;
        uint32_t mb = (op >> 6) & 0x1F;
        uint32_t me = (op >> 1) & 0x1F;
        uint32_t rot = std::rotl(GetGPR(ctx, rD).u32, sh);
        uint32_t mask = 0;
        if (mb <= me) {
          for (uint32_t b = mb; b <= me; ++b) mask |= (1u << (31 - b));
        } else {
          for (uint32_t b = mb; b <= 31; ++b) mask |= (1u << (31 - b));
          for (uint32_t b = 0; b <= me; ++b) mask |= (1u << (31 - b));
        }
        GetGPR(ctx, rA).u64 = rot & mask;
        if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
        pc += 4;
        continue;
      }

      case 18: { // b, bl, ba, bla
        int32_t li = (int32_t)((op & 0x03FFFFFC) << 6) >> 6;
        uint32_t target = (op & 2) ? (uint32_t)li : (pc + li);
        if (op & 1) ctx.lr = pc + 4;
        if (target < pc - 0x100 || target > pc + 0x100) {
          ctx.last_indirect_target = target;
          REX_CALL_INDIRECT_FUNC(target);
          return;
        }
        pc = target;
        continue;
      }

      case 16: { // bc, bca, bcl, bcla
        uint32_t bo = (op >> 21) & 0x1F;
        uint32_t bi = (op >> 16) & 0x1F;
        int16_t bd = (int16_t)(op & 0xFFFC);
        uint32_t target = (op & 2) ? (uint32_t)bd : (pc + bd);
        bool ctr_ok = (bo & 4) != 0;
        if (!ctr_ok) {
          ctx.ctr.u64--;
          ctr_ok = ((ctx.ctr.u32 != 0) ^ ((bo & 2) != 0));
        }
        bool cond_ok = (bo & 16) != 0;
        if (!cond_ok) {
          bool cr_bit = GetCRBit(ctx, bi);
          cond_ok = (cr_bit == ((bo & 8) != 0));
        }
        if (ctr_ok && cond_ok) {
          if (op & 1) ctx.lr = pc + 4;
          if (target < pc - 0x100 || target > pc + 0x100) {
            ctx.last_indirect_target = target;
            REX_CALL_INDIRECT_FUNC(target);
            return;
          }
          pc = target;
        } else {
          pc += 4;
        }
        continue;
      }

      case 19: { // bctr / blr / bclr / bcctr
        uint32_t xo = (op >> 1) & 0x3FF;
        if (xo == 16) { // blr / bclr
          uint32_t bo = (op >> 21) & 0x1F;
          uint32_t bi = (op >> 16) & 0x1F;
          bool ctr_ok = (bo & 4) != 0;
          if (!ctr_ok) {
            ctx.ctr.u64--;
            ctr_ok = ((ctx.ctr.u32 != 0) ^ ((bo & 2) != 0));
          }
          bool cond_ok = (bo & 16) != 0;
          if (!cond_ok) {
            bool cr_bit = GetCRBit(ctx, bi);
            cond_ok = (cr_bit == ((bo & 8) != 0));
          }
          if (ctr_ok && cond_ok) {
            return;
          }
          pc += 4;
          continue;
        } else if (xo == 528) { // bctr / bcctr
          uint32_t bo = (op >> 21) & 0x1F;
          uint32_t bi = (op >> 16) & 0x1F;
          bool cond_ok = (bo & 16) != 0;
          if (!cond_ok) {
            bool cr_bit = GetCRBit(ctx, bi);
            cond_ok = (cr_bit == ((bo & 8) != 0));
          }
          if (cond_ok) {
            if (op & 1) ctx.lr = pc + 4;
            ctx.last_indirect_target = ctx.ctr.u32;
            REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
            return;
          }
          pc += 4;
          continue;
        }
        pc += 4;
        continue;
      }

      case 31: { // Extended opcode 31
        uint32_t xo = (op >> 1) & 0x3FF;
        switch (xo) {
          case 0: { // cmpw
            int crf = (rD >> 2) & 7;
            SetCRField(ctx, crf, (int32_t)GetGPR(ctx, rA).u32, (int32_t)GetGPR(ctx, rB).u32, true);
            break;
          }
          case 32: { // cmplw
            int crf = (rD >> 2) & 7;
            SetCRField(ctx, crf, GetGPR(ctx, rA).u32, GetGPR(ctx, rB).u32, false);
            break;
          }
          case 23: { // lwzx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            GetGPR(ctx, rD).u64 = REX_LOAD_U32(ea);
            break;
          }
          case 87: { // lbzx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            GetGPR(ctx, rD).u64 = REX_LOAD_U8(ea);
            break;
          }
          case 279: { // lhzx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            GetGPR(ctx, rD).u64 = REX_LOAD_U16(ea);
            break;
          }
          case 215: { // stwx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            REX_STORE_U32(ea, GetGPR(ctx, rD).u32);
            break;
          }
          case 247: { // stbx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            REX_STORE_U8(ea, GetGPR(ctx, rD).u8);
            break;
          }
          case 266: { // add
            GetGPR(ctx, rD).u64 = (uint32_t)(GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 40: { // subf
            GetGPR(ctx, rD).u64 = (uint32_t)(GetGPR(ctx, rB).u32 - GetGPR(ctx, rA).u32);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 136: { // subfe
            uint32_t notA = ~GetGPR(ctx, rA).u32;
            uint32_t b = GetGPR(ctx, rB).u32;
            uint64_t res64 = (uint64_t)notA + (uint64_t)b + (uint64_t)ctx.xer.ca;
            GetGPR(ctx, rD).u64 = (uint32_t)res64;
            ctx.xer.ca = (res64 > 0xFFFFFFFFull);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 10: { // addc
            uint64_t res64 = (uint64_t)GetGPR(ctx, rA).u32 + (uint64_t)GetGPR(ctx, rB).u32;
            GetGPR(ctx, rD).u64 = (uint32_t)res64;
            ctx.xer.ca = (res64 > 0xFFFFFFFFull);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 138: { // adde
            uint64_t res64 = (uint64_t)GetGPR(ctx, rA).u32 + (uint64_t)GetGPR(ctx, rB).u32 + (uint64_t)ctx.xer.ca;
            GetGPR(ctx, rD).u64 = (uint32_t)res64;
            ctx.xer.ca = (res64 > 0xFFFFFFFFull);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 235: { // mullw
            GetGPR(ctx, rD).u64 = (uint32_t)((int32_t)GetGPR(ctx, rA).u32 * (int32_t)GetGPR(ctx, rB).u32);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rD).u32);
            break;
          }
          case 922: // extsh
          case 491: { // extsh
            GetGPR(ctx, rA).u64 = (uint32_t)(int32_t)(int16_t)GetGPR(ctx, rD).u16;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 954: { // extsb
            GetGPR(ctx, rA).u64 = (uint32_t)(int32_t)(int8_t)GetGPR(ctx, rD).u8;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 316: { // xor
            GetGPR(ctx, rA).u64 = GetGPR(ctx, rD).u64 ^ GetGPR(ctx, rB).u64;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 444: { // or / mr
            GetGPR(ctx, rA).u64 = GetGPR(ctx, rD).u64 | GetGPR(ctx, rB).u64;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 28: { // and
            GetGPR(ctx, rA).u64 = GetGPR(ctx, rD).u64 & GetGPR(ctx, rB).u64;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 124: { // nor
            GetGPR(ctx, rA).u64 = ~(GetGPR(ctx, rD).u64 | GetGPR(ctx, rB).u64);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 467: { // mtspr
            uint32_t spr = ((op >> 11) & 0x1F) | (((op >> 16) & 0x1F) << 5);
            if (spr == 9 || spr == 288) { // CTR
              ctx.ctr.u64 = GetGPR(ctx, rD).u64;
            } else if (spr == 8 || spr == 256) { // LR
              ctx.lr = GetGPR(ctx, rD).u64;
            }
            break;
          }
          case 339: { // mfspr
            uint32_t spr = ((op >> 11) & 0x1F) | (((op >> 16) & 0x1F) << 5);
            if (spr == 9 || spr == 288) { // CTR
              GetGPR(ctx, rD).u64 = ctx.ctr.u64;
            } else if (spr == 8 || spr == 256) { // LR
              GetGPR(ctx, rD).u64 = ctx.lr;
            }
            break;
          }
          case 24: { // slw
            uint32_t sh = GetGPR(ctx, rB).u32 & 0x3F;
            GetGPR(ctx, rA).u64 = (sh < 32) ? (GetGPR(ctx, rD).u32 << sh) : 0;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 536: { // srw
            uint32_t sh = GetGPR(ctx, rB).u32 & 0x3F;
            GetGPR(ctx, rA).u64 = (sh < 32) ? (GetGPR(ctx, rD).u32 >> sh) : 0;
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 824: { // srawi
            uint32_t sh = (op >> 11) & 0x1F;
            int32_t s = (int32_t)GetGPR(ctx, rD).u32;
            GetGPR(ctx, rA).u64 = (uint32_t)(s >> sh);
            if (op & 1) UpdateCR0(ctx, GetGPR(ctx, rA).u32);
            break;
          }
          case 21: { // ldx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            GetGPR(ctx, rD).u64 = REX_LOAD_U64(ea);
            break;
          }
          case 149: { // stdx
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            REX_STORE_U64(ea, GetGPR(ctx, rD).u64);
            break;
          }
          case 341: { // lwax
            uint32_t ea = (rA == 0) ? GetGPR(ctx, rB).u32 : (GetGPR(ctx, rA).u32 + GetGPR(ctx, rB).u32);
            GetGPR(ctx, rD).s64 = (int64_t)(int32_t)REX_LOAD_U32(ea);
            break;
          }
          case 598: // sync
          case 854: { // eieio
            std::atomic_thread_fence(std::memory_order_seq_cst);
            break;
          }
          case 278: // dcbt
          case 758: // dcbtst
          case 86: { // dcbf
            // Cache line hints: no-op in software emulation
            break;
          }
          default:
            break;
        }
        pc += 4;
        continue;
      }

      case 33: { // lwzu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        GetGPR(ctx, rD).u64 = REX_LOAD_U32(ea);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 35: { // lbzu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        GetGPR(ctx, rD).u64 = REX_LOAD_U8(ea);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 37: { // stwu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        REX_STORE_U32(ea, GetGPR(ctx, rD).u32);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 39: { // stbu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        REX_STORE_U8(ea, GetGPR(ctx, rD).u8);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 41: { // lhzu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        GetGPR(ctx, rD).u64 = REX_LOAD_U16(ea);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 42: { // lha
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        GetGPR(ctx, rD).s64 = (int64_t)(int16_t)REX_LOAD_U16(ea);
        pc += 4;
        continue;
      }

      case 43: { // lhau
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        GetGPR(ctx, rD).s64 = (int64_t)(int16_t)REX_LOAD_U16(ea);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 45: { // sthu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        REX_STORE_U16(ea, GetGPR(ctx, rD).u16);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 48: { // lfs
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        uint32_t u = REX_LOAD_U32(ea);
        GetFPR(ctx, rD).f64 = (double)std::bit_cast<float>(u);
        pc += 4;
        continue;
      }

      case 49: { // lfsu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        uint32_t u = REX_LOAD_U32(ea);
        GetFPR(ctx, rD).f64 = (double)std::bit_cast<float>(u);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 50: { // lfd
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        uint64_t u = REX_LOAD_U64(ea);
        GetFPR(ctx, rD).f64 = std::bit_cast<double>(u);
        pc += 4;
        continue;
      }

      case 51: { // lfdu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        uint64_t u = REX_LOAD_U64(ea);
        GetFPR(ctx, rD).f64 = std::bit_cast<double>(u);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 52: { // stfs
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        float f = (float)GetFPR(ctx, rD).f64;
        REX_STORE_U32(ea, std::bit_cast<uint32_t>(f));
        pc += 4;
        continue;
      }

      case 53: { // stfsu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        float f = (float)GetFPR(ctx, rD).f64;
        REX_STORE_U32(ea, std::bit_cast<uint32_t>(f));
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 54: { // stfd
        uint32_t ea = (rA == 0) ? (uint32_t)simm : (uint32_t)(GetGPR(ctx, rA).u32 + simm);
        uint64_t u = std::bit_cast<uint64_t>(GetFPR(ctx, rD).f64);
        REX_STORE_U64(ea, u);
        pc += 4;
        continue;
      }

      case 55: { // stfdu
        uint32_t ea = GetGPR(ctx, rA).u32 + simm;
        uint64_t u = std::bit_cast<uint64_t>(GetFPR(ctx, rD).f64);
        REX_STORE_U64(ea, u);
        GetGPR(ctx, rA).u64 = ea;
        pc += 4;
        continue;
      }

      case 58: { // ld, ldu, lwa (DS-form: opcd[6], rD[5], rA[5], ds[14], xo[2])
        uint32_t xo = op & 3;
        int32_t ds = (int32_t)((int16_t)(op & 0xFFFC));
        uint32_t ea = (rA == 0) ? (uint32_t)ds : (uint32_t)(GetGPR(ctx, rA).u32 + ds);
        if (xo == 0) { // ld
          GetGPR(ctx, rD).u64 = REX_LOAD_U64(ea);
        } else if (xo == 1) { // ldu
          GetGPR(ctx, rD).u64 = REX_LOAD_U64(ea);
          GetGPR(ctx, rA).u64 = ea;
        } else if (xo == 2) { // lwa
          GetGPR(ctx, rD).s64 = (int64_t)(int32_t)REX_LOAD_U32(ea);
        }
        pc += 4;
        continue;
      }

      case 62: { // std, stdu (DS-form)
        uint32_t xo = op & 3;
        int32_t ds = (int32_t)((int16_t)(op & 0xFFFC));
        uint32_t ea = (rA == 0) ? (uint32_t)ds : (uint32_t)(GetGPR(ctx, rA).u32 + ds);
        if (xo == 0) { // std
          REX_STORE_U64(ea, GetGPR(ctx, rD).u64);
        } else if (xo == 1) { // stdu
          REX_STORE_U64(ea, GetGPR(ctx, rD).u64);
          GetGPR(ctx, rA).u64 = ea;
        }
        pc += 4;
        continue;
      }

      case 63: { // fmr
        uint32_t xo = (op >> 1) & 0x3FF;
        if (xo == 72) {
          GetFPR(ctx, rD).f64 = GetFPR(ctx, rB).f64;
        }
        pc += 4;
        continue;
      }

      case 4: { // AltiVec / VMX vector ops - advance pc silently without terminal I/O latency
        pc += 4;
        continue;
      }

      default:
        // Silently advance pc: avoids devastating I/O throughput drops from printf on unhandled ops
        pc += 4;
        continue;
    }
  }
}

} // namespace rex::runtime
