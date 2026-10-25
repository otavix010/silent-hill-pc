/*
 * Fluidez 50 FPS (mod sh1.fluidez): o gameplay a 50 quadros por segundo, na velocidade original.
 *
 * O laço principal do jogo (decompilação: bodyprog/sys/game_main.c) espera g_IntervalVBlanks
 * VBlanks por quadro e calcula o tempo do quadro pelas linhas de vídeo que passaram
 * (GsGetVcount -> g_DeltaTime). Ao entrar no gameplay ele põe o intervalo em 2 (25 fps no PAL;
 * nos menus é 1), porque o PS1 não dava conta de mais. Como movimento, física e animação usam
 * o tempo medido, com 1 o jogo desenha o dobro de quadros na mesma velocidade (medido: o Harry
 * anda a mesma distância no mesmo tempo nos dois modos).
 *
 * Só o intervalo não basta: o quadro do gameplay custa ~1,6 VBlank de CPU emulada, e sairia a
 * ~31 fps. O game.toml tem [timing] guest_cycle_scale = 2 com guest_cycle_scale_gated: o código
 * do jogo custa metade do tempo emulado (vídeo, CD, som e timers continuam no tempo do
 * hardware), mas só enquanto este mod segura a porta aberta. Desligado, é o jogo original.
 *
 * As demonstrações da tela de título usam o intervalo gravado nelas, e não mudam.
 */
#include <stdint.h>
#include <stdio.h>

#include "mod_plugins.h"

#define PLUGIN "sh1.fluidez"

#define INTERVALO_VBLANKS 0x800A8BCCu   /* g_IntervalVBlanks (EUR) */

static void ativa(void)
{
    psx_mod_set_guest_cycle_scale_gate(1);
    printf("[fluidez] 50 fps no gameplay (CPU emulada %ux)\n", (unsigned)psx_mod_guest_cycle_scale());
    fflush(stdout);
}

/* a cada VBlank: o jogo só põe 2 ao entrar no gameplay; volta para 1 */
static void quadro(void)
{
    if (psx_mod_read_word(INTERVALO_VBLANKS) == 2u)
        psx_mod_write_word(INTERVALO_VBLANKS, 1u);
}

PSX_MOD_CONSTRUCTOR(sh1_registra_fluidez)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
    (void)psx_mod_register_vblank_plugin(PLUGIN, quadro);
}
