# Silent Hill — port nativo para PC

> **Projeto de fãs, sem fins lucrativos, sem relação com a Konami.** *Silent Hill* é marca registrada da Konami Digital Entertainment. Este repositório **não contém o jogo**: você precisa da **sua própria cópia** do disco (a versão europeia, **SLES-01514**).

[English summary below](#english-summary)

## O que é

Um port **nativo** do *Silent Hill* (PlayStation, 1999) para Windows. Não é um emulador: o código do jogo é **traduzido para C e compilado** para x86-64 (recompilação estática), em cima do [PSXRecomp](https://github.com/mstan/psxrecomp).

Como o código do jogo não pode ser distribuído, **o seu PC gera e compila a parte do jogo a partir do seu disco**. Um comando só faz tudo.

**Além do original:**

- **Controles modernos** (um mod que se liga e desliga): o analógico ou o direcional apontam para onde o Harry anda, em relação à câmera, em vez dos controles "tank". Mirar, menus e cenas continuam com o controle original. Desligado, o jogo fica como no PlayStation.
- Resolução interna acompanhando o monitor (cai sozinha se faltar desempenho) e sem o pontilhado (*dithering*) do PS1, que num monitor nítido vira grão colorido.
- **Cor de TV** (mod ligado por padrão): borra só a cor da imagem 3D, como a TV de tubo do PlayStation, e tira os pontinhos coloridos que piscam nas texturas sem perder nitidez.
- **50 FPS no gameplay** (mod ligado por padrão): o PlayStation europeu roda o gameplay a 25 fps; aqui ele vai a 50, na mesma velocidade de jogo.
- **Medidor de desempenho** (outro mod): **F10** mostra FPS, uso e temperatura da CPU e da GPU, VRAM e RAM no canto da tela.

## Como jogar

### Recomendado: pelo Otavix Launcher

O [Otavix Launcher](https://github.com/otavix010/otavix-launcher/releases/latest) junta tudo num lugar só: baixa este port, confere o Python, pede o disco, gera o jogo e abre com as opções de vídeo, os mods e os saves.

### À mão

**1. O que você precisa ter:** a sua cópia do *Silent Hill* **europeu** (SLES-01514, "Silent Hill (Europe) (En,Fr,De,Es,It)"), como **.cue + .bin** ou **.chd**, ou ainda compactada (.7z, .zip, .rar). O gerador confere o disco pelo hash: outras versões (americana, japonesa) não servem, porque os endereços do mod de controles são os da europeia.

**2. Instale o Python (uma vez só)**, num terminal:
```
winget install Python.Python.3.12
```
O resto o gerador baixa sozinho: o PSXRecomp (nas versões fixadas em `framework_pins.txt`), o compilador dele (cmake-clang-v1, para `%LOCALAPPDATA%\retcomm`) e, se o PC não tiver Git, o Git portátil (MinGit).

**3. Baixe este repositório** (botão *Code > Download ZIP*, ou `git clone`) e extraia numa pasta **sem espaços no caminho**, por exemplo `C:\silent-hill-pc`.

**4. Clique duas vezes em `gerar.cmd`** e escolha o disco. Ele:
- baixa o framework e o compilador;
- confere o disco;
- traduz o código do jogo para C (`generated\`, só no seu PC);
- compila (`build-release\SilentHill.exe`).

**5. Jogue com `jogar.cmd`.** Os saves (cartões de memória `card1.mcd` e `card2.mcd`) ficam em `saves\`, na pasta do port: gerar de novo ou atualizar não mexe neles.

Depois de atualizar o port, `gerar.cmd -RefazerDe 3` retraduz e recompila sem repetir as ferramentas e o disco.

## Controles modernos

O pacote `mods/preloaded/packages/sh1.controles-modernos` vem **ligado**. As escolhas ficam em `build-release\mods\state.toml` (o launcher tem a aba de mods). Opções:

| Opção | Padrão | O que faz |
|---|---|---|
| Manter a direção enquanto segurar o analógico | ligada | a direção vale para a câmera do momento em que você inclinou o analógico, até soltar ou virar mais de 45°; o Harry segue reto mesmo quando a câmera gira atrás dele ou corta |
| Correr com o analógico no fim do curso | ligada | todo inclinado corre, um pouco inclinado anda; o botão de correr do jogo continua valendo |
| Direcional também no modo moderno | ligada | o direcional e o teclado apontam a direção, em oito sentidos |
| Velocidade do giro | normal | suave, normal ou instantâneo |
| Zona morta do analógico | 25% | quanto o analógico precisa inclinar antes de o Harry andar |

## Cor de TV

O pacote `sh1.cor-de-tv` vem **ligado**. As texturas do Silent Hill têm ruído de cor (fotos com grão, paleta de 4 ou 8 bits). No PlayStation, a TV recebia vídeo composto/RF, que tem pouca banda para a cor: esse ruído sumia e o brilho continuava nítido. Ampliado num monitor nítido, ele vira pontinhos verdes, vermelhos e azuis que piscam com o movimento. O mod liga um borrão **só de cor** na apresentação dos quadros 3D (a cor de cada ponto vem de uma janela de 5x3 pixels do PS1, como no filtro de cor de uma TV PAL); o brilho fica intacto, e vídeos e menus não mudam. Medido nas demonstrações da tela de título: o ruído de cor caiu de 50% a 70% e o detalhe de brilho ficou igual. O borrão é uma correção nossa no shader do PSXRecomp (`patches/psxrecomp_sh1.py`, aplicada pelo gerador).

## Fluidez 50 FPS

O pacote `sh1.fluidez` vem **ligado**. O laço principal do jogo espera dois VBlanks por quadro no gameplay (25 fps no PAL) e mede o tempo real de cada quadro pelas linhas de vídeo; movimento, física e animação usam esse tempo. O mod faz o jogo esperar um VBlank só, e o `[timing] guest_cycle_scale = 2` do `game.toml` deixa a CPU emulada duas vezes mais rápida enquanto o mod está ligado (vídeo, CD, som e timers seguem o tempo do hardware), porque o quadro do gameplay não cabia num VBlank do PS1. Resultado: 50 fps, e o Harry anda a mesma distância no mesmo tempo. Menus, vídeos e as demonstrações da tela de título não mudam. Desligado, é o ritmo original.

## Medidor de desempenho

O pacote `sh1.medidor` também vem ligado. **F10** mostra ou esconde a linha `FPS · CPU · GPU · VRAM · RAM` (as temperaturas só aparecem quando o Windows as informa). É o medidor padrão dos ports Otavix (`src/otavix_osd.c`): desenhado numa camada própria por cima do jogo, na resolução real da tela, com o tamanho de referência de 1080p (fonte de 16 px numa área de 1080 linhas, proporcional nas outras), nítido e igual qualquer que seja a resolução interna do jogo. Aparece em janela e em tela cheia sem bordas; na tela cheia exclusiva, não. `SH_OSD=1` abre o jogo já com ela visível (é o que a opção do launcher faz) e `SH_OSD_TECLA=0` desliga a tecla. Uma cópia da linha vai para o log alguns segundos depois de ligar, útil para relatar problemas.

## Movimento suave

O pacote `sh1.movimento-suave` vem **desligado**. Ligado, mistura os quadros prontos do jogo na taxa do monitor (ou em 90/120/144/165/240 fps), para a imagem andar mais lisa num monitor rápido. O jogo continua no ritmo original: é uma transição entre quadros, sem vetores de movimento. A mistura **adaptativa** (padrão) corta limpo nas mudanças grandes, para não deixar rastro dobrado; a **linear** mistura sempre. Com ele ligado, a sincronização vertical do driver fica desligada (o próprio apresentador cuida do ritmo).

## Como funciona

- **PSXRecomp** traduz o executável do PS1 (MIPS R3000A) para C e compila com o runtime dele (GPU em OpenGL, GTE, SPU, CD, vídeos MDEC e áudio XA). Os módulos de código que o jogo carrega do disco durante o jogo são traduzidos e compilados na hora pelo próprio runtime.
- **O mod de controles** (`src/controles_modernos.c`) lê o controle a cada quadro: com o analógico inclinado, calcula a direção em relação à câmera, gira o Harry e entrega ao jogo só "para frente" (e "correr"). Os endereços da versão europeia vêm do mapa de símbolos da [decompilação do Silent Hill](https://github.com/Vatuu/silent-hill-decomp) (`configs/EUR`).
- **Opções do launcher** que o runtime só leria pelo mini-launcher do PSXRecomp: `src/opcoes_launcher.c` (por enquanto, `SH_VOLUME`).
- **O medidor** (`src/medidor.c`) usa o tempo de quadro do próprio jogo para o FPS, os contadores de GPU do Windows para este processo e a DXGI para a VRAM.

## Estado atual

Em desenvolvimento. Testado: abertura, menus, começo de jogo e a primeira área jogável, com os controles modernos. O jogo inteiro ainda não foi verificado.

## Licença e créditos

- **O nosso código** (`src/`, `mods/preloaded/packages/sh1.*`, `gerar.ps1`, `gerar.cmd`): [MIT](LICENSE).
- **PSXRecomp** e **recomp-ui** (submódulos, baixados pelo gerador): [PolyForm Noncommercial 1.0.0](https://github.com/mstan/psxrecomp), de mstan e RetroPortingToolKit. Os arquivos que o modelo de projeto do PSXRecomp gerou (`CMakeLists.txt`, `codegen_setup.*`, `tools/`, `scripts/`) seguem a licença dele. Por isso este port, como o framework, **não pode ser usado comercialmente**.
- **Decompilação do Silent Hill** ([Vatuu/silent-hill-decomp](https://github.com/Vatuu/silent-hill-decomp)): referência para os endereços e estruturas usados pelo mod.
- *Silent Hill* é marca registrada da Konami Digital Entertainment. Este é um projeto de fãs, sem fins lucrativos, sem relação com a Konami nem endossado por ela.

## English summary

A **native PC port** of *Silent Hill* (PlayStation, 1999), made by **static recompilation** with [PSXRecomp](https://github.com/mstan/psxrecomp), plus an optional **modern controls** mod (camera-relative analog and D-pad movement instead of tank controls; aiming, menus and cutscenes keep the original controls) and a **performance overlay** (F10: FPS, CPU, GPU, VRAM, RAM).

**Bring your own disc**: no game code or data here. You need the **European** release (SLES-01514, Redump "Silent Hill (Europe) (En,Fr,De,Es,It)") as .cue/.bin or .chd (or a .7z/.zip/.rar with them). Install Python (`winget install Python.Python.3.12`), download this repository to a path without spaces, double-click **`gerar.cmd`** (it fetches the pinned PSXRecomp and its compiler, checks the disc, translates the game code on your PC and builds `build-release\SilentHill.exe`), then play with **`jogar.cmd`**. Easiest: the [Otavix Launcher](https://github.com/otavix010/otavix-launcher/releases/latest).

License: our code is MIT; PSXRecomp and recomp-ui are PolyForm Noncommercial 1.0.0, so this port is non-commercial too. *Silent Hill* is a trademark of Konami Digital Entertainment; this is a non-commercial fan project, not affiliated with or endorsed by Konami.
