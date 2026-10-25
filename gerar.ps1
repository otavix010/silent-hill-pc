<#
.SYNOPSIS
  Gera e compila o Silent Hill (PlayStation, versão europeia) a partir da SUA cópia do disco.

.DESCRIPTION
  Faz tudo em ordem, e cada etapa pula se já foi feita (use -Refazer para refazer):
    1. confere o Python, baixa o PSXRecomp (o framework, nas versões fixadas) e o
       compilador dele (cmake-clang-v1);
    2. confere o disco: tem que ser o Silent Hill europeu, SLES-01514 (cópia Redump);
    3. traduz o código do jogo para C (generated\), no seu PC;
    4. compila (build-release\SilentHill.exe);
    5. deixa o jogar.cmd pronto.
  Os logs de cada etapa ficam em out\gerar\. Nada do jogo vem neste repositório: o
  código traduzido é feito aqui, a partir do seu disco.

.PARAMETER Disco
  O .cue (com o .bin ao lado) ou o .chd do disco, a pasta que contém um deles, ou um
  arquivo .7z/.zip/.rar com eles (extraído uma vez para disco\).
.PARAMETER RefazerDe
  Refaz da etapa N em diante. Depois de atualizar o port, -RefazerDe 3 retraduz e
  recompila sem repetir as ferramentas e o disco.
.EXAMPLE
  gerar.cmd     (clique duas vezes: pede o disco numa janela)
.EXAMPLE
  .\gerar.ps1 -Disco "D:\Jogos\Silent Hill (Europe) (En,Fr,De,Es,It).cue"
#>
param(
    [string]$Disco = '',
    [int]$RefazerDe = 0,
    [switch]$Refazer
)
$ErrorActionPreference = 'Stop'
$R = $PSScriptRoot
if ($R -match ' ') { throw "Coloque o projeto numa pasta sem espaços (agora: $R)" }
# versões fixadas do framework (submódulos) e do Git portátil, se o PC não tiver Git
$PSXRECOMP_URL = 'https://github.com/mstan/psxrecomp.git'
$PSXRECOMP_SHA = '7097d5367d37e228ce95e4d2b38739d619904fd7'
$RECOMPUI_URL = 'https://github.com/RetroPortingToolKit/recomp-ui.git'
$RECOMPUI_SHA = '3f1d6f122a58823614abce14f5b86f4d165a629c'
$MINGIT_URL = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.3/MinGit-2.56.0.3-64-bit.zip'
$OUT = Join-Path $R 'out\gerar'
New-Item -ItemType Directory -Force $OUT | Out-Null
if ($RefazerDe -gt 0) {
    Get-ChildItem $OUT -Filter '*.ok' | Where-Object { [int]($_.Name.Split('_')[0]) -ge $RefazerDe } | Remove-Item -Force
}

function Msg($t) { Write-Host "[gerar] $t" -ForegroundColor Cyan }
# roda um programa externo sem que o stderr dele vire exceção (Windows PowerShell 5.1 com
# ErrorActionPreference = Stop lança antes do teste do $LASTEXITCODE); quem chama confere o código
function SemExcecao([scriptblock]$bloco) {
    $eap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $bloco } finally { $ErrorActionPreference = $eap }
}
# escreve no log por um FileStream compartilhado, tentando de novo: o launcher lê o log da etapa
# a cada 500 ms, e no PowerShell 5.1 o Add-Content/Set-Content falham ("sendo usado por outro
# processo") com o arquivo aberto por outro, mesmo só para leitura
function Anotar([string]$arq, [string]$texto, [switch]$Novo) {
    $b = [Text.Encoding]::UTF8.GetBytes("$texto`r`n")
    for ($i = 0; ; $i++) {
        try {
            $fs = New-Object IO.FileStream($arq, $(if ($Novo) { 'Create' } else { 'Append' }), 'Write', 'ReadWrite')
            try { $fs.Write($b, 0, $b.Length) } finally { $fs.Dispose() }
            return
        } catch [IO.IOException] { if ($i -ge 50) { throw }; Start-Sleep -Milliseconds 100 }
    }
}
# roda um comando pelo cmd (stdout e stderr no log da etapa); falha = exceção com o fim do log
function Exec([string]$log, [string]$linha) {
    Anotar $log "`r`n> $linha"
    cmd /c "$linha >> `"$log`" 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host (Get-Content $log -Tail 25 | Out-String) -ForegroundColor DarkGray
        throw "falhou (código $LASTEXITCODE): $linha`r`nlog completo: $log"
    }
}
function Etapa([string]$nome, [scriptblock]$corpo) {
    $marca = Join-Path $OUT "$nome.ok"
    if ((Test-Path $marca) -and -not $Refazer) { Msg "${nome}: já feita (pulando)"; return }
    $script:LOG = Join-Path $OUT "$nome.log"
    Anotar $script:LOG "== $nome $(Get-Date)" -Novo
    Msg "$nome ..."
    $t0 = Get-Date
    & $corpo
    Set-Content -Path $marca -Value (Get-Date) -Encoding UTF8
    Msg ("{0}: ok ({1:N0} s)" -f $nome, ((Get-Date) - $t0).TotalSeconds)
}
function Escolher-Disco([string]$texto) {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $d = New-Object System.Windows.Forms.OpenFileDialog
        $d.Title = $texto
        $d.Filter = 'Disco do PS1 (*.cue;*.chd;*.7z;*.zip;*.rar)|*.cue;*.chd;*.7z;*.zip;*.rar'
        if ($d.ShowDialog() -eq 'OK') { return $d.FileName }
        return ''
    } catch { return (Read-Host $texto) }
}
$marcaDisco = Join-Path $OUT '2_disco.ok'
if (-not $Disco -and ($Refazer -or -not (Test-Path $marcaDisco))) {
    $Disco = Escolher-Disco 'Escolha o disco do Silent Hill europeu (o .cue, o .chd ou o .7z/.zip com eles)'
    if (-not $Disco) { throw 'sem o disco' }
}

# ---------------------------------------------------------------------------- ferramentas
# git do PC, ou o MinGit portátil baixado para dl\mingit
function Achar-Git {
    $g = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($g) { return $g.Source }
    $m = Join-Path $R 'dl\mingit\cmd\git.exe'
    if (Test-Path -LiteralPath $m) { return $m }
    return $null
}
# um submódulo na versão fixada (com os submódulos dele)
function Submodulo([string]$git, [string]$pasta, [string]$url, [string]$sha) {
    $p = Join-Path $R $pasta
    if (Test-Path -LiteralPath (Join-Path $p '.git')) {
        $atual = (SemExcecao { & $git -C $p rev-parse HEAD 2>$null })
        if ($atual -eq $sha) { return }
    } else {
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }
        Exec $LOG "`"$git`" clone --quiet $url `"$p`""
    }
    # --force: descarta as correções de patches\ (aplicadas de novo depois) se a versão mudar
    Exec $LOG "`"$git`" -C `"$p`" -c advice.detachedHead=false checkout --force --quiet $sha"
    Exec $LOG "`"$git`" -C `"$p`" submodule update --init --recursive --quiet"
}
# correções nossas no PSXRecomp baixado (patches\psxrecomp_sh1.py; pode rodar de novo) e o
# recompilador refeito em cima delas: o tools\compile_overlays.py entra no hash do gerador de
# código, e o runtime recusa (todas as fases no interpretador) um psxrecomp-game com o hash de
# antes. O ensure-emitters é incremental: sem mudança, leva segundos.
function Corrigir-Framework {
    Exec $LOG "python `"$R\patches\psxrecomp_sh1.py`" `"$R\psxrecomp`""
    Exec $LOG "python `"$R\psxrecomp\psxrecomp_cli.py`" ensure-emitters --project-root `"$R`""
}
Etapa '1_ferramentas' {
    # "python" pode ser o atalho da Microsoft Store (abre a loja, não roda nada)
    $v = ''
    try { $v = (cmd /c 'python -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>nul') } catch { }
    if (-not ($v -match '^\d+$')) { throw 'Falta instalar o Python 3 (winget install Python.Python.3.12) -- e feche/abra o terminal depois' }
    if ([int]$v -lt 310) { throw 'Precisa do Python 3.10 ou mais novo (winget install Python.Python.3.12)' }
    $git = Achar-Git
    if (-not $git) {
        Msg "baixando o Git portátil (MinGit) de $MINGIT_URL"
        $zip = Join-Path $R 'dl\mingit.zip'
        New-Item -ItemType Directory -Force (Join-Path $R 'dl') | Out-Null
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $MINGIT_URL -OutFile $zip -UseBasicParsing
        Expand-Archive -Path $zip -DestinationPath (Join-Path $R 'dl\mingit') -Force
        Remove-Item $zip
        $git = Achar-Git
        if (-not $git) { throw 'o Git portátil não ficou em dl\mingit' }
    }
    Msg 'baixando o PSXRecomp e a interface dele (versões fixadas)'
    Submodulo $git 'psxrecomp' $PSXRECOMP_URL $PSXRECOMP_SHA
    Submodulo $git 'recomp-ui' $RECOMPUI_URL $RECOMPUI_SHA
    Msg 'compilador do PSXRecomp (cmake-clang-v1; baixa uma vez para %LOCALAPPDATA%\retcomm)'
    Exec $LOG "python `"$R\psxrecomp\psxrecomp_cli.py`" ensure-toolchain --project-root `"$R`""
}

# ---------------------------------------------------------------------------- disco
# o .cue ou o .chd: o próprio arquivo, o primeiro numa pasta (até dois níveis), ou dentro de
# um .7z/.zip/.rar extraído uma vez para disco\ (o tar do Windows lê os três)
function Achar-Imagem([string]$raiz) {
    $a = Get-ChildItem -LiteralPath $raiz -File -Recurse -Depth 2 -ErrorAction SilentlyContinue |
         Where-Object { $_.Extension -in '.cue', '.chd' } | Sort-Object { $_.Extension -ne '.cue' } | Select-Object -First 1
    if ($a) { return $a.FullName }
    return $null
}
function Achar-Disco([string]$d) {
    if (Test-Path -LiteralPath $d -PathType Container) {
        $i = Achar-Imagem $d
        if ($i) { return $i }
        $arq = @(Get-ChildItem -LiteralPath $d -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.7z', '.zip', '.rar' })
        if ($arq.Count -eq 1) { return (Achar-Disco $arq[0].FullName) }
        throw "não achei um .cue ou .chd em $d"
    }
    if (-not (Test-Path -LiteralPath $d -PathType Leaf)) { throw "não achei $d" }
    if ($d -match '\.(cue|chd)$') { return $d }
    if ($d -match '\.bin$') {
        $cue = [IO.Path]::ChangeExtension($d, '.cue')
        if (Test-Path -LiteralPath $cue) { return $cue }
        throw "escolha o .cue que acompanha $d"
    }
    if ($d -notmatch '\.(7z|zip|rar)$') { throw "o disco tem que ser o .cue, o .chd, a pasta com eles ou um .7z/.zip/.rar ($d)" }
    $dx = Join-Path $R 'disco'
    $achou = $null
    if (Test-Path -LiteralPath $dx) { $achou = Achar-Imagem $dx }
    if (-not $achou) {
        Msg "extraindo $d para $dx"
        New-Item -ItemType Directory -Force $dx | Out-Null
        SemExcecao { & "$env:SystemRoot\System32\tar.exe" -xf $d -C $dx }
        if ($LASTEXITCODE -ne 0) { throw "falhou a extração de $d (tar $LASTEXITCODE). Extraia o arquivo numa pasta e escolha o .cue." }
        $achou = Achar-Imagem $dx
    }
    if (-not $achou) { throw "não achei um .cue ou .chd dentro de $d" }
    return $achou
}
$imagem = Join-Path $OUT 'disco.txt'   # o disco escolhido, para as etapas seguintes
Etapa '2_disco' {
    $img = Achar-Disco $Disco
    Msg "disco: $img"
    SemExcecao { cmd /c "python `"$R\psxrecomp\psxrecomp_cli.py`" verify-disc --config `"$R\game.toml`" --project-root `"$R`" --disc `"$img`" >> `"$LOG`" 2>&1" }
    if ($LASTEXITCODE -eq 3) {
        throw "este disco não é o Silent Hill europeu (SLES-01514) esperado: o port só serve para essa versão (a cópia Redump, 'Silent Hill (Europe) (En,Fr,De,Es,It)'). Veja $LOG"
    }
    if ($LASTEXITCODE -ne 0) { throw "falhou a conferência do disco (código $LASTEXITCODE). Veja $LOG" }
    Set-Content -LiteralPath $imagem -Value $img -Encoding UTF8
}
$img = if (Test-Path -LiteralPath $imagem) { (Get-Content -LiteralPath $imagem -Encoding UTF8 | Select-Object -First 1) } else { $Disco }

# ---------------------------------------------------------------------------- tradução e compilação
Etapa '3_traduzir' {
    Corrigir-Framework   # também no -RefazerDe 3, depois de atualizar o port
    Msg 'traduzindo o código do jogo para C (generated\)'
    Exec $LOG "python `"$R\psxrecomp\psxrecomp_cli.py`" generate --config `"$R\game.toml`" --project-root `"$R`" --disc `"$img`""
}
Etapa '4_compilar' {
    Corrigir-Framework   # também no -RefazerDe 4
    Msg 'compilando (leva alguns minutos)'
    Exec $LOG "python `"$R\psxrecomp\psxrecomp_cli.py`" rebuild --config `"$R\game.toml`" --project-root `"$R`" --build-dir `"$R\build-release`" --no-pgo"
    if (-not (Test-Path -LiteralPath (Join-Path $R 'build-release\SilentHill.exe'))) { throw 'a compilação terminou sem o build-release\SilentHill.exe' }
    # recompilador e runtime com o mesmo hash, senão o jogo recusa compilar as fases e roda
    # tudo no interpretador (bem mais lento) sem avisar ninguém
    $tk = Join-Path $R 'build-release\overlay_toolchain'
    $hRec = "$(SemExcecao { & (Join-Path $tk 'psxrecomp-game.exe') --codegen-hash 2>$null } | Select-Object -First 1)".Trim()
    $hRun = ''
    $hh = Join-Path $tk 'include\overlay_codegen_hash.h'
    if ((Test-Path -LiteralPath $hh) -and ((Get-Content -LiteralPath $hh -Raw) -match 'PSX_OVERLAY_CODEGEN_HASH\s+0x([0-9A-Fa-f]+)')) { $hRun = $Matches[1] }
    if ($hRec -and $hRun -and ($hRec -ne $hRun)) {
        throw "o recompilador (hash $hRec) não bate com o runtime (hash $hRun): as fases rodariam no interpretador. Rode de novo com -RefazerDe 3."
    }
    Msg "recompilador e runtime conferidos (hash $hRun)"
}
Etapa '5_jogar' {
    $j = @(
        '@echo off',
        'rem Abre o Silent Hill gerado (sem o mini-launcher do PSXRecomp; as opções ficam no Otavix Launcher).',
        'set "PSX_NO_LAUNCHER=1"',
        'start "" "%~dp0build-release\SilentHill.exe" %*'
    )
    Set-Content -LiteralPath (Join-Path $R 'jogar.cmd') -Value $j -Encoding ASCII
}
Msg "pronto. Para jogar: $R\jogar.cmd"
