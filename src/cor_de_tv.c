/*
 * Cor de TV (mod sh1.cor-de-tv): borra só a cor dos quadros 3D, como a TV do PS1.
 *
 * As texturas do Silent Hill têm ruído de cor (fotos com grão, paleta de 4/8 bits). No
 * console, a TV recebia vídeo composto/RF, que tem pouca banda para a cor: o ruído de cor
 * sumia e o brilho continuava nítido. Ampliado num monitor nítido, ele aparece como
 * pontinhos verdes, vermelhos e azuis que piscam com o movimento. O patches/psxrecomp_sh1.py
 * acrescenta ao shader de apresentação do PSXRecomp um borrão só de cor (+/-2 pixels do PS1
 * na horizontal, +/-1 linha na vertical); este mod o liga na ativação, antes de a janela
 * existir. Vídeos FMV, menus 2D e o brilho de tudo ficam como estão.
 */
#include "mod_plugins.h"

#define PLUGIN "sh1.cor-de-tv"

/* gpu_gl_renderer.c, com o patches/psxrecomp_sh1.py aplicado */
void gl_renderer_set_tv_chroma(int on);

static void ativa(void)
{
    gl_renderer_set_tv_chroma(1);
}

PSX_MOD_CONSTRUCTOR(sh1_registra_cor_de_tv)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
}
