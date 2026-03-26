# AxonOS

![RISC-V](https://img.shields.io/badge/ISA-RISC--V%20RV32I-yellow?style=for-the-badge&logo=riscv)
![Python](https://img.shields.io/badge/Python-3.10-blue?style=for-the-badge&logo=python)

O **AxonOS** é um sistema operacional leve e customizado, desenvolvido do zero para rodar nativamente no [SoC RISC-V](https://github.com/RISC-V-Azedinha/RISC-V). Projetado com foco em sistemas embarcados.

## 🚀 Principais Funcionalidades

- **Arquitetura Multi-Target**: Possui uma camada de abstração de hardware (HAL) separada em diretórios (`drivers/qemu` e `drivers/fpga`), permitindo que o mesmo código do kernel seja compilado tanto para simulação no QEMU quanto para síntese real na placa FPGA.

- **Kernel Preemptivo**: Inclui um escalonador de tarefas (`scheduler.c`), primitivas de sincronização (`mutex.c`) e tratamento avançado de interrupções (via PLIC e `trap.s`).

- **Shell Interativo**: Um terminal integrado (`task_shell.c`) que disponibiliza comandos utilitários como `ps` (lista de processos), `memtest`, `clear`, `reboot`, entre outros.

- Suporte Nativo à NPU: Drivers dedicados (`hal_npu.c` e `hal_dma.c`) que gerenciam a comunicação MMIO com o acelerador de redes neurais em hardware, facilitando a execução de inferências diretamente pelo sistema operacional.

- Gerenciamento de Recursos: Sistemas básicos de alocação de memória (`mm.c`) e um sistema de arquivos simplificado (`fs.c`).

## 📂 Estrutura do Projeto

A organização do código-fonte segue uma hierarquia modular:


- `include/`: Arquivos de cabeçalho (`.h`) divididos por contexto (apps, hal, kernel, sys, util).
- `src/`:

  - `apps/`: Tarefas de nível de usuário que rodam sobre o kernel (Ex: `task_shell`, `task_monitor`, `task_leds`).
  
  - `bin/`: Binários e comandos executáveis disponíveis no shell interativo.
  
  - `bsp/`: Board Support Package, contendo os scripts de linker (`link.ld`, `link_fpga.ld`) e o código de inicialização (`start.s`).
  
  - `drivers/`: Implementação específica de baixo nível da HAL para os alvos FPGA e QEMU (Timer, UART, VGA, NPU, GPIO, etc.).
  
  - `kernel/`: O "coração" do OS. Contém o scheduler, gerenciador de memória, tratamento de IRQs, logger e chamadas de sistema.

- `makefile`: Automação do fluxo de compilação, linking e execução.

- `upload.py`: Script em Python utilizado para transferir o binário compilado para a FPGA via comunicação serial (UART).

## 🛠️ Como Compilar e Executar

### Pré-requisitos

Certifique-se de ter instalado no seu ambiente a toolchain de compilação cruzada do RISC-V (`riscv64-unknown-elf-gcc` ou similar), o `make` e o `qemu-system-riscv32` (para simulação).

### Simulando no QEMU
Para compilar e testar rapidamente o sistema operacional de forma simulada sem precisar do hardware físico:

```bash
# Compila o OS com os drivers do QEMU e inicia a simulação
make qemu
```

### Executando na FPGA

Para rodar na placa real, você precisa compilar o sistema mirando os drivers da FPGA e enviar o binário via porta serial:

```bash
# Compila o OS e faz upload para a placa
make fpga
```
