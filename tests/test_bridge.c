#include "../gui/tracker_bridge.h"
#include <assert.h>

static int checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "FAIL at line %d: %s\nBridge output: %s\n", __LINE__, #condition, ht_output()); \
    exit(EXIT_FAILURE); } } while (0)

static WorkoutDB fresh(void)
{
    WorkoutDB db = {0};
    db.nextId = db.nextSession = 1;
    CHECK(reserveEntries(&db, 8));
    return db;
}

static int same_entry(const WorkoutEntry *a, const WorkoutEntry *b)
{
    return a->id == b->id && a->sessionId == b->sessionId &&
        a->setNumber == b->setNumber && a->weight100 == b->weight100 &&
        a->reps == b->reps && !strcmp(a->date, b->date) && !strcmp(a->exercise, b->exercise);
}

static void write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    CHECK(fp != NULL);
    CHECK(fputs(text, fp) >= 0);
    CHECK(fclose(fp) == 0);
}

static void validation_tests(void)
{
    int i = 0;
    double d = 0;
    CHECK(ht_parse_int(" 12 ", 1, 1000, &i) && i == 12);
    CHECK(!ht_parse_int("1.5", 1, 1000, &i));
    CHECK(!ht_parse_int("12oops", 1, 1000, &i));
    CHECK(!ht_parse_int("999999999999999999999", 1, 1000, &i));
    CHECK(!ht_parse_int("", 1, 1000, &i));
    CHECK(!ht_parse_int("0", 1, 1000, &i));
    CHECK(!ht_parse_int("1001", 1, 1000, &i));
    CHECK(!ht_parse_int("2\n3", 1, 1000, &i));
    CHECK(ht_parse_double(" 42.125 ", 0, 1000, &d) && d == 42.125);
    CHECK(!ht_parse_double("nan", 0, 1000, &d));
    CHECK(!ht_parse_double("inf", 0, 1000, &d));
    CHECK(!ht_parse_double("1e999", 0, 1000, &d));
    CHECK(!ht_parse_double("12kg", 0, 1000, &d));
    CHECK(!ht_parse_double("-0.1", 0, 1000, &d));
    CHECK(ValidDate("2000-02-29"));
    CHECK(!ValidDate("1900-02-29"));
    CHECK(ValidDate("2024-02-29"));
    CHECK(!ValidDate("2026-02-29"));
    CHECK(!ValidDate("2026-02-30"));
    CHECK(!ValidDate("2026-04-31"));
    CHECK(!ValidDate("1899-12-31"));
    CHECK(ValidDate("1900-01-01"));
    CHECK(ValidDate("2099-12-31"));
    CHECK(!ValidDate("2024-02-29suffix"));
    CHECK(!ValidDate("2100-01-01"));
    CHECK(!ValidDate("2026-1-01"));
    CHECK(!ValidDate("2026-13-01"));
    puts("PASS original numeric and date validation");
}

static void feature_tests(WorkoutDB *db)
{
    CHECK(ht_run(HT_PR, db, "", "Bench press"));
    CHECK(strstr(ht_output(), "highest recorded weight 42.13 kg") != NULL);
    CHECK(strstr(ht_output(), "40.00 kg / 88.18 lb: 15 reps") != NULL);
    CHECK(ht_run(HT_PR, db, "", "bench press"));
    CHECK(strstr(ht_output(), "No matching exercise records") != NULL);
    CHECK(ht_run(HT_HISTORY, db, "", "Bench press"));
    CHECK(strstr(ht_output(), "3 set(s) shown") != NULL);
    CHECK(strstr(ht_output(), "Squat") == NULL);

    CHECK(ht_calories(1, 70, 70, 2500, 3));
    CHECK(strstr(ht_output(), "MAINTENANCE PLAN") != NULL);
    CHECK(strstr(ht_output(), "2500 kcal") != NULL);
    CHECK(ht_calories(1, 70, 80, 2500, 3));
    CHECK(strstr(ht_output(), "BULKING PLAN") != NULL);
    char expected[64];
    snprintf(expected, sizeof expected, "%.0f kcal", 2500.0 + 10.0 * 7700.0 / (3.0 * 30.44));
    CHECK(strstr(ht_output(), expected) != NULL);
    CHECK(ht_calories(2, 170, 160, 2500, 3));
    CHECK(strstr(ht_output(), "CUTTING PLAN") != NULL);
    snprintf(expected, sizeof expected, "%.0f kcal", 2500.0 - 10.0 * 3500.0 / (3.0 * 30.44));
    CHECK(strstr(ht_output(), expected) != NULL);
    CHECK(!ht_calories(1, 100, 10, 1, 0.01));
    CHECK(strstr(ht_output(), "calorie target is invalid") != NULL);
    CHECK(!ht_calories(1, NAN, 70, 2500, 3));
    CHECK(!ht_calories(2, 10, 70, 2500, 3));

    int groups[4] = {1, 2, 6, 7};
    const int options[4][3] = {{1,2,1},{2,1,2},{1,2,1},{2,1,2}};
    CHECK(ht_plan(1, groups, options));
    CHECK(strstr(ht_output(), "Dumbbell flat press") != NULL);
    CHECK(strstr(ht_output(), "Incline barbell press") != NULL);
    CHECK(strstr(ht_output(), "   6  8-12 or to failure   180") != NULL);
    CHECK(ht_plan(2, groups, options));
    CHECK(strstr(ht_output(), "Pull Ups") != NULL);
    CHECK(strstr(ht_output(), "   3  8-12 or to failure   180") != NULL);
    CHECK(ht_plan(4, groups, options));
    CHECK(strstr(ht_output(), "Hamstring Curl") != NULL);
    CHECK(strstr(ht_output(), "Plank") != NULL);
    CHECK(strstr(ht_output(), "   2  8-12 or to failure   180") != NULL);
    groups[1] = groups[0];
    CHECK(!ht_plan(2, groups, options));
    CHECK(!ht_plan(0, groups, options));
    puts("PASS exact-name history, PRs, both calorie units, and workout plans");
}

static void capacity_tests(void)
{
    WorkoutDB db = fresh();
    CHECK(reserveEntries(&db, MAX_RECORDS));
    for (int i = 0; i < MAX_RECORDS - 1; ++i) {
        WorkoutEntry e = {0};
        e.id = i + 1;
        e.sessionId = 1;
        e.setNumber = i + 1;
        e.reps = 1;
        strcpy(e.date, "2026-01-01");
        strcpy(e.exercise, "Capacity test");
        db.data[i] = e;
    }
    db.count = MAX_RECORDS - 1;
    db.nextId = MAX_RECORDS;
    db.nextSession = 2;
    double weights[2] = {10, 20};
    int reps[2] = {10, 8};
    CHECK(!ht_add(&db, "2026-01-02", "Capacity test", 2, weights, reps));
    CHECK(db.count == MAX_RECORDS);
    CHECK(db.nextId == MAX_RECORDS + 1 && db.nextSession == 3);
    CHECK(db.data[MAX_RECORDS - 1].sessionId == 2);
    CHECK(db.data[MAX_RECORDS - 1].setNumber == 1);
    free(db.data);

    db = fresh();
    db.nextId = INT_MAX - 1;
    CHECK(!ht_add(&db, "2026-01-01", "ID test", 2, weights, reps));
    CHECK(db.count == 0 && db.nextId == INT_MAX - 1 && db.nextSession == 1);
    CHECK(ht_add(&db, "2026-01-01", "ID test", 1, weights, reps));
    CHECK(db.count == 1 && db.nextId == INT_MAX && db.nextSession == 2);
    CHECK(!ht_add(&db, "2026-01-01", "ID test", 1, weights, reps));
    db.nextId = 1;
    db.nextSession = INT_MAX;
    CHECK(!ht_add(&db, "2026-01-01", "ID test", 1, weights, reps));
    free(db.data);
    puts("PASS capacity partial-add semantics and exhausted IDs");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: test_bridge.exe <existing isolated test-output-directory>\n");
        return EXIT_FAILURE;
    }
    char path[240], bad_path[240], missing_path[240], backup[256];
    CHECK(snprintf(path, sizeof path, "%s/bridge_roundtrip.ht1", argv[1]) < (int)sizeof path);
    CHECK(snprintf(bad_path, sizeof bad_path, "%s/bridge_invalid.ht1", argv[1]) < (int)sizeof bad_path);
    CHECK(snprintf(missing_path, sizeof missing_path, "%s/does_not_exist.ht1", argv[1]) < (int)sizeof missing_path);
    validation_tests();
    WorkoutDB db = fresh();
    double bench[3] = {40, 42.126, 40};
    int bench_reps[3] = {12, 8, 15};
    CHECK(ht_add(&db, "2026-11-14", "Bench press", 3, bench, bench_reps));
    CHECK(db.count == 3 && db.nextId == 4 && db.nextSession == 2);
    for (int i = 0; i < 3; ++i) {
        CHECK(db.data[i].id == i + 1);
        CHECK(db.data[i].sessionId == 1);
        CHECK(db.data[i].setNumber == i + 1);
    }
    CHECK(db.data[1].weight100 == 4213);
    double squat[2] = {100, 110};
    int squat_reps[2] = {8, 5};
    CHECK(ht_add(&db, "2026-11-10", "Squat", 2, squat, squat_reps));
    CHECK(db.count == 5 && db.nextId == 6 && db.nextSession == 3);
    CHECK(db.data[3].sessionId == 2 && db.data[4].sessionId == 2);
    CHECK(!ht_add(&db, "2026-02-30", "Bench press", 3, bench, bench_reps));
    CHECK(!ht_add(&db, "2026-02-29", "Bench press", 3, bench, bench_reps));
    CHECK(db.count == 5 && db.nextId == 6 && db.nextSession == 3);
    CHECK(!ht_add(&db, "2026-11-14", "Bad|name", 3, bench, bench_reps));
    CHECK(!ht_add(&db, "2026-11-14", "123", 3, bench, bench_reps));
    char overlong[49];
    memset(overlong, 'A', 48); overlong[48] = '\0';
    CHECK(!ht_add(&db, "2026-11-14", overlong, 3, bench, bench_reps));
    CHECK(db.count == 5 && db.nextId == 6 && db.nextSession == 3);
    feature_tests(&db);
    sortEntries(&db, 1);
    CHECK(db.data[0].id == 4 && db.data[1].id == 5 && db.data[2].id == 1);
    sortEntries(&db, 0);
    CHECK(findById(db.data, 0, (int)db.count - 1, 4) == 3);
    CHECK(findById(db.data, 0, (int)db.count - 1, 99) == -1);
    WorkoutEntry before = db.data[1];
    CHECK(ht_edit(&db, 2, "Bench press", 50.5, 7));
    CHECK(db.data[1].weight100 == 5050 && db.data[1].reps == 7);
    CHECK(db.data[1].id == before.id && db.data[1].sessionId == before.sessionId);
    CHECK(db.data[1].setNumber == before.setNumber && !strcmp(db.data[1].date, before.date));
    CHECK(!ht_edit(&db, 2, "Bad|name", 50, 7));
    CHECK(db.data[1].weight100 == 5050);
    CHECK(!ht_edit(&db, 999, "Bench press", 50, 7));
    CHECK(ht_delete(&db, 1));
    CHECK(db.count == 4 && db.nextId == 6 && db.nextSession == 3);
    CHECK(!ht_delete(&db, 1));
    CHECK(ht_run(HT_PR, &db, "", "Bench press"));
    CHECK(strstr(ht_output(), "50.50 kg") != NULL);
    CHECK(ht_add(&db, "2026-11-14", "Deadlift", 1, squat, squat_reps));
    CHECK(db.data[4].id == 6 && db.data[4].sessionId == 3);
    CHECK(db.nextId == 7 && db.nextSession == 4);
    puts("PASS multi-set logging, rounding, ID/date sort, editing, and deletion");

    CHECK(ht_run(HT_SAVE, &db, "", path));
    CHECK(ht_run(HT_SAVE, &db, "", path));
    snprintf(backup, sizeof backup, "%s.bak", path);
    FILE *fp = fopen(backup, "r"); CHECK(fp != NULL); CHECK(fclose(fp) == 0);
    WorkoutDB loaded = fresh();
    CHECK(ht_run(HT_LOAD, &loaded, "", path));
    CHECK(loaded.count == db.count && loaded.nextId == db.nextId && loaded.nextSession == db.nextSession);
    for (size_t i = 0; i < db.count; ++i) CHECK(same_entry(&loaded.data[i], &db.data[i]));
    write_text(bad_path, "HT1|7|4\n2|1|2026-11-14|2|7|5050|Bench press\n2|1|2026-11-14|3|8|4000|Bench press\n");
    CHECK(!ht_run(HT_LOAD, &loaded, "", bad_path));
    CHECK(loaded.count == db.count && loaded.nextId == db.nextId && loaded.nextSession == db.nextSession);
    for (size_t i = 0; i < db.count; ++i) CHECK(same_entry(&loaded.data[i], &db.data[i]));
    write_text(bad_path, "HT1|7|4\n6|3|2026-11-14|1|8|10000|Deadlift|trailing\n");
    CHECK(!ht_run(HT_LOAD, &loaded, "", bad_path));
    write_text(bad_path, "");
    CHECK(!ht_run(HT_LOAD, &loaded, "", bad_path));
    CHECK(ht_run(HT_LOAD, &loaded, "", missing_path));
    CHECK(loaded.count == db.count);
    free(loaded.data);
    free(db.data);
    puts("PASS HT1 round trip, backup creation, malformed-load retention, and missing-file behavior");
    capacity_tests();
    ht_release();
    printf("All %d checks passed.\n", checks);
    return EXIT_SUCCESS;
}
