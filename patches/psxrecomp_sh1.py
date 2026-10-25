"""Correções do port do Silent Hill no PSXRecomp baixado (psxrecomp/, versão de framework_pins.txt).

O gerar.ps1 roda este script antes de compilar; ele pode rodar de novo sem mudar nada
(cada correção tem uma marca). Mexe só nos arquivos do framework na pasta do jogador.

1. compile_overlays.py: "fragment cache-key collision/stale pair" deixa de derrubar a
   rodada. Mapas diferentes do Silent Hill carregam código no mesmo endereço (região
   0x80091000), e um fragmento com a mesma função (entrada + CRC do código) já compilado
   antes, a partir de um C vizinho diferente, fazia toda rodada sair com erro. Com a
   rodada "falha", o runtime espera cada vez mais antes da próxima (até 5 minutos), e o
   código de uma área nova fica no interpretador nesse tempo. Agora o fragmento fica com
   o DLL que já existe (ou no interpretador), como os outros pulos seguros.
   (O tools/compile_overlays.py entra no hash do gerador de código: o gerar.ps1 refaz o
   recompilador com o ensure-emitters depois deste script.)

2. gpu_gl_renderer.c: "cor de TV" nos quadros 3D. As texturas do Silent Hill têm ruído de
   cor (fotos com grão, paleta de 4/8 bits); no PS1 a TV recebia vídeo composto/RF, que
   borra a cor e deixa o brilho, e esse ruído não aparecia. Ampliado num monitor nítido,
   vira pontinhos verdes, vermelhos e azuis que piscam com o movimento. O shader de
   apresentação ganha um borrão só de cor (Cb/Cr BT.601) de +/-2 pixels do PS1 na
   horizontal e +/-1 linha na vertical, como o filtro de cor de uma TV PAL; o brilho
   (luma) fica intacto. Liga com gl_renderer_set_tv_chroma(1) (mod sh1.cor-de-tv).
   Não entra no hash do gerador de código (é runtime).

uso: python patches/psxrecomp_sh1.py [pasta do psxrecomp]
"""
import os
import sys

# ---------------------------------------------------------------- 1. compile_overlays.py
MARCA_OVL = 'silent-hill-pc: pair conflict'

ANTES_OVL = """                    stats.add_skip()
                    return
                key = _interior_fail_key("""
DEPOIS_OVL = """                    stats.add_skip()
                    return
                if status.startswith('fragment cache-key collision/stale pair'):
                    # silent-hill-pc: pair conflict. A DLL with this key (same
                    # entry + code CRC) was built earlier from different
                    # surrounding C; keep it (or the interpreter) instead of
                    # failing the run, which backs autocompile off for minutes.
                    print(f'  fragment 0x{entry:08X} kept cached/interpreted: '
                          f'{status}')
                    stats.add_skip()
                    return
                key = _interior_fail_key("""

# ---------------------------------------------------------------- 2. gpu_gl_renderer.c
MARCA_TV = 'silent-hill-pc: cor de TV'

TROCAS_TV = [
    # o uniform no shader de apresentação
    (r'''    "uniform int  u_chroma;   /* [video] fmv_chroma_smoothing (24-bit frames) */\n"
''',
     r'''    "uniform int  u_chroma;   /* [video] fmv_chroma_smoothing (24-bit frames) */\n"
    "uniform float u_tvchroma; /* silent-hill-pc: cor de TV (texels por pixel do PS1; 0 = desligada) */\n"
'''),
    # o borrão de cor, depois do da FMV e antes das scanlines
    (r'''    "    c.rgb = clamp(vec3(y + 1.402 * cc.y, y - 0.344136 * cc.x - 0.714136 * cc.y, y + 1.772 * cc.x), 0.0, 1.0);\n"
    "  }\n"
    "  c.rgb = psx_scanline(c.rgb, v_uv.y);\n"
''',
     r'''    "    c.rgb = clamp(vec3(y + 1.402 * cc.y, y - 0.344136 * cc.x - 0.714136 * cc.y, y + 1.772 * cc.x), 0.0, 1.0);\n"
    "  }\n"
    /* silent-hill-pc: cor de TV. Cb/Cr de uma janela binomial de 5x3 pixels do PS1
     * (1 4 6 4 1 na horizontal, 1 2 1 na vertical), luma do próprio pixel. */
    "  if (u_tvchroma > 0.0) {\n"
    "    vec2 ts = 1.0 / vec2(textureSize(u_tex, 0));\n"
    "    vec2 lo = min(u_uv_rect.xy, u_uv_rect.zw), hi = max(u_uv_rect.xy, u_uv_rect.zw);\n"
    "    vec2 cc = vec2(0.0); float ws = 0.0;\n"
    "    for (int j = -1; j <= 1; j++) for (int i = -2; i <= 2; i++) {\n"
    "      vec3 s = texture(u_tex, clamp(v_uv + vec2(float(i), float(j)) * u_tvchroma * ts, lo, hi)).rgb;\n"
    "      float w = (i == 0 ? 6.0 : (abs(i) == 1 ? 4.0 : 1.0)) * (j == 0 ? 2.0 : 1.0);\n"
    "      cc += w * vec2(dot(s, vec3(-0.168736, -0.331264, 0.5)), dot(s, vec3(0.5, -0.418688, -0.081312)));\n"
    "      ws += w;\n"
    "    }\n"
    "    cc /= ws;\n"
    "    float y = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
    "    c.rgb = clamp(vec3(y + 1.402 * cc.y, y - 0.344136 * cc.x - 0.714136 * cc.y, y + 1.772 * cc.x), 0.0, 1.0);\n"
    "  }\n"
    "  c.rgb = psx_scanline(c.rgb, v_uv.y);\n"
'''),
    # o estado e a função que o mod chama
    ('static GLint s_present_uChroma = -1;\n',
     'static GLint s_present_uChroma = -1;\n'
     'static GLint s_present_uTvChroma = -1;   /* silent-hill-pc: cor de TV */\n'
     'static int s_tv_chroma = 0;\n'),
    ('int gl_renderer_fmv_chroma_smoothing(void) { return s_fmv_chroma; }\n',
     'int gl_renderer_fmv_chroma_smoothing(void) { return s_fmv_chroma; }\n'
     '/* silent-hill-pc: cor de TV nos quadros 3D (o mod sh1.cor-de-tv liga na ativação) */\n'
     'void gl_renderer_set_tv_chroma(int on) {\n'
     '    GL_RT_SYNC("tv_chroma");\n'
     '    s_tv_chroma = on ? 1 : 0;\n'
     '}\n'),
    # a localização do uniform
    ('''            s_present_uChroma =
                p_glGetUniformLocation(s_present_prog, "u_chroma");
''',
     '''            s_present_uChroma =
                p_glGetUniformLocation(s_present_prog, "u_chroma");
            s_present_uTvChroma =
                p_glGetUniformLocation(s_present_prog, "u_tvchroma");
'''),
    # só nos quadros 3D (present_target_quad); zerado depois, para a FMV e o resto
    ('''    p_glUniform4f(s_present_uUvRect, u0, v0, u1, v1);
    p_glBindVertexArray(s_present_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glBindVertexArray(0);
    p_glUseProgram(0);
}
''',
     '''    p_glUniform4f(s_present_uUvRect, u0, v0, u1, v1);
    if (s_present_uTvChroma >= 0)
        p_glUniform1f(s_present_uTvChroma, s_tv_chroma ? (float)(src_scale > 1 ? src_scale : 1) : 0.f);
    p_glBindVertexArray(s_present_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (s_present_uTvChroma >= 0) p_glUniform1f(s_present_uTvChroma, 0.f);
    p_glBindVertexArray(0);
    p_glUseProgram(0);
}
'''),
]


def ler(arq):
    b = open(arq, 'rb').read()
    return b'\r\n' in b, b.decode('utf-8').replace('\r\n', '\n')


def gravar(arq, crlf, t):
    if crlf:
        t = t.replace('\n', '\r\n')
    open(arq, 'wb').write(t.encode('utf-8'))


def corrige_overlays(arq):
    crlf, t = ler(arq)
    if MARCA_OVL in t:
        return 'ja estava aplicado'
    n = t.count(ANTES_OVL)
    if n != 2:
        raise SystemExit('%s: o trecho a corrigir apareceu %d vezes (esperava 2); '
                         'a versão do PSXRecomp mudou?' % (arq, n))
    gravar(arq, crlf, t.replace(ANTES_OVL, DEPOIS_OVL))
    return 'corrigido'


def corrige_tv(arq):
    crlf, t = ler(arq)
    if MARCA_TV in t:
        return 'ja estava aplicado'
    for antes, depois in TROCAS_TV:
        n = t.count(antes)
        if n != 1:
            raise SystemExit('%s: um trecho a corrigir apareceu %d vezes (esperava 1); '
                             'a versão do PSXRecomp mudou? (%s)' % (arq, n, antes.strip()[:60]))
        t = t.replace(antes, depois)
    gravar(arq, crlf, t)
    return 'corrigido'


def main():
    raiz = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'psxrecomp')
    for rel in ('overlay_toolchain/compile_overlays.py', 'tools/compile_overlays.py'):
        arq = os.path.join(raiz, rel)
        if os.path.isfile(arq):
            print('psxrecomp_sh1: %s %s' % (rel, corrige_overlays(arq)))
    rel = 'runtime/src/gpu_gl_renderer.c'
    print('psxrecomp_sh1: %s %s' % (rel, corrige_tv(os.path.join(raiz, rel))))


if __name__ == '__main__':
    main()
