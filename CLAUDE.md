# Toolscreen

## Язык общения

Мы говорим по-русски. Все ответы, комментарии и документация должны быть на русском языке.

## Главное правило

> **НИКАКИХ ПРАВОК КОДА БЕЗ УТВЕРЖДЁННОГО ПЛАНА.**
>
> 1. `EnterPlanMode` → исследуем → пишем план.
> 2. Пользователь утверждает план.
> 3. Только после этого вносим изменения в код.
>
> Любое изменение файлов (включая однострочные) без утверждённого плана **запрещено**.

## Workflow

- **Ревью (code review) делается без изменений файлов** — только чтение и анализ. Никаких правок кода во время ревью.
- План должен описывать: какие файлы меняются, зачем, архитектуру изменений, как проверять результат.
- **После каждого патча — безусловный коммит.** Как только изменения готовы и проверены, сразу `git add -A && git commit`. Без ожидания дополнительных подтверждений.
- **fprintf(stderr) НЕ РАБОТАЕТ в Minecraft.** Всегда используй HOOK_LOG() или TS_LOG() — они пишут в файл /home/user/toolscreen.log. stderr Minecraft выкидывает.
- **ТОЛЬКО ИНЖЕКТОР.** Никаких подмен Java-файлов, никаких LD_PRELOAD-обёрток. Только `sudo ./scripts/inject.sh`.
- **Сборка всегда с `--parallel $(nproc)`.** `cmake --build out/build/linux-test --parallel $(nproc)`

## Инжектор и отладка

- **Краш происходит сразу после возврата в игру после инжекта.** Сообщения в логах о кадрах (Frame N) до возврата в игру не показательны — они от фонового рендера, а краш происходит на первом «настоящем» кадре после возврата в игру.
- **Лог-файлы:** `/home/user/toolscreen.log` (HOOK_LOG, TS_LOG, X11_LOG), `/home/user/toolscreen_trace.log` (TRACE_CALL, TS_TRACE, DBG_TRACE).
- **stderr не работает** — Minecraft/JVM его уничтожает. Все отладочные сообщения только в файлы логов.
- **Хук должен работать на любой дистанции** — использовать двухшаговый подход: bridge-страница рядом с target (<2GB) для 5-байтового атомарного jmp rel32, в bridge — 14-байтовый absolute jump без ограничения дистанции.
- **SIGSEGV в libnvidia-glcore.so** при вызове ImGui_ImplOpenGL3_RenderDrawData — даже с сохранением/восстановлением GL-стейта. Без RenderDrawData игра стабильна.
- **Sodium** (оптимизатор рендера Minecraft) агрессивно кеширует GL-стейт и чувствителен к посторонним GL-вызовам.
- **Механизм хука**: патч dispatch-таблицы libGL (НЕ кода). `glXSwapBuffers` делает `mov offset(%rip),%rax; jmp *0x118(%rax)`. Мы парсим `mov`, вычисляем адрес таблицы, заменяем `table[0x118/8]` на `hk_glXSwapBuffers`. Оригинальный указатель сохраняется в `g_realSwapBuffers` для прямого вызова без рекурсии. mprotect только в constructor (процесс заморожен gdb — безопасно). В рантайме mprotect НЕ вызывается.
- **Первый успешный ImGui-рендер**: достигнут в коммите `45511da` путём замены ImGui-шного RenderDrawData на собственный шейдер с ортографической проекцией. ImDrawVert имеет layout: `pos` (2 float), `uv` (2 float), `col` (4 байта BGRA). Результат: зелёные полосы, образующие фигуру похожую на прямоугольник — искажения из-за неправильного порядка вершинных атрибутов. Но это первый случай когда ImGui-геометрия реально отобразилась на экране.

## Платформа — проверка регресса рендера

Минимальный тест что GL-рендер работает после инжекта (красный квадрат в центре экрана):

1. Создать шейдер (vertex + fragment), VAO, VBO с 6 вершинами (2 треугольника)
2. Каждый кадр: `glBindFramebuffer(DRAW_FRAMEBUFFER, 0)`, `glBindBuffer(PIXEL_UNPACK_BUFFER, 0)`, сбросить `GL_UNPACK_*` в дефолт
3. `glEnable(GL_BLEND)`, `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`
4. `glUseProgram`, `glBindVertexArray`, `glDrawArrays(GL_TRIANGLES, 0, 6)`
5. После отрисовки: unbind VAO, program, disable BLEND

Ключевые требования для работы GL на фоне Sodium:
- **`glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0)`** — Sodium оставляет кастомный FBO
- **`glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0)`** — Sodium оставляет PBO, иначе `glTexImage2D` падает (интерпретирует pixels как offset)
- **`glPixelStorei(GL_UNPACK_ROW_LENGTH, 0)`** — Sodium выставляет свои значения
- **`glPixelStorei(GL_UNPACK_ALIGNMENT, 4)`** — дефолт
- Без этих сбросов — либо краш (SIGSEGV в libnvidia-glcore.so), либо невидимый рендер
- **`glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0)`** — Sodium использует PBO для загрузки текстур, иначе ImGui font upload падает
- **`glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0)` и `SKIP_ROWS, 0`** — тоже нужно сбрасывать

### Первый успешный ImGui-рендер (зелёный прямоугольник)

Достигнут в коммите `45511da`. Вместо `ImGui_ImplOpenGL3_RenderDrawData` используется собственный шейдер с ортографической проекцией, который рендерит геометрию из `ImDrawData`:

1. `ImGui::Render()` → `ImDrawData* dd = ImGui::GetDrawData()`
2. Для каждого `ImDrawList`: создать VAO/VBO/EBO, загрузить `VtxBuffer`/`IdxBuffer`
3. Vertex attribs: `pos` (2 float, offset 0), `uv` (2 float, offset 8), `col` (4 байта, offset 16)
4. Проекция: `Proj * vec4(p,0,1)` где Proj = ортогональная матрица из пикселей в NDC
5. `glDrawElements` с нашим шейдером (пропускает цвет вершины)

Это первый случай когда геометрия ImGui отобразилась на экране.
