#include "x86_length_disasm.h"

namespace {

// Декодирует длину одной x86-64 инструкции. Возвращает 0 при ошибке.
size_t X86InsnLength(const uint8_t* code, size_t maxLen) {
    if (!code || maxLen < 1) return 0;

    size_t pos = 0;

    // ---- Legacy & REX prefixes ----
    bool hasRex = false;
    while (pos < maxLen) {
        uint8_t b = code[pos];
        // REX prefix (0x40-0x4F)
        if (b >= 0x40 && b <= 0x4F) {
            hasRex = true;
            ++pos;
            continue;
        }
        // Segment override (CS/DS/ES/FS/GS/SS)
        if (b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E ||
            b == 0x64 || b == 0x65) { ++pos; continue; }
        // Address size / operand size override
        if (b == 0x66 || b == 0x67) { ++pos; continue; }
        // LOCK, REPNE, REP/REPE
        if (b == 0xF0 || b == 0xF2 || b == 0xF3) { ++pos; continue; }
        break;
    }

    if (pos >= maxLen) return 0;

    // ---- Opcode ----
    uint8_t op = code[pos++];
    bool is2byte = (op == 0x0F);
    bool is3byte = false;

    if (is2byte) {
        if (pos >= maxLen) return 0;
        op = code[pos++];
        // 3-byte opcode: 0F 38 xx  or  0F 3A xx
        if (op == 0x38 || op == 0x3A) {
            if (pos >= maxLen) return 0;
            op = code[pos++];
            is3byte = true;
        }
    }

    // ---- ModR/M ----
    // Most instructions with a ModR/M byte: opcodes that are NOT
    // in the "no ModR/M" set (push r64, jmp rel, etc.)
    bool hasModRM = false;
    uint8_t modrm = 0;

    // Helper: is opcode in ModR/M-free range
    auto noModRM = [](uint8_t o, bool is2b, bool is3b) -> bool {
        if (is3b) return false; // 3-byte opcodes always have ModR/M
        if (is2b) return false; // 2-byte 0F opcodes have ModR/M
        // 1-byte opcodes without ModR/M:
        // push r64 (50-57), pop r64 (58-5F), short Jcc (70-7F),
        // mov imm (B0-BF), int3 (CC), etc.
        if ((o >= 0x50 && o <= 0x5F) || (o >= 0x70 && o <= 0x7F) ||
            (o >= 0xB0 && o <= 0xBF) || (o >= 0xC0 && o <= 0xC1) ||
            (o >= 0xD0 && o <= 0xD7) || (o >= 0xE0 && o <= 0xE3) ||
            o == 0xE8 || o == 0xE9 || o == 0xEA || o == 0xEB ||
            o == 0x6A || o == 0xCD || o == 0xCF ||
            o == 0x9A || o == 0xC2 || o == 0xC3 || o == 0xCA || o == 0xCB) {
            return true;
        }
        return false;
    };

    if (!noModRM(op, is2byte, is3byte)) {
        if (pos >= maxLen) return 0;
        modrm = code[pos++];
        hasModRM = true;
    }

    // ---- SIB ----
    bool hasSIB = false;
    if (hasModRM) {
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm  = modrm & 7;
        // SIB present if Mod != 11 and R/M == 100
        if (mod != 3 && rm == 4) {
            if (pos >= maxLen) return 0;
            ++pos; // skip SIB
            hasSIB = true;
        }
    }

    // ---- Displacement ----
    if (hasModRM) {
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm  = modrm & 7;

        if (mod == 1) {
            // disp8
            if (pos >= maxLen) return 0;
            ++pos;
        } else if (mod == 2) {
            // disp32
            if (pos + 4 > maxLen) return 0;
            pos += 4;
        } else if (mod == 0 && rm == 5) {
            // RIP-relative (Mod=00, R/M=101) — disp32
            if (pos + 4 > maxLen) return 0;
            pos += 4;
        }
        // Mod=00 with SIB and base=101 — disp32
        if (mod == 0 && hasSIB) {
            // Need to check SIB.base — but SIB is already consumed.
            // Re-read SIB byte at pos-1 (if no displacement consumed yet)
            // Actually, the SIB byte is at code[pos-1].
            // If SIB.base == 5 (rbp/r13) and mod == 0, there's a disp32.
            // But we already skipped the SIB byte. Let's recover:
            uint8_t sib = code[pos - 1];
            uint8_t base = sib & 7;
            if (base == 5) {
                if (pos + 4 > maxLen) return 0;
                pos += 4;
            }
        }
    }

    // ---- Immediate ----
    // Determine immediate size based on opcode
    size_t immSize = 0;

    auto immSizeForOp = [&](uint8_t o, bool is2b) -> size_t {
        // 1-byte opcodes with imm8
        if (!is2b) {
            switch (o) {
                case 0x6A: return 1; // push imm8
                case 0x70: case 0x71: case 0x72: case 0x73: // Jcc rel8
                case 0x74: case 0x75: case 0x76: case 0x77:
                case 0x78: case 0x79: case 0x7A: case 0x7B:
                case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                    return 1;
                case 0x80: // Grp1 r/m8, imm8 (ModR/M.reg determines operation)
                case 0x82: // Grp1 r/m8, imm8 (obsolete alias of 80)
                case 0xC0: case 0xC1: return 1; // shift r/m, imm8
                case 0x83: {
                    // Grp1 r/m, imm8 — but REX.W makes operand 64-bit
                    // Immediate is sign-extended imm8
                    return 1;
                }
                case 0xB0: case 0xB1: case 0xB2: case 0xB3: // MOV r8, imm8
                case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                    return 1;
                case 0xA8: return 1; // TEST AL, imm8
                case 0xC6: {
                    // MOV r/m8, imm8 (when ModR/M.reg == 0)
                    if (hasModRM) {
                        uint8_t reg = (modrm >> 3) & 7;
                        if (reg == 0) return 1;
                    }
                    return 0;
                }
                case 0xC7: {
                    // MOV r/m, imm32 (when ModR/M.reg == 0)
                    if (hasModRM) {
                        uint8_t reg = (modrm >> 3) & 7;
                        if (reg == 0) return hasRex ? 4 : 4; // always imm32 for C7
                    }
                    return 0;
                }
                default: return 0;
            }
        }
        // 2-byte opcodes with immediates
        return 0;
    };

    immSize = immSizeForOp(op, is2byte && !is3byte);

    // Common imm32 cases (opcode-driven)
    if (!is2byte && !is3byte) {
        uint8_t o = op;
        // MOV r, imm64 with REX.W
        if (o >= 0xB8 && o <= 0xBF && hasRex) {
            immSize = 8;
        } else if (o >= 0xB8 && o <= 0xBF && !hasRex) {
            immSize = 4;
        }
        // imm32 for 68 (push imm32)
        if (o == 0x68) immSize = 4;
        // imm32 for C7 (was not caught above)
        if (o == 0xC7 && hasModRM) {
            uint8_t reg = (modrm >> 3) & 7;
            if (reg == 0) immSize = 4;
        }
    }

    if (immSize > 0) {
        if (pos + immSize > maxLen) return 0;
        pos += immSize;
    }

    return pos;
}

} // namespace

size_t X86InsnMinCover(const uint8_t* code, size_t minBytes, size_t maxLen) {
    if (!code || minBytes == 0 || maxLen == 0) return 0;

    size_t total = 0;
    size_t offset = 0;

    while (total < minBytes && offset < maxLen) {
        size_t insnLen = X86InsnLength(code + offset, maxLen - offset);
        if (insnLen == 0) return 0; // ошибка декодирования
        total += insnLen;
        offset += insnLen;
    }

    return total;
}
