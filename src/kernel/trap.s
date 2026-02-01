.section .text
.global trap_entry
.align 4

# ===========================================================================================================
#  ARQUIVO   : trap.s
#  DESCRIÇÃO : Ponto de entrada de baixo nível para Interrupções e Exceções.
#  FUNÇÃO    : Realizar o "Context Switch" (Troca de Contexto).
# ===========================================================================================================

# Tamanho da estrutura context_t definida em task.h
# 32 registradores * 4 bytes = 128 bytes.
.equ CTX_SIZE, 128

trap_entry:

    # =======================================================================================================
    #  FASE 1: O CONGELAMENTO (Salvar o Estado Anterior)
    # =======================================================================================================

    # Quando uma interrupção ocorre, a CPU para o que estava fazendo e pula pra cá.
    # Os registradores ainda têm os valores da Tarefa A. Se usarmos qualquer um
    # deles sem salvar, corrompemos a Tarefa A para sempre.

    # Aloca espaço na PILHA DA TAREFA ATUAL para guardar a "snapshot" dos registradores.
    # A pilha cresce para baixo (endereços menores), por isso subtraímos.
    addi sp,    sp, -CTX_SIZE

    # Salva os Registradores Gerais na pilha.
    # Nota: A ordem deve bater EXATAMENTE com a struct 'context_t' no C.
    sw ra,    0(sp)  # x1  (Return Address: para onde a função atual voltaria)
    
    # O registrador SP (x2) original será salvo depois, pois já o modificamos acima.
    
    # --- Registradores especiais de ambiente ---

    sw gp,    8(sp)   # x3  gp: ponteiro da área global (ABI); base para small data
    sw tp,   12(sp)   # x4  tp: ponteiro de TLS (thread-local storage)

    # --- Registradores temporários (caller-saved) ---

    sw t0,   16(sp)   # x5  t0: temporário volátil
    sw t1,   20(sp)   # x6  t1: temporário volátil
    sw t2,   24(sp)   # x7  t2: temporário volátil

    # --- Frame pointer e saved registers (callee-saved) ---

    sw s0,   28(sp)   # x8  s0/fp: frame pointer / registrador preservado
    sw s1,   32(sp)   # x9  s1: preservado entre chamadas

    # --- Registradores de argumentos / retorno ---

    sw a0,   36(sp)   # x10 a0: arg0 / retorno 0
    sw a1,   40(sp)   # x11 a1: arg1 / retorno 1
    sw a2,   44(sp)   # x12 a2: arg2
    sw a3,   48(sp)   # x13 a3: arg3
    sw a4,   52(sp)   # x14 a4: arg4
    sw a5,   56(sp)   # x15 a5: arg5
    sw a6,   60(sp)   # x16 a6: arg6
    sw a7,   64(sp)   # x17 a7: arg7 / ID de syscall (convenção comum)

    # --- Saved registers adicionais (callee-saved) ---

    sw s2,   68(sp)   # x18 s2: preservado entre chamadas
    sw s3,   72(sp)   # x19 s3: preservado entre chamadas
    sw s4,   76(sp)   # x20 s4: preservado entre chamadas
    sw s5,   80(sp)   # x21 s5: preservado entre chamadas
    sw s6,   84(sp)   # x22 s6: preservado entre chamadas
    sw s7,   88(sp)   # x23 s7: preservado entre chamadas
    sw s8,   92(sp)   # x24 s8: preservado entre chamadas
    sw s9,   96(sp)   # x25 s9: preservado entre chamadas
    sw s10, 100(sp)   # x26 s10: preservado entre chamadas
    sw s11, 104(sp)   # x27 s11: preservado entre chamadas

    # --- Temporários estendidos (caller-saved) ---

    sw t3,  108(sp)   # x28 t3: temporário volátil
    sw t4,  112(sp)   # x29 t4: temporário volátil
    sw t5,  116(sp)   # x30 t5: temporário volátil
    sw t6,  120(sp)   # x31 t6: temporário volátil

    # Salva o MEPC (Machine Exception Program Counter)
    # Este registrador especial contém o endereço exato da instrução que ia ser
    # executada quando a interrupção aconteceu. É para lá que voltaremos.

    csrr t0, mepc
    sw t0,  124(sp)

    # Salva o SP Original
    # Como fizemos 'addi sp, -128' no início, o SP original era (sp atual + 128).
    # Calculamos esse valor e salvamos no slot x2 (offset 4).

    addi t0, sp, CTX_SIZE
    sw t0, 4(sp)

    # =======================================================================================================
    #  FASE 2: Chamar o Kernel em C
    # =======================================================================================================

    # Agora que o estado da tarefa está salvo na pilha dela, podemos usar a CPU
    # livremente para rodar o código do Kernel.

    # Prepara os argumentos para a função C 'trap_handler(mcause, mepc, sp)':

    csrr a0, mcause   # Arg 0: Causa da interrupção (Timer? Syscall? Erro?)
    csrr a1, mepc     # Arg 1: Onde parou
    mv   a2, sp       # Arg 2: Ponteiro para o contexto salvo (para syscalls lerem args)

    # Chama a função C.
    # O Kernel vai decidir o que fazer: tratar syscall, processar timer, e...
    # ...o mais importante: ATUALIZAR 'next_task' se for hora de trocar!

    call trap_handler

    # O trap_handler chamou timer_handler -> schedule() -> atualizou 'next_task'
    
    # =======================================================================================================
    #  FASE 3: Context Switch
    # =======================================================================================================

    # O trap_handler retornou. Agora verificamos se o Kernel mandou trocar de tarefa.
    # O Scheduler em C atualizou as variáveis globais 'current_task' e 'next_task'.

    # Carrega o endereço das variáveis globais em registradores temporários ---------------------------------

    la t0, current_task   # t0 = &current_task
    lw t1, 0(t0)          # t1 = Ponteiro para a struct da Tarefa ATUAL (TCB)
    
    la t2, next_task      # t2 = &next_task
    lw t3, 0(t2)          # t3 = Ponteiro para a struct da Tarefa NOVA (TCB)

    # Verifica se deve trocar -------------------------------------------------------------------------------

    # 1. Se next_task for NULL (erro ou boot), volta para a atual.

    beqz t3, restore_context

    # 2. Se next_task == current_task, não há troca a fazer.

    beq t1, t3, restore_context

    # --- INÍCIO DA TROCA -----------------------------------------------------------------------------------
    
    # Primeiro, guardar onde paramos na pilha da TAREFA VELHA.
    # O registrador 'sp' atual aponta para o topo da pilha da tarefa velha.
    # Precisamos salvar esse endereço dentro da struct task_t (no campo 'sp').
    # Offset 28 calculado: tid(4) + name(16) + state(4) + priority(4) = 28.
    
    # Verifica se current_task existe (pode ser NULL no primeiro boot)
    beqz t1, load_next_task   
    
    sw sp, 28(t1)  # TAREFA VELHA->sp = Registrador SP atual   

load_next_task:

    # Na sequência: atualizar a variável global para que o sistema saiba quem está rodando.

    sw t3, 0(t0)   # current_task = next_task

    # Então, carregar a pilha da TAREFA NOVA.
    # Aqui acontece a mágica. Ao mudar o SP, mudamos o "mundo" da CPU.

    lw sp, 28(t3)  # Registrador SP = TAREFA_NOVA->sp

    # AGORA o registrador 'sp' aponta para a pilha da OUTRA tarefa!
    # Todos os comandos 'lw' (load) abaixo vão ler dados da outra tarefa.

restore_context:

    # =======================================================================================================
    #  FASE 4: Restaurar Contexto
    # =======================================================================================================
    
    # Recupera o endereço de retorno (onde a tarefa parou da última vez).
    # Escreve no registrador CSR 'mepc'.

    lw t0, 124(sp)
    csrw mepc, t0

    # Restaura todos os registradores gerais da pilha.
    # Estamos tirando os dados da "snapshot" e redefinindo o estado do processador.

    lw ra,   0(sp)
    # lw sp,   4(sp) # SP é especial, restauramos no final com addi
    lw gp,   8(sp)
    lw tp,  12(sp)
    lw t0,  16(sp)
    lw t1,  20(sp)
    lw t2,  24(sp)
    lw s0,  28(sp)
    lw s1,  32(sp)
    lw a0,  36(sp)
    lw a1,  40(sp)
    lw a2,  44(sp)
    lw a3,  48(sp)
    lw a4,  52(sp)
    lw a5,  56(sp)
    lw a6,  60(sp)
    lw a7,  64(sp)
    lw s2,  68(sp)
    lw s3,  72(sp)
    lw s4,  76(sp)
    lw s5,  80(sp)
    lw s6,  84(sp)
    lw s7,  88(sp)
    lw s8,  92(sp)
    lw s9,  96(sp)
    lw s10, 100(sp)
    lw s11, 104(sp)
    lw t3,  108(sp)
    lw t4,  112(sp)
    lw t5,  116(sp)
    lw t6,  120(sp)

    # Libera o espaço da pilha (reverte o addi do início).
    # Isso coloca o SP de volta no ponto exato onde a tarefa achava que estava.
    addi sp, sp, CTX_SIZE
    
    # Retorno de Interrupção de Máquina.
    # - Pula para o endereço em MEPC.
    # - Reabilita interrupções globais (MIE).
    # - Muda o modo de privilégio (se necessário).
    mret