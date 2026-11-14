#include "tracker.h"

// load Data

int loadData(WorkoutDB *db, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        if (errno == ENOENT)
        {
            puts("No data file; current records unchanged.");
            return 1;
        }
        perror("Open data");
        return 0;
    }
    /* Check incoming rows separately. The current rows remain usable. */
    WorkoutDB checked = {0};
    int ok = 1, row = 0;
    while (ok)
    {
        char line[256];
        size_t length = 0;
        int ch;
        /* Read one bounded text line; reject NUL and oversized lines. */
        while ((ch = fgetc(fp)) != '\n' && ch != EOF)
        {
            if (length >= sizeof line - 1 || ch == '\0')
                ok = 0;
            else
                line[length++] = (char)ch;
        }
        if (ch == EOF && length == 0)
            break;
        ++row;
        if (!ok)
            break;
        if (length > 0 && line[length - 1] == '\r')
            --length;
        line[length] = '\0';
        /* Read numbers as bounded TEXT first, never unchecked %d. */
        char number[5][11];
        WorkoutEntry e = {0};
        int used = -1, fields, howMany;
        if (row == 1)
        {
            fields = sscanf(line, "HT1|%10[0-9]|%10[0-9]%n",
                            number[0], number[1], &used);
            howMany = 2;
            if (fields != 2)
                ok = 0;
        }
        else
        {
            fields = sscanf(line,
                            "%10[0-9]|%10[0-9]|%10[0-9-]|%10[0-9]|"
                            "%10[0-9]|%10[0-9]|%47[ -~]%n",
                            number[0], number[1], e.date, number[2],
                            number[3], number[4], e.exercise, &used);
            howMany = 5;
            if (fields != 7)
                ok = 0;
        }
        if (!ok || used != (int)length)
        {
            ok = 0;
            break;
        }
        int value[5];
        for (int i = 0; i < howMany; ++i)
        {
            char *end;
            errno = 0;
            long n = strtol(number[i], &end, 10);
            if (*end != '\0' || errno == ERANGE || n > INT_MAX)
            {
                ok = 0;
                break;
            }
            value[i] = (int)n;
        }
        if (!ok)
            break;
        if (row == 1)
        {
            checked.nextId = value[0];
            checked.nextSession = value[1];
            if (value[0] < 1 || value[1] < 1)
                ok = 0;
        }
        else
        {
            e.id = value[0];
            e.sessionId = value[1];
            e.setNumber = value[2];
            e.reps = value[3];
            e.weight100 = value[4];
            if (!validEntry(&checked, &e, 0) ||
                e.id >= checked.nextId || e.sessionId >= checked.nextSession)
            {
                ok = 0;
                break;
            }
            if (!reserveEntries(&checked, checked.count + 1))
            {
                ok = 0;
                break;
            }
            checked.data[checked.count] = e;
            ++checked.count;
        }
    }
    if (row == 0 || ferror(fp))
        ok = 0;
    if (fclose(fp) != 0)
        ok = 0;
    /* Make room before replacing any live rows. Failure keeps them all. */
    if (ok && !reserveEntries(db, checked.count))
        ok = 0;
    if (ok)
    {
        for (size_t i = 0; i < checked.count; ++i)
            db->data[i] = checked.data[i];
        db->count = checked.count;
        db->nextId = checked.nextId;
        db->nextSession = checked.nextSession;
        printf("Loaded %zu set(s). PRs are derived when requested.\n", db->count);
    }
    else
    {
        printf("Load rejected near line %d; old records retained.\n", row);
    }
    free(checked.data); /* Only the temporary checking array is freed. */
    return ok;
}

// save data
int saveData(const WorkoutDB *db, const char *path)
{
    // creates 2 file paths for workouts.txt
    char temp[256], backup[256]; // using snprintf so we can specify the limit for buffer overflow
    if (snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp ||
        snprintf(backup, sizeof backup, "%s.bak", path) >= (int)sizeof backup) // this condition checks for truncation errors and buffer overflow
        return 0;
    FILE *fp = fopen(temp, "w");
    if (!fp)
    {
        perror("Open temporary save");
        return 0;
    }

    // ok will store 1 or 0 based on condition
    int ok = fprintf(fp, "HT1|%d|%d\n", db->nextId, db->nextSession) >= 0; // this printf will define how the data will be stored in the file (HT1 acts like a label for the value inside)

    for (size_t i = 0; i < db->count && ok; ++i) // loops over database entries
    {
        const WorkoutEntry *e = &db->data[i];
        ok = fprintf(fp, "%d|%d|%s|%d|%d|%d|%s\n",
                     e->id, e->sessionId, e->date, e->setNumber,
                     e->reps, e->weight100, e->exercise) >= 0; // this printf writes a single complete workout entry
    }

    // closing can fail too; check it before replacing anything
    if (fclose(fp) != 0) // if the file writing was succesful , close the file
        ok = 0;

    if (!ok)
    {
        perror("Write or close save");
        if (remove(temp) != 0)
            perror("Remove incomplete temporary file");
        return 0;
    }

    // clears out the old backup, moves the current save into the backup spot
    if (remove(backup) != 0 && errno != ENOENT)
    {
        perror("Remove old backup");
        return 0;
    }

    int hadOld = 0;

    if (rename(path, backup) == 0)
        hadOld = 1;
    else if (errno != ENOENT)
    {
        perror("Create backup");
        return 0;
    }

    // restore the backup on failure.
    if (rename(temp, path) != 0)
    {
        perror("Publish save");
        if (hadOld && rename(backup, path) != 0)
            perror("Restore failed: recover the .bak file manually");
        return 0;
    }

    puts("Saved successfully.");
    return 1;
}

/*
saveData writes the workout database to a temporary file,
then replaces the current save file while keeping the previous save as a backup.
It reports success or failure and tries to restore the backup if replacing the file fails.
*/