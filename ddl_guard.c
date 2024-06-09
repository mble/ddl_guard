#include "postgres.h"
#include "fmgr.h"
#include "utils/guc.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

void _PG_init(void);
void _PG_fini(void);

static bool ddl_guard_enabled = false;

void
_PG_init(void)
{
    DefineCustomBoolVariable(
        "ddl_guard.enabled",
        "Enable or disable DDL Guard",
        NULL,
        &ddl_guard_enabled,
        false,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );
}

void
_PG_fini(void)
{
    /* Nothing to cleanup */
}