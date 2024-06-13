#include "postgres.h"
#include "fmgr.h"
#include "utils/fmgrtab.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_largeobject.h"

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
static void lob_object_access_hook(ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg);

static bool ddl_guard_enabled = false;
static bool ddl_guard_ddl_sentinel = false;
static bool ddl_guard_lo_sentinel = false;
static object_access_hook_type next_object_access_hook = NULL;

/* large_object_funcs contains all the create/update/destroy lobject funcs */
static const char *large_object_funcs[] = {
	"lo_create",
	"lo_creat",
	"lo_truncate",
	"lo_truncate64",
	"lo_unlink",
	"lowrite",
	"lo_from_bytea",
	"be_lowrite",
	"be_lo_create",
	"be_lo_creat",
	"be_lo_unlink",
	"be_lo_truncate",
	"be_lo_from_bytea",
	"be_lo_truncate64",
	"be_lo_import",
	"be_lo_import_with_oid",
};
static const int lobject_funcs_count = sizeof(large_object_funcs) / sizeof(large_object_funcs[0]);
static Oid *lobject_func_oids;
static int	max_reserved_oid = 0;
static int	min_reserved_oid = 9000;

PG_FUNCTION_INFO_V1(ddl_guard_check);

/* regards, tom lane */
static const FmgrBuiltin *
fmgr_lookupByName(const char *name)
{
	int			i;

	for (i = 0; i < fmgr_nbuiltins; i++)
	{
		/* switched to strncmp */
		/* current max len is 22 in large_object_funcs */
		if (strncmp(name, fmgr_builtins[i].funcName, 22) == 0)
			return fmgr_builtins + i;
	}

	return NULL;
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

static bool
set_lobject_func_oids()
{
	const FmgrBuiltin *entry;
	int			i;

	lobject_func_oids = (Oid *) malloc(lobject_funcs_count * sizeof(Oid));
	if (lobject_func_oids == NULL)
	{
		return false;
	}

	for (i = 0; i < lobject_funcs_count; i++)
	{
		if ((entry = fmgr_lookupByName(large_object_funcs[i])) != NULL)
		{
			lobject_func_oids[i] = entry->foid;
			if (entry->foid < min_reserved_oid)
			{
				min_reserved_oid = entry->foid;
			}
			else if (entry->foid > max_reserved_oid)
			{
				max_reserved_oid = entry->foid;
			}
		}
	}

	return true;
}

static void
lob_object_access_hook(ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg)
{
	int			i;
	const FmgrBuiltin *entry;

	if (ddl_guard_enabled && !superuser())
	{
		if (ddl_guard_lo_sentinel)
		{
			switch (access)
			{
				case OAT_FUNCTION_EXECUTE:
					/* look up the func and log that shit */
					if (objectId >= min_reserved_oid && objectId <= max_reserved_oid)
					{
						for (i = 0; i < lobject_funcs_count; i++)
						{
							if (lobject_func_oids[i] == objectId)
							{
								/* ooh that's a bingo! */
								if ((entry = fmgr_lookupByName(large_object_funcs[i])) != NULL)
								{
									write_sentinel_file(LO_SENTINEL_FILE);
									ereport(WARNING, errmsg("lo_guard: lobject \"%s\" function call, sentinel file written", entry->funcName));
								}
							}
						}
					}
				default:
					break;
			}
		}
	}
	if (next_object_access_hook)
		(*next_object_access_hook) (access, classId, objectId, subId, arg);
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

	if (set_lobject_func_oids())
	{
		next_object_access_hook = object_access_hook;
		object_access_hook = lob_object_access_hook;
	}
	else
	{
		ereport(ERROR, errmsg("We beefed it, chief."));
	}

	EmitWarningsOnPlaceholders("ddl_guard");
}

void
_PG_fini(void)
{
	if (lobject_func_oids != NULL)
	{
		free(lobject_func_oids);
	}

	object_access_hook = next_object_access_hook;
}
