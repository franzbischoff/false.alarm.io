# false.alarm.io — Copilot Memory

## Última atualização
- Data: 2026-02-28

## Contexto atual
- Projeto em PlatformIO com `framework = espidf` no ambiente principal `esp32idf` (ESP-IDF 5.5.3).
- **Compilador**: GCC 14.2.0 (supports C++11, 14, 17, 20, 23).
- **C++ Standard**: Atualizado de C++11 para **C++17** ✅
- Limpeza recente concluída: remoção de artefatos Gitpod/DevContainer e arquivamento de diretórios legados em `archive/`.
- **Auditoria e compatibilização com ESP-IDF 5.x completada**: todas as correções aplicadas e validadas com build bem-sucedido.

## Status de Compatibilidade ESP-IDF 5.x e C++17
✅ **COMPLETO** - Todas as correções aplicadas e testadas:
1. ✅ Atualizado `idf_component.yml`: `idf: ">=5.0"` (estava ">=4.1")
2. ✅ Limpeza de `Mpx.hpp`: melhor documentação das branches de compilação condicionais
3. ✅ Validação de `sdkconfig.defaults`:
   - Remover `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (não necessário para app de streaming)
   - Mantidos: `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP` com comentário explicativo
4. ✅ Limpeza de `platformio.ini`: removidas linhas comentadas de dependências obsoletas
5. ✅ Build final (ESP-IDF 5.x): SUCCESS - firmware.bin gerado com 23.9% Flash, 4.2% RAM

## Modernização C++11 → C++17 ✅
✅ **COMPLETO** - 4 alterações principais aplicadas com sucesso:
1. ✅ `.clang-format`: `Standard: c++11` → `Standard: c++17`
2. ✅ `platformio.ini`: cppcheck flag `--std=c++11` → `--std=c++17`
3. ✅ `Mpx.hpp`: Removidas macros `MIN/MAX`, adicionado `#include <algorithm>` para `std::min<>/std::max<>`
4. ✅ `Mpx.cpp`: Substituído `calloc()` por `std::make_unique<T[]>()` (RAII) + padronizado `static_cast<>`
5. ✅ Build final (C++17): SUCCESS - firmware.bin gerado com 24.0% Flash, 4.2% RAM (ligeiro aumento esperado)

## Futuras Melhorias de C++17+ (Não Urgent)
Identificadas durante auditoria de modernização, preservam design atual (RAII + raw pointers por previsibilidade):

### 1. Modernizar getters de `Mpx.hpp`
- Contexto: Classes como `get_data_buffer()`, `get_vmatrix_profile()` retornam `float*` (raw pointers).
- Opções futuras:
  - Considerar `std::span<T>` (C++20) para views não-owning
  - Alternativa C++17: retornar `const float*` com métodos size() companheiros
  - **Nota**: Design atual é intencional para embedded (memória previsível)

### 2. Consistência `nullptr` vs `NULL`
- Contexto: Código mistura `NULL` e `nullptr`.
- Ação recomendada: Padronizar para `nullptr` em todo codebase.
- Files afetados: `Mpx.cpp`, `main.cpp`, `esp_littlefs/*`

### 3. Inicialização de Valores
- Padrões atuais mistos: `= 0U`, `= 0`, `= {}`, `= {{}`.
- Ação: Unificar em padrão consistente (e.g., `= 0U` para escalares).

### 4. Aplicações de `constexpr`
- Considerar expor `WINDOW_SIZE`, `SAMPLING_RATE_HZ` como `inline constexpr` em namespace.
- Benefício: Type-safety em compile-time.

### 5. Segurança de Memória
- Status: Em progresso com `std::make_unique<>` em `Mpx.cpp`.
- Próximo: Considerar `std::unique_ptr<T[]>` vs `.release()` se design permitir.

## Checklist de Otimizações de Performance (Para Implementação Futura)
Salvo para quando performance se tornar crítica:

1. [ ] Baseline: Medir CPU load por task, stack high-water mark, heap, latência.
2. [ ] Release build: Comparar `-O2` vs `-Os`, manter o melhor.
3. [ ] Reduzir logging nos caminhos críticos.
4. [ ] FreeRTOS tuning: Revisar priorities, stack sizes, queue capacity.
5. [ ] Memória previsível: Confirmar pré-alocação, evitar malloc em loops.
6. [ ] Isolar I/O: Separar `FILE_DATA=1` do fluxo real durante benchmark.
7. [ ] Sincronização: Reduzir cópias/locks desnecessários.
8. [ ] Enxugar sdkconfig: Desabilitar resources não-usados.
9. [ ] Controlar clock: Fixar frequência de CPU durante medição.
10. [ ] Validar: Múltiplas execuções para significância estatística.

## Próximo Passo Acordado
- Aplicar posteriormente implementação mínima focada em: release build, redução de logging, instrumentação objetiva.
