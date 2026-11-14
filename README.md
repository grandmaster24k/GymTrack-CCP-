# 🏋️‍♂️ GymTrack – Gym & Health Tracker
> A modular, console-based Gym & Health Tracking System built in C.

# How to Build and Run

## CLI Version

**Requirements:**
* A C compiler (GCC or Clang)
* Any OS (Windows, Linux, macOS)
* All `.c` files and `tracker.h` must be in the same folder

**Instructions:**
* Do **NOT** compile `tracker.h` on its own; compile all the `.c` files together and run `main.c`.
* **Dev-C++ / Code::Blocks / VS Code:** Add all five `.c` files to the same project so they are compiled and linked together.
* `workouts.txt` is automatically created in the folder where you run the program.

## GUI Version

**Requirements:**
* Same as the CLI version above

**Instructions:**
* Navigate to the `GUI` folder.
* Run the **GymTrack** application.
* Save your data after logging.
* `workouts.txt` is automatically created in the folder where you run the program.


---

## 📌 Project Introduction
**GymTrack** is a console-based Gym & Health Tracking System developed in C. It is designed to help users track their workouts, monitor exercise progress, generate workout plans, and estimate daily calorie requirements.

* **Dual-Unit Metrics:** Records and displays all weights in both kilograms (kg) and pounds (lb).
* **Data Persistence:** Uses C file handling to store user data, ensuring your workout records persist between sessions.


---

## ⚙️ Project Structure
The project follows a modular design. It uses a custom header file, `tracker.h`, which contains function declarations and serves as a shared library across multiple source files.

### 📂 File Descriptions

| File Name | Description |
| :--- | :--- |
| `tracker.h` | Header file containing shared function declarations. |
| `main.c` | Contains the main program logic and core menu system. |
| `features.c` | Implements features like calorie conversion and workout plan generation. |
| `input.c` | Handles strict user input validation and error handling. |
| `workout.c` | Manages sorting algorithms and searching for workout entries. |
| `storage.c` | Manages workout data storage, file retrieval, and saving. |

---

## 🚀 Features
The application features an interactive, console-driven menu with the following capabilities:

* 📝 **Log Sets** — Record workout sets and exercise weights.
* 📜 **View History** — Display previously recorded workouts.
* 🔍 **Search** — Search for specific workout entries.
* 🗂️ **Sort History** — Organize workout records systematically.
* ✏️ **Edit Entry** — Modify existing workout records.
* 🗑️ **Delete Entry** — Remove unwanted history entries.
* 📋 **Workout Plans** — Generate simple, customized workout routines.
* 🧮 **Calorie Estimate** — Calculate daily target calorie requirements.
* 🏆 **Personal Records** — Keep track of your lifetime personal bests.
* 💾 **Save Data** — Commit workout information to persistent files.
* 🔄 **Load Data** — Retrieve previously saved sessions upon startup.
* ❌ **Exit** — Safely exit the program.

---

## 🛠️ Tech Specs & Environment

* **Programming Language:** C 
* **Project Type:** Console-Based Application (CLI)
* **Header Files:** Custom `tracker.h` library


This project is developed as part of our **1st Year CCP (Complex Coding Project)** at **NED University of Engineering & Technology**.


