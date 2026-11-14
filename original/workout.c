#include "tracker.h"

// find by id

int findById(const WorkoutEntry *a, int left, int right, int id)
{
    // uses binary search and recursion to search through workouts by Ids
    // must be sorted before searching
    if (left > right)
        return -1;
    int mid = left + (right - left) / 2;
    if (a[mid].id == id)
        return mid;
    if (id < a[mid].id)
        return findById(a, left, mid - 1, id);
    return findById(a, mid + 1, right, id);
}

// add sets

void addSets(WorkoutDB *db)
{
    WorkoutEntry e = {0};
    int sets;
    double kg;

    // Check if a Session ID is available
    if (db->nextSession >= INT_MAX)
    {
        puts("Session ID space full.");
        return;
    }

    // Check if at least one Record ID is available
    if (db->nextId >= INT_MAX)
    {
        puts("Record ID space full.");
        return;
    }

    // Assign the Session ID for this exercise
    e.sessionId = db->nextSession;

    // Keep asking until a valid date is entered
    do
    {
        if (!readLine("Date YYYY-MM-DD: ",
                      e.date, sizeof e.date))
            return;

        if (ValidDate(e.date))
            break;

        puts("Use a valid date between 1900-2099.");

    } while (1);

    // Placeholder values for validating exercise name
    e.id = db->nextId;
    e.setNumber = 1;
    e.reps = 1;

    // Keep asking until a valid exercise name is entered
    do
    {
        if (!readLine("Exercise: ",
                      e.exercise, sizeof e.exercise))
            return;

        if (validEntry(db, &e, 0))
            break;

        puts("Use a printable name with a letter and no | character.");

    } while (1);

    // Ask how many sets to record
    if (!readInt("How many sets (1-100)? ",
                 1, 100, &sets))
        return;

    // Check that enough Record IDs exist for all sets
    if (sets > INT_MAX - db->nextId)
    {
        puts("Not enough Record IDs for all sets.");
        return;
    }

    // Record each set
    for (int n = 1; n <= sets; ++n)
    {
        e.setNumber = n;

        printf("Set %d; weights are rounded to 0.01 kg.\n", n);

        if (!readDouble("Weight kg (0-1000): ",
                        0, 1000, &kg))
            return;

        if (!readInt("Repetitions (1-1000): ",
                     1, 1000, &e.reps))
            return;

        // Convert kilograms into hundredths of a kg
        e.weight100 = (int)(kg * 100.0 + 0.5);

        // Assign a unique Record ID to this set
        e.id = db->nextId;

        // Validate the complete workout record
        if (!validEntry(db, &e, 0))
        {
            puts("Invalid set.");
            return;
        }

        // Make sure the database has enough capacity
        if (!reserveEntries(db, db->count + 1))
        {
            puts("Database is full; this set was not added.");
            return;
        }

        // Copy the completed set into the database
        db->data[db->count] = e;

        // Update record count and next Record ID
        ++db->count;
        ++db->nextId;

        // Prepare next Session ID only after first set
        if (n == 1)
            ++db->nextSession;

        puts("Added set:");
        showWorkouts(db, "", e.id);
    }
}
/*
addSets records an exercise session by validating the date and exercise name,
then collecting weight and reps for each requested set.
Each set is stored as a separate entry with a unique record ID and a shared session ID; input,
validation, ID exhaustion, and storage errors are handled,
while previously saved sets remain if a later set fails.
*/

// reserve entries
int reserveEntries(WorkoutDB *db, size_t need) // for allocating memory dynamically to the workout database
{
    if (need <= db->capacity) // if the databse has enough space , return 1 for success
        return 1;
    if (need > MAX_RECORDS) // if the capacity exceeds the max # of records
        return 0;
    size_t cap = db->capacity; // To keep real capacity intact if allocation fails
    while (cap < need)
        cap += 8; // Adds slots by 8 always
    if (cap > MAX_RECORDS)
        cap = MAX_RECORDS;
    WorkoutEntry *p; // We haven't use *data so that Failure leaves the original array intact.
    if (db->data == NULL)
        p = malloc(cap * sizeof *p);
    else
        p = realloc(db->data, cap * sizeof *p);
    if (p == NULL)
        return 0;
    db->data = p;
    db->capacity = cap;
    return 1;
}

/*
makes sure the workout database’s array has room for a requested number of entries.
*/

// showWorkouts
void showWorkouts(const WorkoutDB *db,
                  const char *exercise, int recordId)
{
    int shown = 0; // Counts how many sets have been showed

    puts("   ID Sess Date          Set       kg      lb Reps");

    for (size_t i = 0; i < db->count; ++i)
    {
        const WorkoutEntry *e = &db->data[i];
        /*Pointing an existing record for better readability
        e.g usage of e->id instead of db->data[i].id*/

        if (exercise[0] != '\0' &&
            strcmp(e->exercise, exercise) != 0)
            continue;
        /*validate to show specific exercise
        and if exercise section is ""->show all exercise*/

        if (recordId != 0 && e->id != recordId)
            continue;
        /*validate to show specific exercise related to that
        record and if record section is 0->show all exercise*/

        double kg = e->weight100 / 100.0;
        double lb;

        convertWeight(kg, 1, &lb);

        printf("%5d %5d %s %4d %8.2f %8.2f %4d\n",
               e->id, e->sessionId, e->date, e->setNumber,
               kg, lb, e->reps);

        printf("      Exercise: %s\n", e->exercise);

        ++shown;
    }

    printf("%d set(s) shown. Group by session ID and exercise.\n",
           shown);
}


//Valid Entry

/*This function passes 0 or 1
0 if entry is not valid
1 if it passes all checks*/
int validEntry(const WorkoutDB *db, const WorkoutEntry *e,
               int ignoredId)
{
    // First check values that belong to this row alone.

    if (e->id < 1 || e->id >= INT_MAX)
        return 0; // The id must be a positive integer.
    if (e->sessionId < 1 || e->sessionId >= INT_MAX)
        return 0; // The session ID must be a positive integer.
    if (e->weight100 < 0 || e->weight100 > 100000)
        return 0; // The weight must be a non-negative integer.
    if (e->reps < 1 || e->reps > 1000)
        return 0; // The number of reps must be a positive integer.
    if (e->setNumber < 1 || e->setNumber > MAX_RECORDS)
        return 0; // The set number must be within the valid range.

    if (!ValidDate(e->date))
        return 0;

    int hasLetter = 0;

    for (size_t i = 0; e->exercise[i] != '\0'; ++i)
    {
        char ch = e->exercise[i];

        if (ch < 32 || ch > 126 || ch == '|') // The exercise name must be printable and cannot contain the pipe character.
            return 0;

        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z')) // The exercise name must contain at least one letter.
            hasLetter = 1;
    }

    if (!hasLetter)
        return 0;

    // Check relationships with existing records.

    for (size_t i = 0; i < db->count; ++i)
    {
        const WorkoutEntry *other = &db->data[i]; // Get a pointer to the current record in the database.

        if (other->id == ignoredId) // Skip the record which need to be edited
            continue;

        if (other->id == e->id) // The id must be unique and cannot be the same as any existing record's id.
            return 0;

        if (other->sessionId == e->sessionId)
        {
            if (strcmp(other->date, e->date) != 0) // if same session then date must be same
                return 0;

            if (strcmp(other->exercise, e->exercise) == 0 &&
                other->setNumber == e->setNumber) // if same session then exercise and set number must be unique
                return 0;
        }
    }

    return 1; // If all checks pass, the entry is valid.
}


//sort Entries

/*
sort by date if bydate=1 else by record id
if two same dates encounters,the one with larger recordID comes first
*/
void sortEntries(WorkoutDB *db, int byDate) 
{
    for (size_t i = 1; i < db->count; ++i) //i refers to the record we are currently sorting
    {
        WorkoutEntry key = db->data[i]; //saves copy of record
        size_t j = i; //position where we are trying to insert key

        while (j > 0)
        {
            WorkoutEntry *p = &db->data[j - 1]; //pointer to the record immediately before j

            int cmp = 0;

            if (byDate)
                cmp = strcmp(p->date, key.date);

            int greater = cmp > 0 ||
                          (cmp == 0 && p->id > key.id); //decides whether the preceding record should move right

            if (!greater)
                break;

            db->data[j] = *p;
            --j;
        }

        db->data[j] = key;
    }
}





//Change workout

/*Updates or delete given ID by user
if deleting=1->It deletes that Id and ask first about deletion
if deleting=0->Changes Workout*/

void changeWorkout(WorkoutDB *db, int deleting)
{
    int id;
    if (!readInt("Record ID: ", 1, INT_MAX - 1, &id))
        return;
    sortEntries(db, 0); //Sort entries by Record ID for binary searching
    int pos = findById(db->data, 0, (int)db->count - 1, id);
    if (pos < 0) //If findbyId returns 1 in case of ID not found
    {
        puts("Record not found.");
        return;
    }
    puts("Current entry:");
    showWorkouts(db, "", id); //Shows current entry that would be updated
    if (deleting) //If block for deleting that record
    {
        int yes;
        if (!readInt("Delete this set? 1=yes, 0=no: ", 0, 1, &yes) || !yes)
            return;
        //Close the gap by copying every later complete row left
        for (size_t i = (size_t)pos; i + 1 < db->count; ++i)
            db->data[i] = db->data[i + 1];
        --db->count;
    }
    else //Runs if deleting=0
    {
        WorkoutEntry e = db->data[pos]; //Hold entry temporarily before validing it
        double kg;
        if (!readLine("Correct exercise: ", e.exercise,
                      sizeof e.exercise))
            return;
        puts("Session, date and set number stay fixed; relog to correct them.");
        if (!readDouble("Correct weight kg: ", 0, 1000, &kg) ||
            !readInt("Correct reps: ", 1, 1000, &e.reps))
            return;
        e.weight100 = (int)(kg * 100.0 + 0.5);
        if (!validEntry(db, &e, id)) //If entry could not be verified
        {
            puts("Invalid edit or duplicate session/exercise/set; old row kept.");
            return;
        }
        db->data[pos] = e; //that temporary entry is now being added in database
    }
    if(deleting)
    puts("Deleted. PRs will be rebuilt from the remaining sets.");
    if (!deleting){ //Shows updated workout entry
        puts("Updated. PRs will be rebuilt from the remaining sets.");
        showWorkouts(db, "", id);}
}