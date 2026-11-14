#include "tracker.h"

// Weight Converter
void convertWeight(double value, int toPounds, double *out) // topounds is for direction , 1 = kgtopound , 0 = poundtokg
{
    if (!out)
        return;
    if (toPounds)
        *out = value * KG_TO_LB;
    else
        *out = value / KG_TO_LB;
}

// Calorie Calculator
void calculateCalories(void) // asks users current weight calories etc and make a plan for goal i.e Cutting , Bulking or Maintaining.
{
    double currentWeight, goalWeight, maintenanceCalories;
    double months, calorieFactor;
    int unit;

    puts("Choose weight unit: 1=kilograms (kg), 2=pounds (lb)");

    if (!readInt("Unit choice: ", 1, 2, &unit))
        return;

    if (unit == 1)
        calorieFactor = 7700;
    else
        calorieFactor = 3500;

    double minWeight = 10.0;
    double maxCurrentWeight = 300.0;
    double maxGoalWeight = 400.0;

    if (unit == 2)
    {
        minWeight *= KG_TO_LB;
        maxCurrentWeight *= KG_TO_LB;
        maxGoalWeight *= KG_TO_LB;
    }

    if (!readDouble(unit == 1 ? "Current weight (kg): " : "Current weight (lb): ", minWeight, maxCurrentWeight, &currentWeight) ||

        !readDouble(unit == 1 ? "Goal weight (kg): " : "Goal weight (lb): ", minWeight, maxGoalWeight, &goalWeight) ||

        !readDouble("Maintenance calories (kcal/day): ", 1, 10000, &maintenanceCalories) ||

        !readDouble("Duration (months): ", 0.01, 120, &months))
        return;

    double weightDifference = goalWeight - currentWeight;
    double days = months * 30.44;

    double dailyAdjustment = (weightDifference * calorieFactor) / days;
    double dailyCalories = maintenanceCalories + dailyAdjustment;

    double amount = weightDifference < 0 ? -weightDifference : weightDifference;

    double convertedDifference;

    convertWeight(amount, unit == 1, &convertedDifference);

    const char *selectedUnit = unit == 1 ? "kg" : "lb";
    const char *otherUnit = unit == 1 ? "lb" : "kg";

    if (weightDifference > 0 && dailyCalories > 0)
    {
        puts("\n--- BULKING PLAN ---");

        printf("Weight to gain: %.2f %s | %.2f %s\n", amount, selectedUnit, convertedDifference, otherUnit);

        printf("Target duration: %.1f months\n", months);
        printf("Estimated daily calories: %.0f kcal\n", dailyCalories);
    }

    else if (weightDifference < 0 && dailyCalories > 0)
    {
        puts("\n--- CUTTING PLAN ---");

        printf("Weight to lose: %.2f %s | %.2f %s\n", amount, selectedUnit, convertedDifference, otherUnit);

        printf("Target duration: %.1f months\n", months);
        printf("Estimated daily calories: %.0f kcal\n", dailyCalories);
    }

    else if (weightDifference == 0)
    {
        puts("\n--- MAINTENANCE PLAN ---");
        puts("You are already at your goal weight.");

        printf("Estimated daily calories: %.0f kcal\n", maintenanceCalories);
    }

    else
    {
        puts("The calculated calorie target is invalid.");
        puts("Choose a more realistic goal or longer duration.");
    }
}

// Workout Plan Generator
void generatePlan(void) // asks user the amount of muscles -> excercise for each muscle.
{
    const char *groups[] = {
        "Chest", "Back", "Shoulders", "Biceps",
        "Triceps", "Legs", "Abs"};

    const char *movements[7][3] = {
        {"Flat press", "Incline press", "Fly"},
        {"Vertical pull", "Barbell row", "Cable row"},
        {"Overhead press", "Lateral raise", "Rear-delt fly"},
        {"Curl", "Hammer curl", "Preacher curl"},
        {"Pushdown", "Overhead extension", "Lying extension"},
        {"Squat", "Hip hinge", "Leg press"},
        {"Crunch", "Reverse crunch", "Core exercise"}};

    const char *exerciseOptions[7][3][2] = {/*using 3d array because , 7muscles , 3movements each , 2variations for each movement*/
                                            {{"Dumbbell flat press", "Barbell flat press"},
                                             {"Incline dumbbell press", "Incline barbell press"},
                                             {"Cable fly", "Pec Dec fly"}},
                                            {{"Lat pulldown", "Pull Ups"},
                                             {"Barbell row", "Dumbbell row"},
                                             {"Seated cable row", "Dumbbell Row"}},
                                            {{"Dumbbell overhead press", "Barbell overhead press"},
                                             {"Dumbbell lateral raise", "Cable lateral raise"},
                                             {"Reverse dumbbell fly", "Face Pulls"}},
                                            {{"Dumbbell curl", "Barbell curl"},
                                             {"Dumbbell hammer curl", "Cable hammer curl"},
                                             {"Incline Dumbbell Curl", "Bayesian Curl"}},
                                            {{"Tricep Extension", "Closegrip Benchpress"},
                                             {"Overhead cable extension", "Overhead dumbbell extension"},
                                             {"Single arm Tricep pushdown", "Dumbell Tricep kickbacks"}},
                                            {{"Barbell squat", "Leg Press"},
                                             {"Romanian deadlift", "Hamstring Curl"},
                                             {"Lunges", "Bulgarian Split Squats"}},
                                            {{"Body weight Crunches", "Cable crunches"},
                                             {"Leg Raises", "Hanging knee raise"},
                                             {"Dragon Flag", "Plank"}}};

    int groupCount;

    if (!readInt("How many muscle groups do you want to train (1-4)? ",
                 1, 4, &groupCount))
    {
        return;
    }

    int setsPerExercise;

    if (groupCount == 1)
        setsPerExercise = 6;

    else if (groupCount == 2)
        setsPerExercise = 3;

    else
        setsPerExercise = 2;

    puts("\nMuscle groups:");
    for (int i = 0; i < 7; ++i)
    {
        printf("%d. %s\n", i + 1, groups[i]);
    }

    int selectedGroups[4];
    int selectedCount = 0;

    while (selectedCount < groupCount)
    {
        int choice;

        printf("Choose muscle group %d of %d: ",
               selectedCount + 1, groupCount);

        if (!readInt("", 1, 7, &choice))
        {
            return;
        }

        int alreadySelected = 0;
        for (int i = 0; i < selectedCount; ++i)
        {
            if (selectedGroups[i] == choice)
            {
                alreadySelected = 1;
                break;
            }
        }

        if (alreadySelected)
        {
            puts("That group is already selected. Choose a different one.");
            continue;
        }

        selectedGroups[selectedCount++] = choice;
    }

    int selectedOptions[4][3];

    for (int i = 0; i < groupCount; ++i)
    {
        int groupIndex = selectedGroups[i] - 1;

        printf("\nChoose exercises for %s:\n", groups[groupIndex]);

        for (int j = 0; j < 3; ++j)
        {
            printf("%s:\n", movements[groupIndex][j]);
            printf("  1. %s\n", exerciseOptions[groupIndex][j][0]);
            printf("  2. %s\n", exerciseOptions[groupIndex][j][1]);

            if (!readInt("Option (1-2): ", 1, 2,
                         &selectedOptions[i][j]))
            {
                return;
            }
        }
    }

    puts("\nWorkout plan:");
    puts("Exercise                           Sets  Reps   Rest(seconds)");

    for (int i = 0; i < groupCount; ++i)
    {
        int groupIndex = selectedGroups[i] - 1;

        printf("\n%s:\n", groups[groupIndex]);

        for (int j = 0; j < 3; ++j)
        {
            int optionIndex = selectedOptions[i][j] - 1;

            printf("%-34s %4d  8-12 or to failure   180\n",
                   exerciseOptions[groupIndex][j][optionIndex], /*displays the workout plan based on previous inputs.*/
                   setsPerExercise);
        }
    }
}

// showPR
void showPR(WorkoutDB *db, const char *exercise) // used the workout database and excercise name for PR
{
    int shown = 0;
    /* Visit each exercise once, even if it appears in many rows. */
    for (size_t i = 0; i < db->count; ++i)
    {
        const WorkoutEntry *e = &db->data[i];
        if (exercise[0] && strcmp(e->exercise, exercise))
            continue;
        int seen = 0;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(db->data[j].exercise, e->exercise))
                seen = 1;
        if (seen)
            continue;
        ++shown;
        /* Largest load for this exercise. */
        int best = 0;
        for (size_t j = 0; j < db->count; ++j)
            if (!strcmp(db->data[j].exercise, e->exercise) &&
                db->data[j].weight100 > best)
                best = db->data[j].weight100;
        double kg = best / 100.0, lb;
        convertWeight(kg, 1, &lb);
        printf("%s: highest recorded weight %.2f kg / %.2f lb\n",
               e->exercise, kg, lb);
        /* In the above part , first it checks the excercise name and finds it in the workout database
        then it finds the highest load/weight used in that excercise
        */
        /* At each distinct load, find the largest repetition count. */
        for (size_t j = 0; j < db->count; ++j)
        {
            const WorkoutEntry *p = &db->data[j];
            if (strcmp(p->exercise, e->exercise))
                continue;
            int prior = 0, reps = p->reps;
            for (size_t k = 0; k < j; ++k)
                if (!strcmp(db->data[k].exercise, e->exercise) &&
                    db->data[k].weight100 == p->weight100)
                    prior = 1;
            if (prior)
                continue;
            for (size_t k = 0; k < db->count; ++k)
                if (!strcmp(db->data[k].exercise, e->exercise) &&
                    db->data[k].weight100 == p->weight100 &&
                    db->data[k].reps > reps)
                    reps = db->data[k].reps;
            kg = p->weight100 / 100.0;
            convertWeight(kg, 1, &lb);
            printf(" %.2f kg / %.2f lb: %d reps\n", kg, lb, reps);
        } // this part finds the number of sets performed with that weight and displays it in both kg and lb
    }
    if (!shown) // if no excercise with that name is found
        puts("No matching exercise records.");
}