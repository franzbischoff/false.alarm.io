# false.alarm.io — Copilot Memory

## Última atualização
- Data: 2026-03-09

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

## Teste de Robustez do Mpx (Atualizado 2026-03-09)
✅ **COMPLETO** - Framework de testes atualizado e validado com nova versão da biblioteca

### Status: 20/20 testes passando ✅
- **Ambiente**: Native (x86-64 desktop), compilado com Unity framework
- **Tempo total**: ~26.55 segundos
- **Arquivos de teste**:
  - `test/test_mpx.cpp`: 3 testes funcionais básicos
  - `test/test_mpx_robustness.cpp`: 15 testes de robustez e invariantes
  - `test/test_mpx_golden.cpp`: 2 testes de regressão com golden reference (NOVO)
  - `test/TEST_STRATEGY.md`: Documentação de validação

### Mudanças na Biblioteca Mpx (2026-03-09)
1. **prune_buffer()**: Alterado de random walk para padrão senoidal determinístico
   - Antes: `data_buffer_[0] = 0.001F` + random walk
   - Agora: `data_buffer_[i] = sinf(2*pi*i/100)` para resultados reproduzíveis
   - **Impacto nos testes**: `test_mpx_prune_buffer_invariants` atualizado para esperar `data[0] = 0.0F`

2. **floss_iac_()**: Implementada distribuição analítica de Kumaraswamy
   - Substituiu simulação Monte Carlo por fórmula analítica determinística
   - Parâmetros: a=1.939274, b=1.698150, normalização=4.035477
   - **Benefício**: Resultados consistentes sem aleatoriedade

### Testes Core Passando (20/20) ✅
**Testes Funcionais Básicos (test_mpx.cpp) - 3 testes:**
1. `test_mpx_constructor_initial_state` ✅
2. `test_mpx_prune_buffer_invariants` ✅ (atualizado hoje)
3. `test_mpx_compute_and_floss_produce_valid_output` ✅

**Testes de Robustez (test_mpx_robustness.cpp) - 15 testes:**
4. `test_movmean_returns_finite_values` ✅ (Numerical Stability)
5. `test_movsig_returns_valid_values` ✅ (Numerical Stability)
6. `test_differential_arrays_are_finite` ✅ (Numerical Stability)
7. `test_mp_profile_values_bounded` ✅ (Matrix Profile Invariants)
8. `test_mp_indexes_valid_or_empty` ✅ (Matrix Profile Invariants)
9. `test_floss_returns_finite_values` ✅ (FLOSS Output)
10. `test_floss_endpoints_preserve_value_one` ✅ (FLOSS Output)
11. `test_minimal_buffer_size` ✅ (Edge Cases)
12. `test_large_exclusion_zone` ✅ (Edge Cases - ativado hoje)
13. `test_time_constraint_accepted` ✅ (Edge Cases - ativado hoje)
14. `test_constant_signal_no_crash` ✅ (Data Pattern Robustness)
15. `test_mixed_pattern_signal` ✅ (Data Pattern Robustness)
16. `test_noise_signal_stability` ✅ (Data Pattern Robustness - ativado hoje)
17. `test_sequential_compute_does_not_crash` ✅ (Sequential Processing - ativado hoje)
18. `test_floss_after_sequential_compute` ✅ (Sequential Processing - ativado hoje)

**Testes de Regressão Golden Reference (test_mpx_golden.cpp) - 2 testes (NOVO):**
19. `test_golden_reference_metadata` ✅ (Golden Reference - criado hoje)
20. `test_golden_reference_sample_validation` ✅ (Golden Reference - criado hoje)

### Golden Reference Testing (2026-03-09) ✅
**Implementação Completa**: Testes de regressão contra golden reference CSV
- **Arquivo**: `test/golden_reference_nodelete.csv` (43.334 linhas, 5000 buffer + 4791 profile)
- **Parâmetros**: window_size=210, buffer_size=5000, chunk_size=500, 54 iterações
- **Método**: Processa 27.000 amostras de `test_data.csv` e compara com golden reference
- **Validação**:
  - Metadata exato (buffer_used, profile_len, movsum, mov2sum)
  - Amostragem de 434 valores de todos os buffers (100% match)
- **Tolerância**: 1e-5 (0.001%) para valores float
- **Utilidade**: Detecta regressões e mudanças não intencionais na biblioteca

### Comparação com other_tests (2026-03-09)
- **Análise realizada**: Comparação completa entre `other_tests/` e `test/`
- **Resultado Robustez**: Todos os testes já implementados em Unity
- **Resultado Golden**: Testes golden transportados com sucesso de `other_tests/test_mpx_golden.cpp`
- **Diferença**: 5 testes robustez estavam desabilitados + 2 testes golden adicionados
- **Conclusão**: Cobertura completa — todos os testes de `other_tests` portados para Unity ✅

### Dataset de Teste Atualizado
- **Arquivo**: `test/test_data.csv` agora contém dataset mais robusto (1601 samples de ECG)
- Características: Maior variabilidade e estrutura temporal para validação aprimorada
- Uso nos testes:
  - Testes sintéticos: `TestSignalGenerator` para padrões controlados
  - Testes golden: `test_data.csv` completo (30.000 amostras processadas)

### Próximas Ações
1. Integração com Rcpp como Golden Standard
2. Ativar testes desabilitados com boundary checks robustos
3. Validação em esp32idf após sucesso em native

## Sessão Atual — ESP32 SD + Golden (2026-03-09)
✅ **COMPLETO**

### Resultado final dos testes
- `platformio test -e native`: **20/20 PASS** (~25s)
- `platformio test -e esp32_test`: **20/20 PASS** (~1m44s)

### Ajustes implementados nesta sessão
1. **Novo environment de testes embarcados**
  - Adicionado `env:esp32_test` em `platformio.ini` para `sparkfun_esp32_iot_redboard`
  - Flag principal: `-DUSE_SD_CARD=1`

2. **Leitura de CSV no ESP32 via SD card**
  - Testes golden adaptados para usar paths SD (`/sdcard/...`)
  - Fallback para nomes curtos FATFS 8.3:
    - `TEST_D~1.CSV`
    - `GOLDEN~1.CSV`

3. **Montagem SD corrigida para SDSPI**
  - Substituição de abordagem por `esp_vfs_fat_sdspi_mount`
  - Pinos default configuráveis por build flags (`SD_SPI_*`)

4. **Estabilidade em hardware (loop/reset resolvido)**
  - Causa: stack canary na task principal durante testes golden
  - Correção: execução dos testes em task dedicada com stack maior (`unity_test`, 32768)
  - `Mpx` dos testes golden movido para heap (`new (std::nothrow)`)

5. **Performance dos testes golden melhorada**
  - `test_golden_reference_sample_validation` otimizado para leitura do golden em **passagem única (streaming)**
  - Eliminada releitura repetida de arquivo por linha amostrada

6. **Comportamento validado dos testes golden**
  - Metadata: processa **27.000 amostras** (`54 x 500`)
  - Sample validation: amostra a cada 100 linhas (~434 amostras), comparando buffers suportados
  - Tolerância igual em `native` e `esp32_test`: `1e-5` (com regra relativa para floats)

7. **Documentação atualizada**
  - `test/README_ESP32_TESTING.md` atualizado para estado real (montagem automática, SDSPI, fallback 8.3, tempos e fluxo atual)
  - `test/TEST_STRATEGY.md` atualizado com baseline atual do golden e estratégia de validação vigente
  - Comentários de cabeçalho em `test/test_mpx_golden.cpp` alinhados com comportamento atual

