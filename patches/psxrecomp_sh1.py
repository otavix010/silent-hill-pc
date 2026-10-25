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

uso: python patches/psxrecomp_sh1.py [pasta do psxrecomp]
"""
import os
import sys

MARCA = 'silent-hill-pc: pair conflict'

ANTES = """                    stats.add_skip()
                    return
                key = _interior_fail_key("""
DEPOIS = """                    stats.add_skip()
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


def corrige(arq):
    b = open(arq, 'rb').read()
    crlf = b'\r\n' in b
    t = b.decode('utf-8').replace('\r\n', '\n')
    if MARCA in t:
        return 'ja estava aplicado'
    n = t.count(ANTES)
    if n != 2:
        raise SystemExit('%s: o trecho a corrigir apareceu %d vezes (esperava 2); '
                         'a versão do PSXRecomp mudou?' % (arq, n))
    t = t.replace(ANTES, DEPOIS)
    if crlf:
        t = t.replace('\n', '\r\n')
    open(arq, 'wb').write(t.encode('utf-8'))
    return 'corrigido'


def main():
    raiz = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'psxrecomp')
    for rel in ('overlay_toolchain/compile_overlays.py', 'tools/compile_overlays.py'):
        arq = os.path.join(raiz, rel)
        if os.path.isfile(arq):
            print('psxrecomp_sh1: %s %s' % (rel, corrige(arq)))


if __name__ == '__main__':
    main()
