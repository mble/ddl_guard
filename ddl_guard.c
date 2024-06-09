#include "postgres.h"
#include "utils/guc.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

void		_PG_init(void);
void		_PG_fini(void);

static bool ddl_guard_enabled = false;

PG_FUNCTION_INFO_V1(ddl_guard_check);

void
_PG_init(void)
{
	DefineCustomBoolVariable("ddl_guard.enabled", "Enable or disable DDL Guard",
							 NULL, &ddl_guard_enabled, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);
}

void
_PG_fini(void)
{								/* Nothing to cleanup */
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
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("Non-superusers are not allowed to execute DDL statements"),
				 errhint("ddl_guard.enabled is set.")));
	};

	PG_RETURN_VOID();
}
