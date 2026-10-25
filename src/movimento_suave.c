/*
 * Movimento suave (mod sh1.movimento-suave): liga a interpolação de apresentação do PSXRecomp.
 *
 * No PSXRecomp a interpolação de quadros é de mod: as chaves [video] frame_interpolation do
 * settings.toml são ignoradas ("mod-owned for this title"). Este mod a liga na ativação, antes
 * de a janela existir, como pede a API. O jogo continua no ritmo dele (50 Hz PAL, a lógica a
 * ~25-30 quadros); o apresentador do OpenGL mistura os quadros prontos na taxa pedida. Não há
 * vetores de movimento: é uma transição, por isso o modo "adaptativo" (o padrão) corta limpo
 * nas mudanças grandes, para não deixar rastro dobrado em quem se move.
 *
 * A fonte é a troca real de quadro (FLIP): o Silent Hill não troca a cada VBlank, e assim a
 * mistura se espalha pelo quadro inteiro do jogo em vez de piscar um VBlank e parar no outro.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mod_plugins.h"

#define PLUGIN "sh1.movimento-suave"

static void ativa(void)
{
    char v[32];
    uint32_t fps = 0;   /* 0 = a taxa do monitor */
    if (psx_mod_current_option_value("taxa", v, sizeof v)) {
        const long n = strtol(v, NULL, 10);
        if (n >= 90 && n <= 480) fps = (uint32_t)n;
    }
    uint32_t modo = PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE;
    if (psx_mod_current_option_value("modo", v, sizeof v) && strcmp(v, "linear") == 0)
        modo = PSX_MOD_FRAME_INTERPOLATION_LINEAR;
    if (!psx_mod_set_frame_interpolation(fps)) {   /* devolve 1 quando aceita */
        printf("[movimento-suave] o runtime recusou a interpolação\n");
        fflush(stdout);
        return;
    }
    (void)psx_mod_set_frame_interpolation_blend(modo);
    (void)psx_mod_set_frame_interpolation_source(PSX_MOD_FRAME_SOURCE_FLIP);
    if (fps) printf("[movimento-suave] ligado: %u fps, ", (unsigned)fps);
    else printf("[movimento-suave] ligado: taxa do monitor, ");
    printf("%s\n", modo == PSX_MOD_FRAME_INTERPOLATION_LINEAR ? "linear" : "adaptativo");
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(sh1_registra_movimento_suave)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
}
