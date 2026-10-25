/*
 * Silent Hill: opções que o Otavix Launcher passa por variável de ambiente e que o
 * runtime só aplicaria pelo mini-launcher do framework (que o nosso launcher pula com
 * PSX_NO_LAUNCHER=1).
 *   SH_VOLUME=0..100   volume geral (sem ela, o runtime começa em 100)
 */
#include <stdlib.h>

#include "mod_plugins.h"

extern void host_volume_set(int percent);

PSX_MOD_CONSTRUCTOR(sh1_opcoes_launcher)
{
    const char* v = getenv("SH_VOLUME");
    if (v && *v) host_volume_set(atoi(v));
}
