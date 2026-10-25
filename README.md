# Silent Hill — port nativo para PC

> **Projeto de fãs, sem fins lucrativos, sem relação com a Konami.** *Silent Hill* é marca registrada da Konami Digital Entertainment. Este repositório **não contém o jogo**: você precisa da **sua própria cópia** do disco (a versão europeia, **SLES-01514**).

[English summary below](#english-summary)

## O que é

Um port **nativo** do *Silent Hill* (PlayStation, 1999) para Windows. Não é um emulador: o código do jogo é **traduzido para C e compilado** para x86-64 (recompilação estática), em cima do [PSXRecomp](https://github.com/mstan/psxrecomp).

Como o código do jogo não pode ser distribuído, **o seu PC gera e compila a parte do jogo a partir do seu disco**. Um comando só faz tudo.

**Além do original:**

- **Controles modernos** (um mod que se liga e desliga): o analógico ou o direcional apontam para onde o Harry anda, em relação à câmera, em vez dos controles "tank". Mirar, menus e cenas continuam com o controle original. Desligado, o jogo fica como no PlayStation.
- Resolução interna acompanhando o monitor (cai sozinha se faltar desempenho) e sem o pontilhado (*dithering*) do PS1, que num monitor nítido vira grão colorido.

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

**5. Jogue com `jogar.cmd`.** Os saves (cartão de memória) ficam em `build-release\saves\`.

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

## Como funciona

- **PSXRecomp** traduz o executável do PS1 (MIPS R3000A) para C e compila com o runtime dele (GPU em OpenGL, GTE, SPU, CD, vídeos MDEC e áudio XA). Os módulos de código que o jogo carrega do disco durante o jogo são traduzidos e compilados na hora pelo próprio runtime.
- **O mod de controles** (`src/controles_modernos.c`) lê o controle a cada quadro: com o analógico inclinado, calcula a direção em relação à câmera, gira o Harry e entrega ao jogo só "para frente" (e "correr"). Os endereços da versão europeia vêm do mapa de símbolos da [decompilação do Silent Hill](https://github.com/Vatuu/silent-hill-decomp) (`configs/EUR`).
- **Opções do launcher** que o runtime só leria pelo mini-launcher do PSXRecomp: `src/opcoes_launcher.c` (por enquanto, `SH_VOLUME`).

## Estado atual

Em desenvolvimento. Testado: abertura, menus, começo de jogo e a primeira área jogável, com os controles modernos. O jogo inteiro ainda não foi verificado.

## Licença e créditos

- **O nosso código** (`src/`, `mods/preloaded/packages/sh1.*`, `gerar.ps1`, `gerar.cmd`): [MIT](LICENSE).
- **PSXRecomp** e **recomp-ui** (submódulos, baixados pelo gerador): [PolyForm Noncommercial 1.0.0](https://github.com/mstan/psxrecomp), de mstan e RetroPortingToolKit. Os arquivos que o modelo de projeto do PSXRecomp gerou (`CMakeLists.txt`, `codegen_setup.*`, `tools/`, `scripts/`) seguem a licença dele. Por isso este port, como o framework, **não pode ser usado comercialmente**.
- **Decompilação do Silent Hill** ([Vatuu/silent-hill-decomp](https://github.com/Vatuu/silent-hill-decomp)): referência para os endereços e estruturas usados pelo mod.
- *Silent Hill* é marca registrada da Konami Digital Entertainment. Este é um projeto de fãs, sem fins lucrativos, sem relação com a Konami nem endossado por ela.

## English summary

A **native PC port** of *Silent Hill* (PlayStation, 1999), made by **static recompilation** with [PSXRecomp](https://github.com/mstan/psxrecomp), plus an optional **modern controls** mod (camera-relative analog and D-pad movement instead of tank controls; aiming, menus and cutscenes keep the original controls).

**Bring your own disc**: no game code or data here. You need the **European** release (SLES-01514, Redump "Silent Hill (Europe) (En,Fr,De,Es,It)") as .cue/.bin or .chd (or a .7z/.zip/.rar with them). Install Python (`winget install Python.Python.3.12`), download this repository to a path without spaces, double-click **`gerar.cmd`** (it fetches the pinned PSXRecomp and its compiler, checks the disc, translates the game code on your PC and builds `build-release\SilentHill.exe`), then play with **`jogar.cmd`**. Easiest: the [Otavix Launcher](https://github.com/otavix010/otavix-launcher/releases/latest).

License: our code is MIT; PSXRecomp and recomp-ui are PolyForm Noncommercial 1.0.0, so this port is non-commercial too. *Silent Hill* is a trademark of Konami Digital Entertainment; this is a non-commercial fan project, not affiliated with or endorsed by Konami.
