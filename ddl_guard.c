#include "postgres.h"
#include "fmgr.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "pgstat.h"

#include <stdio.h>
#include <unistd.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define DDL_SENTINEL_FILE PG_STAT_TMP_DIR "/ddl_guard_ddl_sentinel"
#define LO_SENTINEL_FILE PG_STAT_TMP_DIR "/ddl_guard_lo_sentinel"

void		_PG_init(void);
void		_PG_fini(void);
void		write_sentinel_file(const char *filename);
Datum		ddl_guard_check(PG_FUNCTION_ARGS);
Datum		lo_guard_check(PG_FUNCTION_ARGS);

static bool ddl_guard_enabled = false;
static bool ddl_guard_ddl_sentinel = false;
static bool ddl_guard_lo_sentinel = false;

PG_FUNCTION_INFO_V1(ddl_guard_check);
PG_FUNCTION_INFO_V1(lo_guard_check);

void
_PG_init(void)
{
	DefineCustomBoolVariable("ddl_guard.enabled", "Enable or disable DDL Guard",
							 NULL, &ddl_guard_enabled, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	DefineCustomBoolVariable("ddl_guard.ddl_sentinel", "Write sentinel file for DDL statements",
							 NULL, &ddl_guard_ddl_sentinel, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	DefineCustomBoolVariable("ddl_guard.lo_sentinel", "Write sentinel file for pg_largeobject modifications",
							 NULL, &ddl_guard_lo_sentinel, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	unlink(DDL_SENTINEL_FILE);
	unlink(LO_SENTINEL_FILE);

	EmitWarningsOnPlaceholders("ddl_guard");
}

void
_PG_fini(void)
{								/* Nothing to cleanup */
}

void
write_sentinel_file(const char *filename)
{
	FILE	   *fp = NULL;

	fp = AllocateFile(filename, "w");
	if (fp == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("could not create sentinel file \"%s\": %m", filename)));

	if (FreeFile(fp))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("could not write sentinel file \"%s\": %m", filename)));
}

Datum
ddl_guard_check(PG_FUNCTION_ARGS)
{
	if (!CALLED_AS_EVENT_TRIGGER(fcinfo))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("ddl_guard_check: not fired by event trigger manager")));

	if (ddl_guard_enabled && !superuser())
	{
		if (ddl_guard_ddl_sentinel)
		{
			write_sentinel_file(DDL_SENTINEL_FILE);
			ereport(WARNING, (errmsg("ddl_guard: ddl detected, sentinel file written")));
			PG_RETURN_VOID();
		}
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("Non-superusers are not allowed to execute DDL statements"),
				 errhint("ddl_guard.enabled is set.")));
	};

	PG_RETURN_VOID();
}
