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
 * A porta custa caro no PC: o runtime emula ciclo a ciclo, então a CPU emulada 2x dobra o tempo
 * da thread de emulação (medido no café, 9800X3D: ~30% de um núcleo com a porta fechada, ~54%
 * aberta). Num PC em que isso passa de um núcleo, a emulação atrasa em relação ao relógio: tudo
 * fica em câmera lenta e o som picota (o relato de um Ryzen 5 5600X a partir do café). Por isso
 * a porta abre só numa fração d dos VBlanks (CPU emulada efetiva 1+d), distribuída por igual, e
 * d se ajusta a cada 50 VBlanks: o mod mede, na própria thread de emulação, a CPU usada e o tempo
 * de parede, estima o custo de 1x (ocupação / (1+d)) e escolhe o d que deixa a thread em ~85%
 * (75% se a janela atrasou). Desce na hora e sobe pela metade da diferença a cada janela. Num PC
 * com folga d fica em 1 (o 2x de sempre); num mais lento o jogo fica entre 25 e 50 quadros, mas
 * sempre na velocidade certa. Janelas com a emulação adiantada (carga acelerada, turbo) ou
 * parada (um VBlank de mais de 250 ms) não contam.
 *
 * Teste: SH_FLUIDEZ_LENTO=k faz cada trabalho da thread de emulação custar (1+k)x, como uma CPU
 * k vezes mais lenta (0.6 ~ um 5600X diante de um 9800X3D nesta carga), e SH_FLUIDEZ_FIXO=1
 * deixa a porta sempre aberta, como antes da adaptação (para comparar).
 *
 * As demonstrações da tela de título usam o intervalo gravado nelas, e não mudam.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_plugins.h"

#define PLUGIN "sh1.fluidez"

#define INTERVALO_VBLANKS 0x800A8BCCu   /* g_IntervalVBlanks (EUR) */

#define JANELA          50      /* VBlanks por medida (1 s no PAL) */
#define PAUSA_S         0.25    /* um VBlank mais longo que isto: emulação parada, a janela recomeça */
#define ATRASO_VBPS     48.5    /* abaixo disto a janela atrasou */
#define ADIANTADA_VBPS  50.5    /* acima disto a emulação corre solta: a janela não conta */
#define ALVO            0.85    /* ocupação desejada da thread de emulação */
#define ALVO_ATRASADA   0.75

static double  s_freq;
static double  s_d = 1.0, s_acum;
static int     s_porta = -1, s_abertos;
static int     s_vblanks;
static double  s_t_janela, s_cpu_janela, s_t_vblank;
static double  s_avisou_em = -1e9;
static int     s_cheia = 1;
static double  s_lento;
static int     s_fixo;
static ULONG64 s_ciclos;

static double agora(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / s_freq;
}

static double cpu_da_thread(void)
{
    FILETIME cri, fim, nucleo, usuario;
    if (!GetThreadTimes(GetCurrentThread(), &cri, &fim, &nucleo, &usuario)) return 0.0;
    ULONGLONG k = ((ULONGLONG)nucleo.dwHighDateTime << 32) | nucleo.dwLowDateTime;
    ULONGLONG u = ((ULONGLONG)usuario.dwHighDateTime << 32) | usuario.dwLowDateTime;
    return (double)(k + u) / 1e7;
}

/* SH_FLUIDEZ_LENTO: gira pelo trabalho do último VBlank vezes k */
static void simula_cpu_lenta(void)
{
    ULONG64 c, alvo;
    QueryThreadCycleTime(GetCurrentThread(), &c);
    if (s_ciclos) {
        alvo = c + (ULONG64)((double)(c - s_ciclos) * s_lento);
        do QueryThreadCycleTime(GetCurrentThread(), &c); while (c < alvo);
    }
    s_ciclos = c;
}

static void ajusta(double t, double vbps, double ocupada, double fracao)
{
    double custo1x = ocupada / (1.0 + fracao);
    double d = custo1x > 0.01 ? (vbps < ATRASO_VBPS ? ALVO_ATRASADA : ALVO) / custo1x - 1.0 : 1.0;
    if (d < 0.0) d = 0.0;
    if (d > 1.0) d = 1.0;
    s_d = d < s_d ? d : s_d + 0.5 * (d - s_d);
    if (s_d > 0.98) s_d = 1.0;
    /* no log: quando desce abaixo de 1,9x, quando volta ao 2x, e o nível no máximo a cada 60 s */
    int cheia = s_cheia ? s_d >= 0.9 : s_d >= 1.0;
    if (cheia != s_cheia || (!cheia && t - s_avisou_em >= 60.0)) {
        if (cheia)
            printf("[fluidez] folga de volta: CPU emulada 2x (thread de emulação em %.0f%%)\n", ocupada * 100.0);
        else
            printf("[fluidez] o PC não dá conta do 2x: CPU emulada %.2fx (%.1f VBlanks/s, thread de emulação "
                   "em %.0f%%)\n", 1.0 + s_d, vbps, ocupada * 100.0);
        fflush(stdout);
        s_cheia = cheia;
        s_avisou_em = t;
    }
}

static void mede(int aberta)
{
    double t = agora(), cpu = cpu_da_thread();
    if (s_vblanks == 0 || t - s_t_vblank > PAUSA_S) {
        s_vblanks = 0;
        s_abertos = 0;
        s_t_janela = t;
        s_cpu_janela = cpu;
    }
    s_t_vblank = t;
    if (++s_vblanks <= JANELA) {
        s_abertos += aberta;
        return;
    }
    double dt = t - s_t_janela;
    double vbps = JANELA / dt, ocupada = (cpu - s_cpu_janela) / dt;
    double fracao = (double)s_abertos / JANELA;
    s_vblanks = 1;
    s_abertos = aberta;
    s_t_janela = t;
    s_cpu_janela = cpu;
    if (vbps <= ADIANTADA_VBPS)
        ajusta(t, vbps, ocupada, fracao);
}

static void ativa(void)
{
    LARGE_INTEGER f;
    const char *lento = getenv("SH_FLUIDEZ_LENTO");
    QueryPerformanceFrequency(&f);
    s_freq = (double)f.QuadPart;
    s_lento = lento ? atof(lento) : 0.0;
    s_fixo = getenv("SH_FLUIDEZ_FIXO") != NULL;
    s_ciclos = 0;
    s_vblanks = 0;
    s_d = 1.0;
    s_acum = 0.0;
    s_cheia = 1;
    s_avisou_em = -1e9;
    s_porta = 1;
    psx_mod_set_guest_cycle_scale_gate(1);
    printf("[fluidez] 50 fps no gameplay (CPU emulada %ux, menos se o PC não acompanhar)%s%s\n",
           (unsigned)psx_mod_guest_cycle_scale(), s_lento > 0.0 ? " [teste: CPU lenta simulada]" : "",
           s_fixo ? " [teste: porta fixa aberta]" : "");
    fflush(stdout);
}

/* a cada VBlank: o jogo só põe 2 ao entrar no gameplay; volta para 1. E a porta deste VBlank. */
static void quadro(void)
{
    if (psx_mod_read_word(INTERVALO_VBLANKS) == 2u)
        psx_mod_write_word(INTERVALO_VBLANKS, 1u);
    if (s_lento > 0.0) simula_cpu_lenta();
    if (s_fixo) return;
    s_acum += s_d;
    int aberta = s_acum >= 1.0;
    if (aberta) s_acum -= 1.0;
    if (aberta != s_porta) {
        psx_mod_set_guest_cycle_scale_gate(aberta);
        s_porta = aberta;
    }
    mede(aberta);
}

/* um estado carregado traz a porta como estava ao salvar: o próximo VBlank decide de novo */
static void estado_carregado(void)
{
    s_porta = -1;
    s_vblanks = 0;
}

PSX_MOD_CONSTRUCTOR(sh1_registra_fluidez)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
    (void)psx_mod_register_vblank_plugin(PLUGIN, quadro);
    (void)psx_mod_register_savestate_plugin(PLUGIN, estado_carregado);
}
