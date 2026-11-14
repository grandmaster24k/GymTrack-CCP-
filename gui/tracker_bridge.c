/*
 * GUI adapter for the supplied C11 GymTrack.
 * Original files are included unchanged. Only terminal input/output is
 * substituted; parsing, validation, mutation, planning, PRs and HT1 storage
 * execute the original implementation. Do NOT compile the embedded .c files
 * separately when linking this translation unit.
 */
#include "tracker_bridge.h"
#include <stdarg.h>

static const char *input_cursor = "";
static char *output_text;
static size_t output_length, output_capacity;
static int exhausted, capture_failed;

static void reset_io(const char *input)
{
    input_cursor = input ? input : "";
    output_length = 0;
    if (output_text) output_text[0] = '\0';
    exhausted = capture_failed = 0;
}

static int append_output(const char *text, size_t length)
{
    if (length > (size_t)-1 - output_length - 1) {
        capture_failed = 1;
        return -1;
    }
    size_t needed = output_length + length + 1;
    if (needed > output_capacity) {
        size_t capacity = output_capacity ? output_capacity : 2048;
        while (capacity < needed) {
            if (capacity > (size_t)-1 / 2) { capacity = needed; break; }
            capacity *= 2;
        }
        char *fresh = (char *)realloc(output_text, capacity);
        if (!fresh) { capture_failed = 1; return -1; }
        output_text = fresh;
        output_capacity = capacity;
    }
    memcpy(output_text + output_length, text, length);
    output_length += length;
    output_text[output_length] = '\0';
    return 0;
}

static int capture_printf(const char *format, ...)
{
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) { va_end(args); capture_failed = 1; return -1; }
    char local[1024];
    char *buffer = (size_t)length < sizeof local ? local : (char *)malloc((size_t)length + 1);
    if (!buffer) { va_end(args); capture_failed = 1; return -1; }
    vsnprintf(buffer, (size_t)length + 1, format, args);
    va_end(args);
    int ok = append_output(buffer, (size_t)length);
    if (buffer != local) free(buffer);
    return ok < 0 ? -1 : length;
}

static int capture_puts(const char *text)
{
    if (append_output(text, strlen(text)) < 0) return EOF;
    return append_output("\n", 1) < 0 ? EOF : 1;
}

static void capture_perror(const char *text)
{
    int saved_errno = errno;
    capture_printf("%s: %s\n", text, strerror(saved_errno));
    errno = saved_errno;
}

static char *script_fgets(char *text, int size, FILE *stream)
{
    if (stream != stdin) return fgets(text, size, stream);
    if (size <= 1 || !*input_cursor) { exhausted = 1; return NULL; }
    int used = 0;
    while (used < size - 1 && *input_cursor) {
        char ch = *input_cursor++;
        text[used++] = ch;
        if (ch == '\n') break;
    }
    text[used] = '\0';
    return text;
}

static int script_getchar(void)
{
    if (!*input_cursor) return EOF;
    return (unsigned char)*input_cursor++;
}

/* Input prompts are already represented by labels in the GUI. */
static int script_fputs(const char *text, FILE *stream)
{
    return stream == stdout ? 0 : fputs(text, stream);
}
static int script_fflush(FILE *stream)
{
    return stream == stdout ? 0 : fflush(stream);
}

#define printf capture_printf
#define puts capture_puts
#define perror capture_perror
#define fgets script_fgets
#define getchar script_getchar
#define fputs script_fputs
#define fflush script_fflush
#include "../original/input.c"
#include "../original/workout.c"
#include "../original/features.c"
#include "../original/storage.c"
#undef printf
#undef puts
#undef perror
#undef fgets
#undef getchar
#undef fputs
#undef fflush

const char *ht_output(void)
{
    if (capture_failed) return "Not enough memory to display the complete result. Check the records before retrying.";
    return output_text ? output_text : "";
}

int ht_input_ok(void) { return !exhausted && !capture_failed; }

void ht_release(void)
{
    free(output_text);
    output_text = NULL;
    output_capacity = output_length = 0;
    input_cursor = "";
}

static int fail(const char *message)
{
    reset_io("");
    capture_puts(message);
    return 0;
}

/* Remove console selection chatter but keep the original result text. */
static void keep_from(const char *marker)
{
    if (!output_text) return;
    char *start = strstr(output_text, marker);
    if (start) {
        size_t length = strlen(start);
        memmove(output_text, start, length + 1);
        output_length = length;
    }
}

int ht_run(HTOperation operation, WorkoutDB *db,
           const char *answers, const char *argument)
{
    reset_io(answers);
    const char *arg = argument ? argument : "";
    size_t old_count = db ? db->count : 0;
    int ok = 0;
    if (!db && operation != HT_PLAN && operation != HT_CALORIES)
        return fail("No workout database is available.");
    switch (operation) {
    case HT_ADD:
        addSets(db);
        ok = db->count > old_count && !strstr(ht_output(), "Database is full");
        break;
    case HT_EDIT:
        changeWorkout(db, 0);
        ok = strstr(ht_output(), "Updated.") != NULL;
        break;
    case HT_DELETE:
        changeWorkout(db, 1);
        ok = db->count < old_count;
        break;
    case HT_PLAN:
        generatePlan();
        ok = strstr(ht_output(), "Workout plan:") != NULL;
        if (ok) keep_from("Workout plan:");
        break;
    case HT_CALORIES:
        calculateCalories();
        ok = strstr(ht_output(), "Estimated daily calories:") != NULL;
        if (ok) keep_from("--- ");
        else keep_from("The calculated calorie target is invalid.");
        break;
    case HT_PR: showPR(db, arg); ok = 1; break;
    case HT_HISTORY: showWorkouts(db, arg, 0); ok = 1; break;
    case HT_SAVE: ok = saveData(db, arg); break;
    case HT_LOAD: ok = loadData(db, arg); break;
    default: return fail("Unknown operation.");
    }
    return ok && ht_input_ok();
}

int ht_parse_int(const char *text, int low, int high, int *result)
{
    if (!text || !result || strlen(text) > 120 || strpbrk(text, "\r\n"))
        return fail("Enter one whole number.");
    reset_io(text);
    int ok = readInt("", low, high, result);
    input_cursor = "";
    return ok;
}

int ht_parse_double(const char *text, double low, double high, double *result)
{
    if (!text || !result || strlen(text) > 120 || strpbrk(text, "\r\n"))
        return fail("Enter one finite number.");
    reset_io(text);
    int ok = readDouble("", low, high, result);
    input_cursor = "";
    return ok;
}

static int valid_name(const char *name)
{
    if (!name || strlen(name) >= NAME_LEN) return 0;
    WorkoutDB empty = {0};
    WorkoutEntry probe = {0};
    probe.id = probe.sessionId = probe.setNumber = probe.reps = 1;
    strcpy(probe.date, "2000-01-01");
    strcpy(probe.exercise, name);
    return validEntry(&empty, &probe, 0);
}

int ht_add(WorkoutDB *db, const char *date, const char *exercise,
           int count, const double *weights, const int *reps)
{
    if (!db || !date || !ValidDate(date)) return fail("Use a valid date from 1900 to 2099 in YYYY-MM-DD format.");
    if (!valid_name(exercise)) return fail("Use 1-47 printable characters, including a letter, with no | character.");
    if (count < 1 || count > 100 || !weights || !reps) return fail("Choose 1 to 100 sets.");
    for (int i = 0; i < count; ++i) {
        if (!isfinite(weights[i]) || weights[i] < 0 || weights[i] > 1000 || reps[i] < 1 || reps[i] > 1000)
            return fail("Every set needs a finite weight from 0 to 1000 kg and whole-number reps from 1 to 1000.");
    }
    char answers[8192];
    int used = snprintf(answers, sizeof answers, "%s\n%s\n%d\n", date, exercise, count);
    for (int i = 0; i < count; ++i)
        used += snprintf(answers + used, sizeof answers - (size_t)used, "%.17g\n%d\n", weights[i], reps[i]);
    size_t before = db->count;
    int ok = ht_run(HT_ADD, db, answers, "");
    input_cursor = "";
    return ok && db->count - before == (size_t)count;
}

int ht_edit(WorkoutDB *db, int id, const char *exercise, double kg, int reps)
{
    if (!db || id < 1 || id >= INT_MAX) return fail("Choose a valid record ID.");
    if (!valid_name(exercise)) return fail("Use 1-47 printable characters, including a letter, with no | character.");
    if (!isfinite(kg) || kg < 0 || kg > 1000 || reps < 1 || reps > 1000)
        return fail("Use a finite weight from 0 to 1000 kg and whole-number reps from 1 to 1000.");
    char answers[256];
    snprintf(answers, sizeof answers, "%d\n%s\n%.17g\n%d\n", id, exercise, kg, reps);
    int ok = ht_run(HT_EDIT, db, answers, "");
    input_cursor = "";
    return ok;
}

int ht_delete(WorkoutDB *db, int id)
{
    if (!db || id < 1 || id >= INT_MAX) return fail("Choose a valid record ID.");
    char answers[32];
    snprintf(answers, sizeof answers, "%d\n1\n", id);
    int ok = ht_run(HT_DELETE, db, answers, "");
    input_cursor = "";
    return ok;
}

int ht_calories(int unit, double current, double goal, double maintenance, double months)
{
    if (unit != 1 && unit != 2) return fail("Choose kilograms or pounds.");
    double factor = unit == 2 ? KG_TO_LB : 1;
    if (!isfinite(current) || current < 10 * factor || current > 300 * factor ||
        !isfinite(goal) || goal < 10 * factor || goal > 400 * factor ||
        !isfinite(maintenance) || maintenance < 1 || maintenance > 10000 ||
        !isfinite(months) || months < 0.01 || months > 120)
        return fail("Check weights, maintenance calories (1-10000), and duration (0.01-120 months). Current weight must be 10-300 kg; goal weight 10-400 kg (or pounds equivalent).");
    char answers[256];
    snprintf(answers, sizeof answers, "%d\n%.17g\n%.17g\n%.17g\n%.17g\n", unit, current, goal, maintenance, months);
    int ok = ht_run(HT_CALORIES, NULL, answers, "");
    input_cursor = "";
    return ok;
}

int ht_plan(int count, const int *groups, const int options[4][3])
{
    if (count < 1 || count > 4 || !groups || !options) return fail("Choose one to four different muscle groups.");
    for (int i = 0; i < count; ++i) {
        if (groups[i] < 1 || groups[i] > 7) return fail("Choose a valid muscle group.");
        for (int j = 0; j < i; ++j)
            if (groups[i] == groups[j]) return fail("Choose each muscle group only once.");
        for (int j = 0; j < 3; ++j)
            if (options[i][j] < 1 || options[i][j] > 2) return fail("Choose an exercise option for every movement.");
    }
    char answers[256];
    int used = snprintf(answers, sizeof answers, "%d\n", count);
    for (int i = 0; i < count; ++i)
        used += snprintf(answers + used, sizeof answers - (size_t)used, "%d\n", groups[i]);
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < 3; ++j)
            used += snprintf(answers + used, sizeof answers - (size_t)used, "%d\n", options[i][j]);
    int ok = ht_run(HT_PLAN, NULL, answers, "");
    input_cursor = "";
    return ok;
}
