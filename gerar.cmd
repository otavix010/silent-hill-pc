@echo off
rem Clique duas vezes: gera e compila o Silent Hill a partir da sua copia do disco (veja o README).
rem Pede o disco numa janela; tambem aceita os parametros do gerar.ps1 (-Disco -RefazerDe ...).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0gerar.ps1" %*
echo.
pause
