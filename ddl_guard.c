#include "postgres.h"
#include "fmgr.h"
#include "utils/guc.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_class.h"
#include "catalog/pg_language.h"
#include "catalog/pg_proc.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "tcop/utility.h"


#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

void		_PG_init(void);
void		_PG_fini(void);
void		log_sentinel_event(const char *volatile operation);
Datum		ddl_guard_check(PG_FUNCTION_ARGS);
static void lob_object_access_hook(ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg);
static bool set_lobject_funcs(void);
static const char *lookup_lobject_log_name(Oid funcid);
static Oid get_sentinel_log_owner(void);

static bool ddl_guard_enabled = false;
static bool ddl_guard_ddl_sentinel = false;
static bool ddl_guard_lo_sentinel = false;
static object_access_hook_type next_object_access_hook = NULL;
static bool lobject_funcs_initialized = false;
static bool lobject_funcs_ready = false;

typedef struct LobjectFuncEntry
{
	Oid			oid;
	char	   *log_name;
} LobjectFuncEntry;

/* large_object_func_names contains all the SQL lobject functions */
static const char *large_object_func_names[] = {
	"lo_create",
	"lo_creat",
	"lo_truncate",
	"lo_truncate64",
	"lo_unlink",
	"lowrite",
	"lo_from_bytea",
	"lo_import",
	"lo_import_with_oid",
};
static const int large_object_func_names_count = sizeof(large_object_func_names) / sizeof(large_object_func_names[0]);
static LobjectFuncEntry *lobject_funcs = NULL;
static int	lobject_funcs_count = 0;
static int	lobject_funcs_cap = 0;

PG_FUNCTION_INFO_V1(ddl_guard_check);

static void
add_lobject_func(Oid funcid, const char *log_name)
{
	int			i;
	MemoryContext	oldcxt;

	for (i = 0; i < lobject_funcs_count; i++)
	{
		if (lobject_funcs[i].oid == funcid)
		{
			if (log_name != NULL)
				pfree((void *) log_name);
			return;
		}
	}

	oldcxt = MemoryContextSwitchTo(TopMemoryContext);
	if (lobject_funcs_cap == 0)
	{
		lobject_funcs_cap = 16;
		lobject_funcs = (LobjectFuncEntry *) palloc0(lobject_funcs_cap * sizeof(LobjectFuncEntry));
	}
	else if (lobject_funcs_count == lobject_funcs_cap)
	{
		lobject_funcs_cap *= 2;
		lobject_funcs = (LobjectFuncEntry *) repalloc(lobject_funcs, lobject_funcs_cap * sizeof(LobjectFuncEntry));
	}

	lobject_funcs[lobject_funcs_count].oid = funcid;
	lobject_funcs[lobject_funcs_count].log_name = (log_name != NULL) ? pstrdup(log_name) : NULL;
	lobject_funcs_count++;
	MemoryContextSwitchTo(oldcxt);

	if (log_name != NULL)
		pfree((void *) log_name);
}

static bool
set_lobject_funcs(void)
{
	int			i;
	Oid			nspoid;

	nspoid = get_namespace_oid("pg_catalog", true);
	if (!OidIsValid(nspoid))
		return false;

	for (i = 0; i < large_object_func_names_count; i++)
	{
		CatCList   *catlist;
		int			j;

		catlist = SearchSysCacheList1(PROCNAMEARGSNSP,
									  CStringGetDatum(large_object_func_names[i]));
		for (j = 0; j < catlist->n_members; j++)
		{
			HeapTuple	tuple;
			Form_pg_proc procform;
			Datum		prosrc_datum;
			bool		isnull;
			char	   *log_name;
			Oid			funcid;

			tuple = &catlist->members[j]->tuple;
			procform = (Form_pg_proc) GETSTRUCT(tuple);

			if (procform->pronamespace != nspoid)
				continue;
			if (procform->prolang != INTERNALlanguageId && procform->prolang != ClanguageId)
				continue;

			prosrc_datum = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_prosrc, &isnull);
			if (isnull)
				continue;

			log_name = TextDatumGetCString(prosrc_datum);
			funcid = procform->oid;
			add_lobject_func(funcid, log_name);
		}
		ReleaseSysCacheList(catlist);
	}

	return lobject_funcs_count > 0;
}

static const char *
lookup_lobject_log_name(Oid funcid)
{
	int			i;

	for (i = 0; i < lobject_funcs_count; i++)
	{
		if (lobject_funcs[i].oid == funcid)
			return lobject_funcs[i].log_name;
	}

	return NULL;
}

static Oid
get_sentinel_log_owner(void)
{
	HeapTuple	tuple;
	Datum		relowner_datum;
	bool		isnull;
	Oid			nspoid;
	Oid			relid;
	Oid			relowner;

	nspoid = get_namespace_oid("ddl_guard", true);
	if (!OidIsValid(nspoid))
		return InvalidOid;

	relid = get_relname_relid("sentinel_log", nspoid);
	if (!OidIsValid(relid))
		return InvalidOid;

	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		return InvalidOid;

	relowner_datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relowner, &isnull);
	if (isnull)
	{
		ReleaseSysCache(tuple);
		return InvalidOid;
	}

	relowner = DatumGetObjectId(relowner_datum);
	ReleaseSysCache(tuple);

	return relowner;
}

void
log_sentinel_event(const char *volatile operation)
{
	int			ret;
	Oid			argtypes[1];
	Datum		values[1];
	char		nulls[1];
	Oid			log_owner;
	Oid			saved_userid;
	int			saved_sec_context;
	int			new_sec_context;
	volatile bool	connected = false;

	log_owner = get_sentinel_log_owner();
	if (!OidIsValid(log_owner))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("ddl_guard.sentinel_log not found")));

	GetUserIdAndSecContext(&saved_userid, &saved_sec_context);
	new_sec_context = saved_sec_context | SECURITY_LOCAL_USERID_CHANGE;
	SetUserIdAndSecContext(log_owner, new_sec_context);

	PG_TRY();
	{
		const char *op = (const char *) operation;

		if (op == NULL || op[0] == '\0')
			op = "unknown";

		if (SPI_connect() != SPI_OK_CONNECT)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("could not connect to SPI")));
		connected = true;

		argtypes[0] = TEXTOID;
		values[0] = CStringGetTextDatum(op);
		nulls[0] = ' ';

		ret = SPI_execute_with_args(
			"INSERT INTO ddl_guard.sentinel_log (operation) VALUES ($1)",
			1, argtypes, values, nulls, false, 0);
		if (ret != SPI_OK_INSERT)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("could not insert ddl_guard sentinel log entry")));

		ret = SPI_finish();
		if (ret != SPI_OK_FINISH)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("could not close SPI connection")));
		connected = false;
	}
	PG_CATCH();
	{
		if (connected)
			(void) SPI_finish();
		SetUserIdAndSecContext(saved_userid, saved_sec_context);
		PG_RE_THROW();
	}
	PG_END_TRY();

	SetUserIdAndSecContext(saved_userid, saved_sec_context);
}

static void
lob_object_access_hook(ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg)
{
	const char *log_name;

	if (ddl_guard_enabled && ddl_guard_lo_sentinel)
	{
		if (!lobject_funcs_initialized && IsTransactionState())
		{
			lobject_funcs_ready = set_lobject_funcs();
			if (lobject_funcs_ready)
				lobject_funcs_initialized = true;
		}
		if (!lobject_funcs_ready)
			goto done;

		switch (access)
		{
			case OAT_FUNCTION_EXECUTE:
				/* log large object function calls in sentinel mode */
				if (classId != ProcedureRelationId)
					break;

				log_name = lookup_lobject_log_name(objectId);
				if (log_name != NULL)
				{
					log_sentinel_event(log_name);
					ereport(WARNING, errmsg("lo_guard: lobject \"%s\" function call, sentinel entry written", log_name));
				}
				break;
			default:
				break;
		}
	}
done:
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

	if (ddl_guard_enabled)
	{
		if (ddl_guard_ddl_sentinel)
		{
			EventTriggerData *trigdata = (EventTriggerData *) fcinfo->context;

			log_sentinel_event(GetCommandTagName(trigdata->tag));
			ereport(WARNING, (errmsg("ddl_guard: ddl detected, sentinel entry written")));
			PG_RETURN_VOID();
		}
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("Non-superusers are not allowed to execute DDL statements"),
					 errhint("ddl_guard.enabled is set.")));
	}

	PG_RETURN_VOID();
}

void
_PG_init(void)
{
	DefineCustomBoolVariable("ddl_guard.enabled", "Enable or disable DDL Guard",
							 NULL, &ddl_guard_enabled, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	DefineCustomBoolVariable("ddl_guard.ddl_sentinel", "Write sentinel entry for DDL statements",
							 NULL, &ddl_guard_ddl_sentinel, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	DefineCustomBoolVariable("ddl_guard.lo_sentinel", "Write sentinel entry for pg_largeobject modifications",
							 NULL, &ddl_guard_lo_sentinel, false, PGC_SUSET, 0, NULL,
							 NULL, NULL);

	next_object_access_hook = object_access_hook;
	object_access_hook = lob_object_access_hook;

	EmitWarningsOnPlaceholders("ddl_guard");
}

void
_PG_fini(void)
{
	int			i;

	if (lobject_funcs != NULL)
	{
		for (i = 0; i < lobject_funcs_count; i++)
		{
			if (lobject_funcs[i].log_name != NULL)
				pfree(lobject_funcs[i].log_name);
		}
		pfree(lobject_funcs);
		lobject_funcs = NULL;
		lobject_funcs_count = 0;
		lobject_funcs_cap = 0;
		lobject_funcs_initialized = false;
		lobject_funcs_ready = false;
	}

	object_access_hook = next_object_access_hook;
}
