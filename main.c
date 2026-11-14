#include "tracker.h"

int main(void)
{
    WorkoutDB db = {0};
    db.nextId = db.nextSession = 1;
    if (!reserveEntries(&db, 8) || !loadData(&db, DATA_FILE))
    {
        puts("Startup failed; existing file was not overwritten.");
        free(db.data);
        return EXIT_FAILURE;
    }
    int running = 1, status = EXIT_SUCCESS;
    while (running)
    {
        int choice, sub, id, yes;
        char name[NAME_LEN] = "";
        puts("\n\tWelcome To GymTrack \n\tHEALTH TRACKER");
        puts("\t1. Log sets");
        puts("\t2. View history");
        puts("\t3. Search");
        puts("\t4. Sort history");
        puts("\t5. Edit entry");
        puts("\t6. Delete entry");
        puts("\t7. Workout plans");
        puts("\t8. Calorie estimate");
        puts("\t9. Personal records");
        puts("\t10. Save data");
        puts("\t11. Load data");
        puts("\t0. Exit (save first)");
        if (!readInt("Choice: ", 0, 11, &choice))
            choice = 0;
        switch (choice)
        {
        case 1:
            addSets(&db);
            break;
        case 2:
            showWorkouts(&db, "", 0);
            break;
        case 3:
            puts("Search: 1 Exercise 2 Record ID 0 Back");
            if (!readInt("Search choice: ", 0, 2, &sub))
                break;
            switch (sub)
            {
            case 1:
                if (readLine("Exact exercise name: ", name, sizeof name))
                {
                    if (name[0])
                        showWorkouts(&db, name, 0);
                    else
                        puts("Enter a nonempty name.");
                }
                break;
            case 2:
                if (!readInt("Record ID: ", 1, INT_MAX - 1, &id))
                    break;
                sortEntries(&db, 0);
                if (findById(db.data, 0, (int)db.count - 1, id) < 0)
                    puts("Record not found.");
                else
                    showWorkouts(&db, "", id);
                break;
            case 0:
                break;
            }
            break;
        case 4:
            puts("Sort: \n1. By ID \n2. By Date \n0. Back");
            if (readInt("Sort choice: ", 0, 2, &sub) && sub != 0)
            {
                sortEntries(&db, sub == 2);
                showWorkouts(&db, "", 0);
            }
            break;
        case 5:
            changeWorkout(&db, 0);
            break;
        case 6:
            changeWorkout(&db, 1);
            break;
        case 7:
            generatePlan();
            break;
        case 8:
            calculateCalories();
            break;
        case 9:
            puts("PR: \n1. All exercises \n2. One exercise \n0. Back");
            if (!readInt("PR choice: ", 0, 2, &sub) || sub == 0)
                break;
            if (sub == 2)
            {
                if (!readLine("Exact exercise name: ", name, sizeof name))
                    break;
                if (!name[0])
                {
                    puts("Enter a nonempty name.");
                    break;
                }
            }
            showPR(&db, name);
            break;
        case 10:
            if (!saveData(&db, DATA_FILE))
                puts("Save failed; memory retained.");
            break;
        case 11:
            if (readInt("Replace memory from file? 1=yes 0=no: ", 0, 1, &yes) && yes && !loadData(&db, DATA_FILE))
                puts("Load failed; memory retained.");
            break;
        case 0:
            if (saveData(&db, DATA_FILE))
                running = 0;
            else
            {
                puts("Save failed. 0=return to menu, 1=exit without saving.");
                if (!readInt("Choice: ", 0, 1, &yes) || yes)
                {
                    status = EXIT_FAILURE;
                    running = 0;
                }
            }
            break;
        }
    }
    free(db.data);
    puts("Memory released. Goodbye!");
    return status;
}
