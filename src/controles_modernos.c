/*
 * Silent Hill (PS1, versão europeia SLES-01514): controles modernos.
 *
 * Com o mod ligado, o analógico esquerdo (ou o direcional) aponta para onde o
 * Harry anda, relativo à câmera: o mod gira o Harry até essa direção e entrega
 * ao jogo só "para frente" (e "correr" com o analógico no fim do curso). Os
 * controles "tank" originais continuam valendo para mirar, nos menus, nas cenas
 * e nos eventos, quando o jogo recebe o controle sem mudança.
 *
 * Referência da câmera: com a opção travar_direcao, vale a câmera do momento em
 * que o analógico foi inclinado, até soltar ou virar o analógico mais de 45
 * graus. A câmera do jogo gira sozinha atrás do Harry (e às vezes corta), e
 * seguir ela a cada quadro faria o Harry andar em círculo.
 *
 * Endereços da versão europeia, do mapa de símbolos da decompilação
 * (github.com/Vatuu/silent-hill-decomp, configs/EUR/sym.bodyprog.txt; o
 * SLES_015.14 do disco confere com o checksum de lá). Ângulos em Q12:
 * 4096 = 360 graus; o Harry anda na direção (sin, cos) de rotation.vy em (x, z).
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mod_plugins.h"

#define PLUGIN "sh1.controles-modernos"

/* g_SysWork */
#define SYSWORK          0x800B95B0u
#define SYS_STATE        (SYSWORK + 0x8)          /* e_SysState: 0 = gameplay */
#define PLAYER           (SYSWORK + 0x4C)         /* playerWork.player (s_SubCharacter) */
#define PLAYER_ROT_Y     (PLAYER + 0x24 + 2)      /* rotation.vy */
#define PLAYER_HEADING   (PLAYER + 0x3C)          /* headingAngle */
#define PLAYER_STATE     (PLAYER + 0x128 + 0x1C)  /* playerWork.extra.state: 0 = nenhum, 2 = parado */
#define PLAYER_STOP      (SYSWORK + 0x235A)       /* playerStopFlags */
/* vcWork (câmera) */
#define VCWORK           0x800B92C0u
#define CAM_YAW          (VCWORK + 0x8E + 2)      /* cam_mat_ang.vy: para onde a câmera olha */
/* g_GameWork */
#define GAMEWORK         0x800BBD18u
#define CFG_AIM          (GAMEWORK + 0x8)         /* config.controllerConfig.aim (máscara de botões) */
#define CFG_RUN          (GAMEWORK + 0xC)         /* config.controllerConfig.run */
#define CFG_WALKRUN_REV  (GAMEWORK + 0x2B)        /* config.extraWalkRunCtrl: correr por padrão */
#define GAME_STATE       (GAMEWORK + 0x594)       /* e_GameState: 11 = no jogo */

/* botões do PS1 (ativos em 0 no controle; as máscaras da configuração usam a mesma ordem) */
#define PAD_UP     0x0010u
#define PAD_RIGHT  0x0020u
#define PAD_DOWN   0x0040u
#define PAD_LEFT   0x0080u
#define PAD_DIRS   (PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT)
#define PAD_R1     0x0800u
#define PAD_SQUARE 0x8000u

static int s_travar = 1;            /* opção travar_direcao */
static int s_correr_curso = 1;      /* opção correr_no_fim_do_curso */
static int s_direcional = 1;        /* opção direcional_moderno */
static int s_giro = 0x100;          /* opção giro: passo máximo por quadro (Q12) */
static int s_zona = 32;             /* zona morta do analógico, de 128 */

static int s_ativo;                 /* quadros que o giro ainda vale (renovado a cada leitura do controle) */
static int s_alvo;                  /* direção pedida (Q12) */
static int s_cam_ref;               /* direção da câmera usada como referência */
static int s_ent_ref;               /* direção do analógico quando a referência foi tomada */
static int s_tinha_entrada;
static int s_correndo;              /* histerese do correr pelo analógico */

static int dif_ang(int a, int b)
{
    int d = (a - b) & 0xFFF;
    return d >= 0x800 ? d - 0x1000 : d;
}

static uint16_t mascara(uint32_t addr, uint16_t padrao)
{
    const uint16_t m = psx_mod_read_half(addr);
    return m ? m : padrao;
}

/* Só no jogo, em controle normal: sem menu, cena, evento ou ação travada. */
static int pode_controlar(void)
{
    if (!psx_mod_game_started()) return 0;
    if ((int32_t)psx_mod_read_word(GAME_STATE) != 11) return 0;
    if ((int32_t)psx_mod_read_word(SYS_STATE) != 0) return 0;
    const int32_t st = (int32_t)psx_mod_read_word(PLAYER_STATE);
    if (st != 0 && st != 2) return 0;
    if (psx_mod_read_byte(PLAYER_STOP)) return 0;
    return 1;
}

static void solta(void)
{
    s_ativo = 0;
    s_tinha_entrada = 0;
    s_correndo = 0;
}

static int transforma(const PSXModPadFrame* f, PSXModPadOutput* o)
{
    if (!pode_controlar()) { solta(); return 0; }
    const uint32_t apertado = ~f->buttons & 0xFFFFu;
    const uint16_t mirar = mascara(CFG_AIM, PAD_R1);
    if (apertado & mirar) { solta(); return 0; }     /* mirando: controle original */

    /* entrada: analógico esquerdo, ou o direcional (oito direções, andando) */
    const int x = (int)f->lx - 0x80, y = (int)f->ly - 0x80;
    const double raio = sqrt((double)(x * x + y * y));
    double ex = 0, ey = 0, forca = 0;
    if (raio > s_zona) {
        ex = x; ey = y;
        forca = (raio - s_zona) / (127.0 - s_zona);
        if (forca > 1) forca = 1;
    } else if (s_direcional && (apertado & PAD_DIRS)) {
        ex = ((apertado & PAD_RIGHT) ? 1 : 0) - ((apertado & PAD_LEFT) ? 1 : 0);
        ey = ((apertado & PAD_DOWN) ? 1 : 0) - ((apertado & PAD_UP) ? 1 : 0);
        forca = (ex || ey) ? 0.5 : 0;
    }

    const int cam = psx_mod_read_half(CAM_YAW) & 0xFFF;

    if (forca <= 0) {
        /* sem direção: o jogo recebe os botões, sem o direcional (que é do mod) */
        if (s_direcional) o->buttons |= PAD_DIRS;
        o->lx = o->ly = 0x80;
        solta();
        s_cam_ref = cam;
        return 1;
    }

    /* para a frente = longe da câmera; direita = direita da tela */
    const int ent = (int)lround(atan2(ex, -ey) * 4096.0 / (2.0 * 3.14159265358979323846)) & 0xFFF;
    if (!s_tinha_entrada || !s_travar) {
        s_cam_ref = cam;
        s_ent_ref = ent;
    } else if (abs(dif_ang(ent, s_ent_ref)) > 0x200) {
        s_cam_ref = cam;                  /* virou o analógico: nova referência */
        s_ent_ref = ent;
    }
    s_tinha_entrada = 1;
    s_alvo = (s_cam_ref + ent) & 0xFFF;
    s_ativo = 3;

    /* anda girando enquanto a diferença é de até 60 graus; numa virada maior
     * (meia-volta) gira parado primeiro, sem andar para o lado errado */
    const int rot = psx_mod_read_half(PLAYER_ROT_Y) & 0xFFF;
    const int frente = abs(dif_ang(s_alvo, rot)) < 0x2AB;
    uint32_t b = o->buttons | PAD_DIRS;
    if (frente) b &= ~PAD_UP;
    const uint16_t correr = mascara(CFG_RUN, PAD_SQUARE);
    /* analógico: corre acima de 85% e só volta a andar abaixo de 75% */
    if (forca > 0.85) s_correndo = 1; else if (forca < 0.75) s_correndo = 0;
    int quer_correr = (apertado & correr) != 0 || (s_correr_curso && s_correndo);
    if (psx_mod_read_byte(CFG_WALKRUN_REV)) quer_correr = !quer_correr;
    if (quer_correr && frente) b &= ~(uint32_t)correr; else b |= correr;
    o->buttons = b & 0xFFFFu;
    o->lx = o->ly = 0x80;
    return 1;
}

/* Uma vez por quadro do console: gira o Harry até a direção pedida. */
static void quadro(void)
{
    if (s_ativo <= 0) return;
    s_ativo--;
    if (!pode_controlar()) return;
    const int16_t rot = (int16_t)psx_mod_read_half(PLAYER_ROT_Y);
    int d = dif_ang(s_alvo, rot);
    if (d > s_giro) d = s_giro; else if (d < -s_giro) d = -s_giro;
    if (!d) return;
    int novo = rot + d;
    if (novo >= 0x1000) novo -= 0x1000;
    if (novo < -0x1000) novo += 0x1000;
    psx_mod_write_half(PLAYER_ROT_Y, (uint16_t)novo);
    psx_mod_write_half(PLAYER_HEADING, (uint16_t)novo);
}

static int opcao_bool(const char* id, int padrao)
{
    char v[16];
    if (!psx_mod_current_option_value(id, v, sizeof v)) return padrao;
    return strcmp(v, "true") == 0 || strcmp(v, "1") == 0;
}

static void prende_controle(void)
{
    static const PSXModPadTransform t = {
        sizeof(PSXModPadTransform),
        PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DIGITAL) | PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DUALSHOCK),
        PSX_MOD_PAD_DUALSHOCK,
        transforma,
    };
    (void)psx_mod_set_pad_transform(0, &t);
}

static void ativa(void)
{
    char v[16];
    s_travar = opcao_bool("travar_direcao", 1);
    s_correr_curso = opcao_bool("correr_no_fim_do_curso", 1);
    s_direcional = opcao_bool("direcional_moderno", 1);
    s_giro = 0x100;
    if (psx_mod_current_option_value("giro", v, sizeof v)) {
        if (!strcmp(v, "suave")) s_giro = 0x0A0;
        else if (!strcmp(v, "instantaneo")) s_giro = 0x800;
    }
    if (psx_mod_current_option_value("zona_morta", v, sizeof v)) {
        const int z = atoi(v);                          /* em %, de 5 a 60 */
        if (z >= 5 && z <= 60) s_zona = z * 128 / 100;
    }
    solta();
    prende_controle();
}

PSX_MOD_CONSTRUCTOR(sh1_registra_controles_modernos)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
    (void)psx_mod_register_vblank_plugin(PLUGIN, quadro);
    (void)psx_mod_register_savestate_plugin(PLUGIN, prende_controle);
}
