/*
 * Silent Hill: medidor de desempenho na tela, como o do GoW Ascension e o do MSI
 * Afterburner. Fica escondido: F10 mostra e esconde (com a janela do jogo em foco), e
 * SH_OSD=1 já abre mostrando (a opção do Otavix Launcher). SH_OSD_TECLA=0 desliga a F10.
 *
 * Linha, a cada meio segundo: FPS do jogo e tempo de quadro, uso e temperatura da CPU e
 * da GPU, VRAM e RAM. FPS = 4096 / g_DeltaTime: o Silent Hill guarda a duração do quadro
 * que acabou de fazer em segundos Q19.12 (configs/EUR da decompilação), então é o FPS do
 * próprio jogo, não o da tela. O resto vem de uma thread que lê, a cada segundo, os mesmos
 * contadores do Gerenciador de Tarefas (PDH), a DXGI e o D3DKMT, como no GoW. O texto vai
 * para o medidor padrão dos ports Otavix (otavix_osd.c): nítido, desenhado na resolução da
 * tela e do tamanho de referência de 1080p, em vez do OSD do runtime (fonte 8x8 ampliada).
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dxgi1_4.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "otavix_osd.h"
#include "mod_plugins.h"

#define PLUGIN "sh1.medidor"
#define DELTA_TIME 0x800B52B0u      /* g_DeltaTime (Q19.12 s) */

static volatile LONG s_gpu_pct = -1, s_vram_mb = -1, s_vram_tot_mb = -1;
static volatile LONG s_temp_cpu = -1, s_temp_gpu = -1, s_ram_mb = -1, s_ram_tot_mb = -1;
static LUID s_luid;
static IDXGIAdapter3* s_adaptador;
static int s_ligado, s_tecla = 1, s_f10, s_amostrador;

/* D3DKMT (gdi32), estruturas de d3dkmthk.h: KMTQAITYPE_ADAPTERPERFDATA = 62, décimos de grau */
typedef struct { LUID luid; UINT h; } kmt_abre;
typedef struct { UINT h; int tipo; void* dados; UINT tam; } kmt_consulta;
typedef struct { UINT32 indice; ULONGLONG mf, mmf, mmfoc, mb, pcie; ULONG fan, potencia, temperatura; UCHAR ps; } kmt_perf;
static int temp_gpu(void)
{
    static LONG (APIENTRY *abre)(kmt_abre*);
    static LONG (APIENTRY *consulta)(const kmt_consulta*);
    static UINT h;
    static int pronto;
    if (!pronto) {
        pronto = -1;
        HMODULE m = GetModuleHandleA("gdi32.dll");
        if (m) {
            abre = (LONG (APIENTRY*)(kmt_abre*))(void*)GetProcAddress(m, "D3DKMTOpenAdapterFromLuid");
            consulta = (LONG (APIENTRY*)(const kmt_consulta*))(void*)GetProcAddress(m, "D3DKMTQueryAdapterInfo");
        }
        kmt_abre a; a.luid = s_luid; a.h = 0;
        if (abre && consulta && (s_luid.LowPart || s_luid.HighPart) && abre(&a) == 0) { h = a.h; pronto = 1; }
    }
    if (pronto != 1) return -1;
    kmt_perf p; memset(&p, 0, sizeof p);
    kmt_consulta q = { h, 62, &p, sizeof p };
    if (consulta(&q) != 0 || !p.temperatura) return -1;
    return (int)(p.temperatura / 10);
}

/* a placa do jogo: a de mais memória dedicada (o OpenGL não diz qual é) */
static void acha_adaptador(void)
{
    IDXGIFactory1* f = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&f))) return;
    SIZE_T melhor = 0;
    IDXGIAdapter1* a = NULL;
    for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory > melhor) {
            IDXGIAdapter3* a3 = NULL;
            if (SUCCEEDED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter3, (void**)&a3))) {
                if (s_adaptador) IDXGIAdapter3_Release(s_adaptador);
                s_adaptador = a3;
                s_luid = d.AdapterLuid;
                melhor = d.DedicatedVideoMemory;
                InterlockedExchange(&s_vram_tot_mb, (LONG)(d.DedicatedVideoMemory >> 20));
            }
        }
        IDXGIAdapter1_Release(a);
    }
    IDXGIFactory1_Release(f);
}

/* a cada segundo: GPU (motores 3D do processo, como o Gerenciador de Tarefas), temperaturas,
 * VRAM e RAM */
static DWORD WINAPI amostrador(LPVOID arg)
{
    (void)arg;
    acha_adaptador();
    wchar_t cam[128], luid_txt[48];
    swprintf(cam, 128, L"\\GPU Engine(pid_%lu*engtype_3D)\\Utilization Percentage", (unsigned long)GetCurrentProcessId());
    swprintf(luid_txt, 48, L"luid_0x%08lx_0x%08lx", (unsigned long)s_luid.HighPart, (unsigned long)s_luid.LowPart);
    PDH_HQUERY q = NULL;
    PDH_HCOUNTER c = NULL, ct = NULL;
    int idade = 0, ct_decimos = 1, ct_iguais = 0, ct_parada = 0;
    long ct_ult = -100000;
    for (;;) {
        if (!q || ++idade >= 10) {   /* as instâncias por processo aparecem depois: refaz a cada 10 s */
            if (q) PdhCloseQuery(q);
            q = NULL; c = NULL; ct = NULL; idade = 0;
            if (PdhOpenQueryW(NULL, 0, &q) == ERROR_SUCCESS) {
                if (PdhAddEnglishCounterW(q, cam, 0, &c) != ERROR_SUCCESS) c = NULL;
                ct_decimos = 1;
                if (PdhAddEnglishCounterW(q, L"\\Thermal Zone Information(*)\\High Precision Temperature", 0, &ct) != ERROR_SUCCESS) {
                    ct_decimos = 0;
                    if (PdhAddEnglishCounterW(q, L"\\Thermal Zone Information(*)\\Temperature", 0, &ct) != ERROR_SUCCESS) ct = NULL;
                }
                PdhCollectQueryData(q);
            }
        }
        Sleep(1000);
        if (q && PdhCollectQueryData(q) == ERROR_SUCCESS) {
            DWORD tam = 0, n = 0;
            if (c && PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &tam, &n, NULL) == PDH_MORE_DATA && tam) {
                PDH_FMT_COUNTERVALUE_ITEM_W* it = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(tam);
                if (it && PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &tam, &n, it) == ERROR_SUCCESS) {
                    double mx = -1;
                    for (DWORD k = 0; k < n; k++) {
                        wchar_t nome[256];
                        wcsncpy(nome, it[k].szName ? it[k].szName : L"", 255); nome[255] = 0;
                        for (wchar_t* w = nome; *w; w++) *w = (wchar_t)towlower(*w);
                        const int desta = (!s_luid.LowPart && !s_luid.HighPart) || wcsstr(nome, luid_txt);
                        if (desta && it[k].FmtValue.doubleValue > mx) mx = it[k].FmtValue.doubleValue;
                    }
                    if (mx >= 0) InterlockedExchange(&s_gpu_pct, (LONG)(mx > 100.0 ? 100.0 : mx + 0.5));
                }
                free(it);
            }
            tam = 0; n = 0;
            if (ct && PdhGetFormattedCounterArrayW(ct, PDH_FMT_DOUBLE, &tam, &n, NULL) == PDH_MORE_DATA && tam) {
                PDH_FMT_COUNTERVALUE_ITEM_W* it = (PDH_FMT_COUNTERVALUE_ITEM_W*)malloc(tam);
                if (it && PdhGetFormattedCounterArrayW(ct, PDH_FMT_DOUBLE, &tam, &n, it) == ERROR_SUCCESS) {
                    double mx = -1000;
                    for (DWORD k = 0; k < n; k++) {
                        const double cel = (ct_decimos ? it[k].FmtValue.doubleValue / 10.0 : it[k].FmtValue.doubleValue) - 273.15;
                        if (cel > mx) mx = cel;
                    }
                    /* zona parada: em muitos desktops a ACPI devolve um valor fixo, que não é o
                     * processador; 90 s sem mudar nem um décimo -> "--" (como no GoW) */
                    if (mx > 5.0 && mx < 125.0 && !ct_parada) {
                        const long dec = (long)(mx * 10.0 + 0.5);
                        if (dec != ct_ult) { ct_ult = dec; ct_iguais = 0; }
                        else if (++ct_iguais >= 90) { ct_parada = 1; InterlockedExchange(&s_temp_cpu, -1); }
                    }
                    if (mx > 5.0 && mx < 125.0 && !ct_parada) InterlockedExchange(&s_temp_cpu, (LONG)(mx + 0.5));
                }
                free(it);
            }
        }
        { const int tg = temp_gpu(); if (tg > 0 && tg < 125) InterlockedExchange(&s_temp_gpu, tg); }
        if (s_adaptador) {
            DXGI_QUERY_VIDEO_MEMORY_INFO mi;
            if (SUCCEEDED(IDXGIAdapter3_QueryVideoMemoryInfo(s_adaptador, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mi)))
                InterlockedExchange(&s_vram_mb, (LONG)(mi.CurrentUsage >> 20));
        }
        MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
        if (GlobalMemoryStatusEx(&ms)) {
            InterlockedExchange(&s_ram_mb, (LONG)((ms.ullTotalPhys - ms.ullAvailPhys) >> 20));
            InterlockedExchange(&s_ram_tot_mb, (LONG)(ms.ullTotalPhys >> 20));
        }
    }
    return 0;
}

static int janela_em_foco(void)
{
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

/* a cada vblank, na thread do jogo: F10, a média do tempo de quadro e, a cada meio segundo,
 * a linha do OSD */
static void quadro(void)
{
    if (s_tecla) {
        const int f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (f10 && !s_f10 && janela_em_foco()) {
            s_ligado = !s_ligado;
            if (!s_ligado) otx_osd_texto(NULL);
        }
        s_f10 = f10;
    }
    if (!s_ligado) return;
    if (!s_amostrador) { s_amostrador = 1; CreateThread(NULL, 0, amostrador, NULL, 0, NULL); }
    static double soma; static int n;
    static LONGLONG t0; static ULONGLONG i0, k0, u0;
    if (psx_mod_game_started()) {
        const int32_t dt = (int32_t)psx_mod_read_word(DELTA_TIME);
        if (dt > 0 && dt < 4096 * 2) { soma += dt; n++; }
    }
    LARGE_INTEGER agora, fr;
    QueryPerformanceCounter(&agora); QueryPerformanceFrequency(&fr);
    if (t0 && (agora.QuadPart - t0) * 2 < fr.QuadPart) return;
    FILETIME fi, fk, fu;
    ULONGLONG i1 = 0, k1 = 0, u1 = 0;
    if (GetSystemTimes(&fi, &fk, &fu)) {
        i1 = ((ULONGLONG)fi.dwHighDateTime << 32) | fi.dwLowDateTime;
        k1 = ((ULONGLONG)fk.dwHighDateTime << 32) | fk.dwLowDateTime;
        u1 = ((ULONGLONG)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
    }
    if (t0) {
        const ULONGLONG tot = (k1 - k0) + (u1 - u0), oc = tot - (i1 - i0);
        const int cpu = tot ? (int)(100.0 * (double)oc / (double)tot + 0.5) : 0;
        char fps[24] = "FPS --", gpu[16] = "--", tc[12] = "", tg[12] = "", vram[32] = "", ram[32] = "";
        if (n) {
            const double media = soma / n;           /* Q12 segundos por quadro */
            snprintf(fps, sizeof fps, "FPS %.1f %.1f ms", 4096.0 / media, 1000.0 * media / 4096.0);
        }
        if (s_gpu_pct >= 0) snprintf(gpu, sizeof gpu, "%ld%%", (long)s_gpu_pct);
        if (s_temp_cpu > 0) snprintf(tc, sizeof tc, " %ld\xC2\xB0" "C", (long)s_temp_cpu);   /* °C em UTF-8 */
        if (s_temp_gpu > 0) snprintf(tg, sizeof tg, " %ld\xC2\xB0" "C", (long)s_temp_gpu);
        if (s_vram_mb >= 0) snprintf(vram, sizeof vram, "  VRAM %.1f/%.1f GB", s_vram_mb / 1024.0, s_vram_tot_mb / 1024.0);
        if (s_ram_mb >= 0) snprintf(ram, sizeof ram, "  RAM %.1f/%.1f GB", s_ram_mb / 1024.0, s_ram_tot_mb / 1024.0);
        char linha[160];
        snprintf(linha, sizeof linha, "%s   CPU %d%%%s   GPU %s%s %s %s", fps, cpu, tc, gpu, tg, vram, ram);
        otx_osd_texto(linha);
        /* uma linha no log (~5 s depois de ligar, com o amostrador já rodando) para relatos */
        static int linhas;
        if (++linhas == 10) { printf("[medidor] %s\n", linha); fflush(stdout); }
    }
    soma = 0; n = 0;
    t0 = agora.QuadPart; i0 = i1; k0 = k1; u0 = u1;
}

static void ativa(void)
{
    const char* e = getenv("SH_OSD");
    s_ligado = e && *e == '1';
    e = getenv("SH_OSD_TECLA");
    s_tecla = !(e && *e == '0');
    otx_osd_iniciar();   /* a thread do medidor fica escondida até ter texto */
}

PSX_MOD_CONSTRUCTOR(sh1_registra_medidor)
{
    (void)psx_mod_register_activation_plugin(PLUGIN, ativa);
    (void)psx_mod_register_vblank_plugin(PLUGIN, quadro);
}
