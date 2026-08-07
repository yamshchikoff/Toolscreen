# Подходы к viewport clamping на Linux

## Контекст

Windows Toolscreen перехватывает **три** GL-функции для viewport clamping:

| Функция | Зачем |
|---------|-------|
| `hkglViewport` | Перехват `glViewport` — подмена размеров вьюпорта |
| `hkglBlitFramebuffer` | Перехват `glBlitFramebuffer` — подмена src/dst прямоугольников при блите FBO→экран |
| `hkglNamedFramebufferTexture` | Перехват привязки текстур к FBO |

На Linux ни одна из них не работает через LD_PRELOAD — LWJGL резолвит function pointers через `dlsym` в обход PLT.

## Что мы знаем

- Майнкрафт **не вызывает** `glViewport` в обычном кадре — ни на Linux, ни на Windows (доказано GDB dprintf)
- Майнкрафт рендерит в свой FBO, затем блиттит на экран через `glBlitFramebuffer`
- `glViewport` не влияет на `glBlitFramebuffer` — у него свои src/dst параметры
- `glXSwapBuffers` успешно перехвачен через патч dispatch-таблицы
- `glBlitFramebuffer` в сошке объявлен через LD_PRELOAD (`glx_hook.cpp`), но неизвестно перехватывается ли он реально

## Что проверить

1. Вызывается ли `glBlitFramebuffer` из Майнкрафта? (GDB dprintf)
2. Работает ли LD_PRELOAD для `glBlitFramebuffer`?
3. Можно ли запатчить dispatch-таблицу для `glBlitFramebuffer` аналогично `glXSwapBuffers`?

## Механизм Windows (из `dllmain.cpp`)

### hkglViewport (строка 1791)
Пассивный перехватчик. При вызове `glViewport` из Майнкрафта или нашего кода подменяет width/height на размеры активного режима из `CachedModeViewport`.

### hkglBlitFramebuffer (строка 1854)
Перехватывает блит FBO→экран. Это критично потому что `glBlitFramebuffer` не использует viewport — у него явные src/dst координаты.

### hkglNamedFramebufferTexture (строка 1884)
Перехватывает привязку текстур к FBO для отслеживания игрового framebuffer'а.
