// This file contains all the necessary header files , structures for id;s and workout entries and declaration of all functions used.

#ifndef TRACKER_H
#define TRACKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

#define NAME_LEN 48
#define MAX_RECORDS 10000
#define DATA_FILE "workouts.txt"
#define KG_TO_LB 2.20462

typedef struct
{
    int id;        // Unique Ids for each entry
    int sessionId; // same if same session, different if different session
    char exercise[NAME_LEN];
    int weight100; // take weight as weight * 100 to avoid floating point issues
    int reps;
    int setNumber;
    char date[11];
} WorkoutEntry;

typedef struct
{
    WorkoutEntry *data;
    size_t count;
    size_t capacity;
    int nextId;
    int nextSession;
} WorkoutDB;

// input.c
int readLine(const char *prompt, char *out, size_t size); // takes the users prompt in string

int readInt(const char *prompt, int low, int high, int *out); // converts the prompt taken by readline into int and returns the integer after validating it.

int readDouble(const char *prompt, double low, double high, double *out); // converts the prompt taken by readline into double and returns the double after validating it.

int ValidDate(const char *s); // checks the date entered and validates if it is in correct format and in the 1900 to 2099 years and 1-12 month range

// features.c
void convertWeight(double value, int toPounds, double *out); // pounds->kg , kg->pounds , based on direction.

void calculateCalories(void); // calculates calorie targets and makes a plan , i.e bulking,cutting etc

void generatePlan(void); // generates a workout plan based on preferences i.e , muscle groups , sets etc

void showPR(WorkoutDB *db, const char *exercise); // displays the highest weight on an excercise and sets (in kgs and lb)

// Workout.c
int validEntry(const WorkoutDB *db, const WorkoutEntry *e, int ignoredId); // validates the entry of the excercise or weight

void addSets(WorkoutDB *db); // For adding/logging workouts (weights,sets,repetitions)

int findById(const WorkoutEntry *a, int left, int right, int id); // for searching the workout entry by its ID.

int reserveEntries(WorkoutDB *db, size_t need); // for allocating memory dynamically to the workout database 

void sortEntries(WorkoutDB *db, int byDate); // sort by date if bydate=1 else by record id

void showWorkouts(const WorkoutDB *db, const char *exercise, int recordId); // for displaying workout entries

void changeWorkout(WorkoutDB *db, int deleting); // change workout entries

// Storage.c
int saveData(const WorkoutDB *db, const char *path); // makes a temp storage file , then replaces with saved file

int loadData(WorkoutDB *db, const char *path); // loading saved data before adding other entries

#endif
