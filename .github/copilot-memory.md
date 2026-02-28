# false.alarm.io — Copilot Memory

## Última atualização
- Data: 2026-02-28

## Contexto atual
- Projeto em PlatformIO com `framework = espidf` no ambiente principal `esp32idf` (ESP-IDF 5.5.3).
- Limpeza recente concluída: remoção de artefatos Gitpod/DevContainer e arquivamento de diretórios legados em `archive/`.
- **Auditoria e compatibilização com ESP-IDF 5.x completada**: todas as correções aplicadas e validadas com build bem-sucedido.

## Status de Compatibilidade ESP-IDF 5.x
✅ **COMPLETO** - Todas as correções aplicadas e testadas:
1. ✅ Atualizado `idf_component.yml`: `idf: ">=5.0"` (estava ">=4.1")
2. ✅ Limpeza de `Mpx.hpp`: melhor documentação das branches de compilação condicionais
3. ✅ Validação de `sdkconfig.defaults`:
   - Remover `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (não necessário para app de streaming)
   - Mantidos: `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP` com comentário explicativo
4. ✅ Limpeza de `platformio.ini`: removidas linhas comentadas de dependências obsoletas
5. ✅ Build final: SUCCESS - firmware.bin gerado com 23.9% Flash, 4.2% RAM

## Checklist salvo para próxima execução (versão mínima e segura)
Objetivo: aplicar otimizações sem alterar a lógica do algoritmo.

1. Estabelecer baseline de desempenho
   - Medir CPU por task, stack high-water mark, heap livre/mínimo, latência fim-a-fim.
   - Rodar cenário fixo (mesmo input/duração) para comparação.

2. Ajustar build para medição real
   - Usar perfil release para benchmark (`build_type = release`).
   - Comparar `-O2` vs `-Os` e manter o melhor para o caso.

3. Reduzir logging no caminho crítico
   - Diminuir nível global de log.
   - Evitar logs dentro dos loops de aquisição/processamento.

4. Revisar FreeRTOS para previsibilidade
   - Validar prioridade/afinidade de tasks.
   - Confirmar stacks por medição e ajustar tamanhos.
   - Garantir capacidade adequada de queue/ring buffer para picos.

5. Garantir memória previsível
   - Evitar alocação dinâmica em loops de alta frequência.
   - Pré-alocar e reutilizar buffers.

6. Isolar perfil de benchmark vs perfil de I/O
   - Separar claramente benchmark com arquivo (`FILE_DATA=1`) do fluxo real de sensor.
   - Evitar I/O LittleFS no caminho crítico durante benchmark.

7. Minimizar sincronização/cópias
   - Reduzir cópias de buffer.
   - Evitar seções críticas longas/locks desnecessários.

8. Enxugar sdkconfig
   - Desligar recursos não usados.
   - Manter watchdog/timers configurados para carga real.

9. Controlar clock/energia no benchmark
   - Fixar frequência de CPU durante medição para evitar variabilidade.

10. Validar resultado final
   - Repetir medições (3-5 execuções), comparar média e pior caso.
   - Manter perfil debug e release no `platformio.ini`.

## Próximo passo acordado
- Aplicar posteriormente uma implementação mínima e segura focada em:
  - perfil release,
  - redução de logging,
  - instrumentação objetiva de stack/heap.
