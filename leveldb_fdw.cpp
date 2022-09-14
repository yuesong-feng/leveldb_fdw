extern "C"
{
#include "postgres.h"
#include "fmgr.h"
#include "foreign/fdwapi.h"
#include "optimizer/pathnode.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/planmain.h"
#include "utils/rel.h"
#include "access/table.h"
#include "foreign/foreign.h"
#include "commands/defrem.h"
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "optimizer/appendinfo.h"
    PG_MODULE_MAGIC;
}
#include "leveldb/db.h"
#include "leveldb/iterator.h"

/*
 * FDW-specific information for ForeignScanState.fdw_state.
 */
typedef struct LevelDBFdwScanState
{
    leveldb::DB *db;
    leveldb::Iterator *it;
    bool finish;
} LevelDBFdwScanState;

typedef struct LevelDBFdwModifyState
{
    leveldb::DB *db;
} LevelDBFdwModifyState;

/*
 * SQL functions
 */
extern "C"
{
    PG_FUNCTION_INFO_V1(leveldb_fdw_handler);
    // PG_FUNCTION_INFO_V1(file_fdw_validator);
}

/*
 * FDW callback routines
 */
static void leveldbGetForeignRelSize(PlannerInfo *root,
                                     RelOptInfo *baserel,
                                     Oid foreigntableid);
static void leveldbGetForeignPaths(PlannerInfo *root,
                                   RelOptInfo *baserel,
                                   Oid foreigntableid);
static ForeignScan *leveldbGetForeignPlan(PlannerInfo *root,
                                          RelOptInfo *baserel,
                                          Oid foreigntableid,
                                          ForeignPath *best_path,
                                          List *tlist,
                                          List *scan_clauses,
                                          Plan *outer_plan);
static void leveldbBeginForeignScan(ForeignScanState *node,
                                    int eflags);
static TupleTableSlot *leveldbIterateForeignScan(ForeignScanState *node);
static void leveldbReScanForeignScan(ForeignScanState *node);
static void leveldbEndForeignScan(ForeignScanState *node);
static void leveldbBeginForeignInsert(ModifyTableState *mtstate,
                                      ResultRelInfo *rinfo);
static TupleTableSlot *leveldbExecForeignInsert(EState *estate,
                                                ResultRelInfo *rinfo,
                                                TupleTableSlot *slot,
                                                TupleTableSlot *planSlot);
static void leveldbEndForeignInsert(EState *estate,
                                    ResultRelInfo *rinfo);
static void
leveldbAddForeignUpdateTargets(PlannerInfo *root,
                               Index rtindex,
                               RangeTblEntry *target_rte,
                               Relation target_relation);
static List *
leveldbPlanForeignModify(PlannerInfo *root,
                         ModifyTable *plan,
                         Index resultRelation,
                         int subplan_index);

/*
 * Foreign-data wrapper handler function: return a struct with pointers
 * to my callback routines.
 */
extern "C"
{
    Datum leveldb_fdw_handler(PG_FUNCTION_ARGS)
    {
        FdwRoutine *fdwroutine = makeNode(FdwRoutine);

        /* Functions for scanning foreign tables */
        fdwroutine->GetForeignRelSize = leveldbGetForeignRelSize;
        fdwroutine->GetForeignPaths = leveldbGetForeignPaths;
        fdwroutine->GetForeignPlan = leveldbGetForeignPlan;
        fdwroutine->BeginForeignScan = leveldbBeginForeignScan;
        fdwroutine->IterateForeignScan = leveldbIterateForeignScan;
        fdwroutine->ReScanForeignScan = leveldbReScanForeignScan;
        fdwroutine->EndForeignScan = leveldbEndForeignScan;

        fdwroutine->AddForeignUpdateTargets = leveldbAddForeignUpdateTargets;
        fdwroutine->BeginForeignInsert = leveldbBeginForeignInsert;
        fdwroutine->ExecForeignInsert = leveldbExecForeignInsert;
        fdwroutine->EndForeignInsert = leveldbEndForeignInsert;
        PG_RETURN_POINTER(fdwroutine);
    }
}

/*
 * leveldbGetForeignRelSize
 *		Obtain relation size estimates for a foreign table
 */
static void leveldbGetForeignRelSize(PlannerInfo *root,
                                     RelOptInfo *baserel,
                                     Oid foreigntableid)
{
    baserel->rows = 100;
}

/*
 * postgresGetForeignPaths
 *		Create possible scan paths for a scan on the foreign table
 */
static void leveldbGetForeignPaths(PlannerInfo *root,
                                   RelOptInfo *baserel,
                                   Oid foreigntableid)
{
    add_path(baserel, (Path *)
                          create_foreignscan_path(root, baserel,
                                                  NULL, /* default pathtarget */
                                                  baserel->rows,
                                                  1,                 // start up cost
                                                  1 + baserel->rows, // total cost
                                                  NIL,               /* no pathkeys */
                                                  NULL,
                                                  NULL, /* no extra plan */
                                                  NIL));
}

/*
 * leveldbGetForeignPlan
 *		Create a ForeignScan plan node for scanning the foreign table
 */
static ForeignScan *leveldbGetForeignPlan(PlannerInfo *root,
                                          RelOptInfo *baserel,
                                          Oid foreigntableid,
                                          ForeignPath *best_path,
                                          List *tlist,
                                          List *scan_clauses,
                                          Plan *outer_plan)
{
    Index scan_relid = baserel->relid;

    scan_clauses = extract_actual_clauses(scan_clauses, false);
    return make_foreignscan(tlist,
                            scan_clauses,
                            scan_relid,
                            NIL, /* no expressions to evaluate */
                            NIL,
                            NIL, /* no custom tlist */
                            NIL, /* no remote quals */
                            outer_plan);
}

/*
 * leveldbBeginForeignScan
 *		Initiate access to the file by creating CopyState
 */
static void leveldbBeginForeignScan(ForeignScanState *node,
                                    int eflags)
{
    ForeignScan *plan = (ForeignScan *)node->ss.ps.plan;

    LevelDBFdwScanState *fsstate = (LevelDBFdwScanState *)palloc0(sizeof(LevelDBFdwScanState));

    fsstate->finish = false;

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/Users/yuesong/Desktop/postgresql-14.4/contrib/leveldb_fdw/testdb", &fsstate->db);
    assert(status.ok());

    fsstate->it = fsstate->db->NewIterator(leveldb::ReadOptions());
    fsstate->it->SeekToFirst();
    assert(fsstate->it->status().ok());

    node->fdw_state = (void *)fsstate;
}

static TupleTableSlot *leveldbIterateForeignScan(ForeignScanState *node)
{
    LevelDBFdwScanState *fsstate = (LevelDBFdwScanState *)node->fdw_state;
    TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
    ExecClearTuple(slot);

    if (!fsstate->finish)
    {
        leveldb::DB *db = fsstate->db;
        leveldb::Iterator *it = fsstate->it;

        std::string value;
        leveldb::Status status = db->Get(leveldb::ReadOptions(), it->key(), &value);
        assert(status.ok());

        it->Next();

        if (!it->Valid())
            fsstate->finish = true;

        slot->tts_isnull[0] = false;
        slot->tts_values[0] = Int32GetDatum(std::stoi(value));
        // slot->tts_values[0] = CStringGetDatum(value.c_str());
        ExecStoreVirtualTuple(slot);
    }

    return slot;
}

static void leveldbReScanForeignScan(ForeignScanState *node)
{
    LevelDBFdwScanState *fsstate = (LevelDBFdwScanState *)node->fdw_state;
    fsstate->finish = false;
    fsstate->it->SeekToFirst();
    assert(fsstate->it->status().ok());
}

static void leveldbEndForeignScan(ForeignScanState *node)
{
    LevelDBFdwScanState *fsstate = (LevelDBFdwScanState *)node->fdw_state;
    delete fsstate->it;
    delete fsstate->db;
}

static void
leveldbAddForeignUpdateTargets(PlannerInfo *root,
                               Index rtindex,
                               RangeTblEntry *target_rte,
                               Relation target_relation)
{
    Var *var;

    var = makeVar(rtindex,
                  SelfItemPointerAttributeNumber,
                  TIDOID,
                  -1,
                  InvalidOid,
                  0);
    add_row_identity_var(root, var, rtindex, "leveldb_kdy");
}

static List *
leveldbPlanForeignModify(PlannerInfo *root,
                         ModifyTable *plan,
                         Index resultRelation,
                         int subplan_index)
{
    CmdType operation = plan->operation;
    RangeTblEntry *rte = planner_rt_fetch(resultRelation, root);
    Relation rel;
    List *targetAttrs = NIL;
    List *array_elem_list = NIL;

    rel = table_open(rte->relid, NoLock);
    TupleDesc	tupdesc = RelationGetDescr(rel);


}

static void leveldbBeginForeignInsert(ModifyTableState *mtstate,
                                      ResultRelInfo *rinfo)
{
    // LevelDBFdwModifyState *fmstate = (LevelDBFdwModifyState *)palloc0(sizeof(LevelDBFdwModifyState));

    // leveldb::Options options;
    // options.create_if_missing = true;
    // leveldb::Status status = leveldb::DB::Open(options, "/Users/yuesong/Desktop/postgresql-14.4/contrib/leveldb_fdw/testdb", &fmstate->db);
    // assert(status.ok());

    // rinfo->ri_FdwState = (void *)fmstate;
}
static TupleTableSlot *leveldbExecForeignInsert(EState *estate,
                                                ResultRelInfo *rinfo,
                                                TupleTableSlot *slot,
                                                TupleTableSlot *planSlot)
{
    // LevelDBFdwModifyState *fmstate = (LevelDBFdwModifyState *)rinfo->ri_FdwState;
    // leveldb::DB *db = fmstate->db;
    // leveldb::Status status = db->Put(leveldb::WriteOptions(), "234", "234");
    // assert(status.ok());

    return NULL;
}
static void leveldbEndForeignInsert(EState *estate,
                                    ResultRelInfo *rinfo)
{
    // LevelDBFdwModifyState *fmstate = (LevelDBFdwModifyState *)rinfo->ri_FdwState;
    // delete fmstate->db;
}