#pragma once
#include <cstddef>
#include <cstdint>

// Возвращает минимальное количество байт, покрывающих ≥ minBytes инструкций
// в x86-64 коде. Если не удаётся декодировать — возвращает 0.
// Используется для вычисления backup-размера в inline-хуках (CreateHook).
size_t X86InsnMinCover(const uint8_t* code, size_t minBytes, size_t maxLen);
