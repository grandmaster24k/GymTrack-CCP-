#ifndef TRACKER_BRIDGE_H
#define TRACKER_BRIDGE_H

#include "../original/tracker.h"

/* The bridge is single-threaded, like the original console program. Output
   belongs to the bridge and remains valid until the next bridge operation. */
typedef enum {
    HT_ADD, HT_EDIT, HT_DELETE, HT_PLAN, HT_CALORIES,
    HT_PR, HT_HISTORY, HT_SAVE, HT_LOAD
} HTOperation;

/* answers: newline-separated answers, in the original console prompt order.
   argument: exact exercise name for PR/history, file path for save/load.
   NULL answers/argument mean an empty string. Form fields should first be
   checked with ht_parse_int/ht_parse_double and the original validEntry. */
int ht_run(HTOperation operation, WorkoutDB *db,
           const char *answers, const char *argument);
const char *ht_output(void);
int ht_input_ok(void);
int ht_parse_int(const char *text, int low, int high, int *result);
int ht_parse_double(const char *text, double low, double high, double *result);
void ht_release(void);

/* Safe form adapters. These validate the entire form before invoking the
   original functions, so a mistyped field cannot become the next answer.
   Returns 1 on full success, 0 on failure. Original addSets still retains any
   successfully added sets if a later memory allocation fails. */
int ht_add(WorkoutDB *db, const char *date, const char *exercise,
           int count, const double *weights, const int *reps);
int ht_edit(WorkoutDB *db, int id, const char *exercise, double kg, int reps);
int ht_delete(WorkoutDB *db, int id);
int ht_calories(int unit, double current, double goal,
                double maintenance, double months);
int ht_plan(int count, const int *groups, const int options[4][3]);

#endif
