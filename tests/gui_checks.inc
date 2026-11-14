/* Included ONLY by a build compiled with -DHT_GUI_TEST.
 * Exercises the real native controls, callbacks and database in an isolated
 * executable directory. Never included in the production executable.
 * Confirmation popups are not automated; delete/reload core behavior is
 * checked without clicking those dialogs. No external UI automation needed.
 */
static void gui_click(HWND parent, int id)
{
    SendMessageA(parent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
                 (LPARAM)GetDlgItem(parent, id));
}

static void gui_select(HWND controlWindow, int id, int item)
{
    selectChoice(controlWindow, item);
    SendMessageA(mainWindow, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE),
                 (LPARAM)controlWindow);
}

static int gui_row_id(int row)
{
    LVITEMA item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    return ListView_GetItem(recordList, &item) ? (int)item.lParam : 0;
}

static int gui_write_surface(const char *path, const HTSurface *s)
{
    BITMAPFILEHEADER fileHeader={0}; BITMAPINFOHEADER info={0};
    DWORD bytes=(DWORD)((size_t)fxWidth*fxHeight*4);
    fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof fileHeader+sizeof info;
    fileHeader.bfSize=fileHeader.bfOffBits+bytes;
    info.biSize=sizeof info;info.biWidth=fxWidth;info.biHeight=-fxHeight;
    info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=bytes;
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(&fileHeader,sizeof fileHeader,1,f)==1 && fwrite(&info,sizeof info,1,f)==1 &&
           fwrite(s->pixels,bytes,1,f)==1;
    fclose(f);return ok;
}

static int ht_gui_checks(void)
{
    int count = 0, failed = 0;
    char text[8192];
    WorkoutEntry previous;
    FILE *log = fopen("gui-test-results.txt", "w");
    if (!log) return 1;
    ShowWindow(mainWindow, SW_HIDE);
#define GUI_CHECK(expression, description) do { \
    ++count; \
    if (!(expression)) { \
        fprintf(log, "FAIL %d: %s (line %d)\n", count, description, __LINE__); \
        fflush(log); failed = 1; goto finish; \
    } \
    fprintf(log, "PASS %d: %s\n", count, description); fflush(log); \
} while (0)

    GUI_CHECK(IsWindow(mainWindow) && IsWindow(recordList), "native main window and record table exist");
    GUI_CHECK(db.count == 0 && db.nextId == 1 && db.nextSession == 1,
              "test starts with a separate empty data directory");
    GUI_CHECK(ListView_GetItemCount(recordList) == 0, "initial native table is empty");

    gui_click(mainWindow, ACT_ADD);
    GUI_CHECK(IsWindow(logDialog.window), "Log sets opens the real native form");
    ShowWindow(logDialog.window, SW_HIDE);
    GUI_CHECK(!IsWindowEnabled(mainWindow), "logging form owns the disabled main window");
    SetWindowTextA(logDialog.date, "2026-11-14");
    SetWindowTextA(logDialog.name, "Bench press");
    SetWindowTextA(logDialog.weight, "40.125");
    SetWindowTextA(logDialog.reps, "10");
    const char *invalidDates[] = {"2026-02-29", "2026-04-31", "1899-12-31", "2100-01-01",
                                 "2024-02-29suffix", "2024-2-29", "2026-13-01", "2026-02-00", ""};
    for (size_t dateIndex = 0; dateIndex < sizeof invalidDates / sizeof invalidDates[0]; ++dateIndex)
    {
        SetWindowTextA(logDialog.date, invalidDates[dateIndex]);
        GetWindowTextA(logDialog.dateFeedback, text, sizeof text);
        GUI_CHECK(!logDialog.dateValid && !IsWindowEnabled(GetDlgItem(logDialog.window, LOG_ADD)) &&
                  !IsWindowEnabled(GetDlgItem(logDialog.window, LOG_COMMIT)) &&
                  strstr(text, "Invalid date") != NULL,
                  "invalid calendar date immediately shows feedback and disables actions");
        gui_click(logDialog.window, LOG_ADD);
        GUI_CHECK(logDialog.count == 0 && db.count == 0 && db.nextId == 1 && db.nextSession == 1,
                  "invalid date cannot stage a set even through a direct command");
        gui_click(logDialog.window, LOG_COMMIT);
        GUI_CHECK(IsWindow(logDialog.window) && db.count == 0 && db.nextId == 1 && db.nextSession == 1,
                  "invalid date cannot commit or consume record and session IDs");
    }
    const char *validDates[] = {"2024-02-29", "2000-02-29", "1900-01-01", "2099-12-31"};
    for (size_t dateIndex = 0; dateIndex < sizeof validDates / sizeof validDates[0]; ++dateIndex)
    {
        SetWindowTextA(logDialog.date, validDates[dateIndex]);
        GUI_CHECK(logDialog.dateValid && IsWindowEnabled(GetDlgItem(logDialog.window, LOG_ADD)) &&
                  IsWindowEnabled(GetDlgItem(logDialog.window, LOG_COMMIT)),
                  "correcting the date to a valid leap day or boundary year re-enables actions");
    }
    SetWindowTextA(logDialog.date, "2026-11-14");
    gui_click(logDialog.window, LOG_ADD);
    GUI_CHECK(logDialog.count == 1 && ListView_GetItemCount(logDialog.list) == 1,
              "first set enters the staging table");
    GUI_CHECK(logDialog.weights[0] == 40.13 && logDialog.repetitions[0] == 10,
              "staged weight rounds to hundredths and stores repetitions");
    SetWindowTextA(logDialog.date, "2026-02-29");
    gui_click(logDialog.window, LOG_COMMIT);
    GUI_CHECK(logDialog.count == 1 && db.count == 0 && db.nextId == 1 && db.nextSession == 1 &&
              IsWindow(logDialog.window),
              "changing an already staged session to an invalid date blocks commit without losing its preview");
    SetWindowTextA(logDialog.date, "2026-11-14");
    SetWindowTextA(logDialog.weight, "60.25");
    SetWindowTextA(logDialog.reps, "6");
    gui_click(logDialog.window, LOG_ADD);
    GUI_CHECK(logDialog.count == 2 && logDialog.weights[1] == 60.25 &&
              logDialog.repetitions[1] == 6, "second set has independent weight and repetitions");
    ListView_SetItemState(logDialog.list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    SetWindowTextA(logDialog.weight, "41.50");
    SetWindowTextA(logDialog.reps, "11");
    gui_click(logDialog.window, LOG_UPDATE);
    GUI_CHECK(logDialog.count == 2 && logDialog.weights[0] == 41.5 &&
              logDialog.repetitions[0] == 11, "Update selected changes the staged set");
    ListView_SetItemState(logDialog.list, 1, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    gui_click(logDialog.window, LOG_REMOVE);
    GUI_CHECK(logDialog.count == 1, "Remove selected deletes only the staged row");
    SetWindowTextA(logDialog.weight, "62.75");
    SetWindowTextA(logDialog.reps, "7");
    gui_click(logDialog.window, LOG_ADD);
    gui_click(logDialog.window, LOG_COMMIT);
    GUI_CHECK(!IsWindow(logDialog.window) && IsWindowEnabled(mainWindow),
              "committing closes the form and re-enables the main window");
    GUI_CHECK(db.count == 2 && dirty && ListView_GetItemCount(recordList) == 2,
              "committing stores two actual records and refreshes the table");
    GUI_CHECK(record(1) && record(2) && record(1)->sessionId == 1 &&
              record(2)->sessionId == 1 && record(1)->setNumber == 1 &&
              record(2)->setNumber == 2, "logged sets share a session and receive distinct IDs");
    GUI_CHECK(record(1)->weight100 == 4150 && record(1)->reps == 11 &&
              record(2)->weight100 == 6275 && record(2)->reps == 7,
              "committed rows preserve both per-set values");

    gui_click(mainWindow, ACT_ADD);
    GUI_CHECK(IsWindow(logDialog.window), "another session can be opened");
    ShowWindow(logDialog.window, SW_HIDE);
    SetWindowTextA(logDialog.date, "2026-11-10");
    SetWindowTextA(logDialog.name, "Squat");
    SetWindowTextA(logDialog.weight, "100");
    SetWindowTextA(logDialog.reps, "5");
    gui_click(logDialog.window, LOG_ADD);
    gui_click(logDialog.window, LOG_COMMIT);
    GUI_CHECK(db.count == 3 && record(3) && record(3)->sessionId == 2 &&
              db.nextId == 4 && db.nextSession == 3,
              "another session advances record and session counters");

    gui_click(mainWindow, NAV_RECORDS);
    GUI_CHECK(view == 1, "sidebar navigates to workout records");
    gui_select(filterMode, FILTER_MODE, 0);
    SetWindowTextA(filterText, "Bench press");
    gui_click(mainWindow, ACT_FILTER);
    GUI_CHECK(ListView_GetItemCount(recordList) == 2, "exercise search filters the real table");
    SetWindowTextA(filterText, "bench press");
    gui_click(mainWindow, ACT_FILTER);
    GUI_CHECK(ListView_GetItemCount(recordList) == 0, "exercise search retains exact case-sensitive semantics");
    gui_select(filterMode, FILTER_MODE, 1);
    SetWindowTextA(filterText, "2");
    gui_click(mainWindow, ACT_FILTER);
    GUI_CHECK(ListView_GetItemCount(recordList) == 1 && gui_row_id(0) == 2,
              "record-ID search returns the requested record");
    gui_click(mainWindow, ACT_CLEAR);
    GUI_CHECK(ListView_GetItemCount(recordList) == 3, "Clear restores all records");
    gui_select(sortMode, SORT_MODE, 1);
    GUI_CHECK(gui_row_id(0) == 3 && gui_row_id(1) == 1 && gui_row_id(2) == 2,
              "date sorting orders dates then equal-date record IDs");
    gui_select(sortMode, SORT_MODE, 0);
    GUI_CHECK(gui_row_id(0) == 1 && gui_row_id(1) == 2 && gui_row_id(2) == 3,
              "record-ID sorting restores ascending IDs");

    ListView_SetItemState(recordList, 1, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    GUI_CHECK(selectedRecord() == 2 && IsWindowEnabled(toolbar[1]),
              "selecting a record enables Edit set");
    previous = *record(2);
    gui_click(mainWindow, ACT_EDIT);
    GUI_CHECK(IsWindow(logDialog.window) && logDialog.editId == 2,
              "Edit set opens the selected record in the native editor");
    ShowWindow(logDialog.window, SW_HIDE);
    GUI_CHECK(!IsWindowEnabled(logDialog.date), "record date remains fixed in the editor");
    SetWindowTextA(logDialog.weight, "65");
    SetWindowTextA(logDialog.reps, "8");
    gui_click(logDialog.window, EDIT_COMMIT);
    GUI_CHECK(record(2)->weight100 == 6500 && record(2)->reps == 8,
              "Save changes commits the edited weight and repetitions");
    GUI_CHECK(record(2)->id == previous.id && record(2)->sessionId == previous.sessionId &&
              record(2)->setNumber == previous.setNumber && !strcmp(record(2)->date, previous.date),
              "editing preserves record ID, session ID, date and set number");

    gui_click(mainWindow, NAV_PR);
    GUI_CHECK(view == 2, "sidebar opens personal records");
    SetWindowTextA(prText, "Bench press");
    gui_click(mainWindow, PR_RUN);
    GetWindowTextA(report, text, sizeof text);
    GUI_CHECK(strstr(text, "highest recorded weight 65.00 kg") != NULL &&
              strstr(text, "41.50 kg") != NULL && strstr(text, "11 reps") != NULL,
              "PR report shows the current heaviest load and reps at each load");
    GUI_CHECK(strstr(text, "Squat") == NULL, "PR exercise filter excludes other exercises");

    gui_click(mainWindow, NAV_PLAN);
    gui_select(planGroups[0], PLAN_GROUP, 1);
    gui_select(planGroups[1], PLAN_GROUP + 1, 6);
    GUI_CHECK(IsWindowEnabled(planOptions[0][0]) &&
              SendMessage(planOptions[0][0], CB_GETCOUNT, 0, 0) == 2,
              "choosing a muscle group populates its native exercise controls");
    selectChoice(planOptions[0][1], 1);
    selectChoice(planOptions[1][2], 1);
    gui_click(mainWindow, PLAN_RUN);
    GetWindowTextA(report, text, sizeof text);
    GUI_CHECK(strstr(text, "Incline barbell press") != NULL &&
              strstr(text, "Bulgarian Split Squats") != NULL,
              "plan generation uses the selected exercise variations");
    GUI_CHECK(strstr(text, "   3  8-12 or to failure   180") != NULL,
              "two selected groups produce the original sets, reps and rest values");

    gui_click(mainWindow, NAV_CAL);
    selectChoice(calUnit, 0);
    SetWindowTextA(calFields[0], "70"); SetWindowTextA(calFields[1], "75");
    SetWindowTextA(calFields[2], "2500"); SetWindowTextA(calFields[3], "3");
    gui_click(mainWindow, CAL_RUN);
    GetWindowTextA(report, text, sizeof text);
    GUI_CHECK(strstr(text, "BULKING PLAN") != NULL && strstr(text, "2922 kcal") != NULL,
              "calorie form computes and displays the original kg formula");
    selectChoice(calUnit, 1);
    SetWindowTextA(calFields[0], "175"); SetWindowTextA(calFields[1], "175");
    SetWindowTextA(calFields[2], "2600");
    gui_click(mainWindow, CAL_RUN);
    GetWindowTextA(report, text, sizeof text);
    GUI_CHECK(strstr(text, "MAINTENANCE PLAN") != NULL && strstr(text, "2600 kcal") != NULL,
              "pounds selection and maintenance result work through the form");

    gui_click(mainWindow, ACT_SAVE);
    GUI_CHECK(!dirty && GetFileAttributesA(DATA_FILE) != INVALID_FILE_ATTRIBUTES,
              "Save data writes HT1 and clears the unsaved flag");
    GUI_CHECK(ht_delete(&db, 2) && db.count == 2, "delete bridge removes a saved row without reusing IDs");
    refreshRecords();
    GUI_CHECK(ListView_GetItemCount(recordList) == 2 && db.nextId == 4,
              "table refresh reflects deletion while preserving the next ID");
    GUI_CHECK(ht_run(HT_LOAD, &db, "", DATA_FILE) && db.count == 3 && record(2) &&
              record(2)->weight100 == 6500 && db.nextId == 4 && db.nextSession == 3,
              "reload restores the saved edited records and counters");
    sortEntries(&db, 0); refreshRecords();
    GUI_CHECK(ListView_GetItemCount(recordList) == 3, "native table refreshes after reload");
    gui_click(mainWindow, ACT_SAVE);
    GUI_CHECK(GetFileAttributesA("workouts.txt.bak") != INVALID_FILE_ATTRIBUTES,
              "a later Save data creates the original backup file");
    gui_click(mainWindow, NAV_HOME);
    GUI_CHECK(view == 0, "sidebar returns to the overview");
    gui_click(mainWindow, ACT_ADD);
    GUI_CHECK(IsWindow(logDialog.window), "a new draft can be opened after all operations");
    ShowWindow(logDialog.window, SW_HIDE);
    gui_click(logDialog.window, LOG_CANCEL);
    GUI_CHECK(db.count == 3 && db.nextId == 4 && db.nextSession == 3,
              "Cancel leaves all records and counters unchanged");

    gui_click(mainWindow, ACT_ADD);
    GUI_CHECK(IsWindow(logDialog.window), "a valid leap-day logging form opens");
    ShowWindow(logDialog.window, SW_HIDE);
    SetWindowTextA(logDialog.date, "2024-02-29");
    SetWindowTextA(logDialog.name, "Leap day test");
    SetWindowTextA(logDialog.weight, "25");
    SetWindowTextA(logDialog.reps, "8");
    gui_click(logDialog.window, LOG_ADD);
    GUI_CHECK(logDialog.count == 1, "a real leap day can stage a set");
    gui_click(logDialog.window, LOG_COMMIT);
    GUI_CHECK(db.count == 4 && record(4) && !strcmp(record(4)->date, "2024-02-29") &&
              db.nextId == 5 && db.nextSession == 4,
              "2024-02-29 commits successfully with preserved exact date and correct IDs");


    size_t effectCount=db.count;int savedId=db.nextId,savedSession=db.nextSession;
    ShowWindow(mainWindow,SW_SHOW);SetForegroundWindow(mainWindow);UpdateWindow(mainWindow);fxMotion=TRUE;
    switchView((view+1)%5);
    GUI_CHECK(fxActive && fxControlCount>0 && fxOffset==S(72),
              "page change starts a visible cached rendering surface with a 72 pixel slide");
    HWND moving=fxControls[0].window;RECT base=fxControls[0].rect,actual;
    GetWindowRect(moving,&actual);MapWindowPoints(NULL,mainWindow,(POINT*)&actual,2);
    GUI_CHECK(actual.left==base.left && !IsWindowVisible(moving) && IsWindowVisible(fxOverlay),
              "live controls keep their geometry and are hidden beneath the visible animation surface");
    unsigned beforeTicks=fxTickCount,beforeFrames=fxPaintCount;int positions=0,lastOffset=fxOffset;
    ULONGLONG deadline=GetTickCount64()+2000;
    while(fxActive && GetTickCount64()<deadline){
        MSG pending;
        while(PeekMessage(&pending,NULL,0,0,PM_REMOVE)){
            if(!IsDialogMessage(mainWindow,&pending)){TranslateMessage(&pending);DispatchMessage(&pending);}
            if(lastOffset!=fxOffset){positions++;lastOffset=fxOffset;}
        }
        if(lastOffset!=fxOffset){positions++;lastOffset=fxOffset;}
        MsgWaitForMultipleObjects(0,NULL,FALSE,8,QS_ALLINPUT);
    }
    fprintf(log,"LIVE MOTION: %u timer ticks, %u rendered frames, %d distinct slide positions\n",
            fxTickCount-beforeTicks,fxPaintCount-beforeFrames,positions);
    fprintf(log,"RENDERER: %s; last result 0x%08lx\n",fxDirect?"Direct2D":"GDI",(unsigned long)fxGraphicsResult);
    double maxGap=0,totalGap=0,maxCost=0;
    for(unsigned i=0;i<fxSamples;i++){
        if(fxFrameCosts[i]>maxCost)maxCost=fxFrameCosts[i];
        if(i){double gap=fxFrameTimes[i]-fxFrameTimes[i-1];totalGap+=gap;if(gap>maxGap)maxGap=gap;}
    }
    fprintf(log,"FRAME TIMING: mean %.2f ms, worst %.2f ms, maximum compose/present %.2f ms\n",
        fxSamples>1?totalGap/(fxSamples-1):0,maxGap,maxCost);
    GUI_CHECK(positions>=15 && fxPaintCount-beforeFrames>=20 && maxGap<50,
              "420 ms live transition renders at least 20 frames with no gap reaching 50 ms");
    GUI_CHECK(!fxActive && fxOffset==0,"animation ends without an active timer or displaced controls");
    GetWindowRect(moving,&actual);MapWindowPoints(NULL,mainWindow,(POINT*)&actual,2);
    GUI_CHECK(actual.left==base.left && actual.top==base.top && IsWindowVisible(moving),
              "live controls become visible at exactly their original destination");
    size_t changedPixels=0;
    for(int y=0;y<fxHeight;y++)for(int x=fxLeft;x<fxWidth;x++)
        if((fxFrom.pixels[(size_t)y*fxWidth+x]&0xffffff)!=(fxTo.pixels[(size_t)y*fxWidth+x]&0xffffff))changedPixels++;
    GUI_CHECK(changedPixels>10000,"outgoing and incoming page captures contain different rendered content");
    GUI_CHECK(gui_write_surface("motion-from.bmp",&fxFrom) && gui_write_surface("motion-to.bmp",&fxTo),
              "actual rendering surfaces export for visual inspection");
    for(int frame=0;frame<=24;frame++){
        char path[64];snprintf(path,sizeof path,"motion-frame-%02d.bmp",frame);
        fxCompose((double)frame/24);
        for(int y=0;y<fxHeight;y++)memcpy(fxFrame.pixels+(size_t)y*fxWidth,
            fxTo.pixels+(size_t)y*fxWidth,(size_t)fxLeft*4);
        GUI_CHECK(gui_write_surface(path,&fxFrame),"transition frame exports for visual inspection");
    }
    fxOffset=0;
    for(int size=0;size<2;size++){
        if(size)ShowWindow(mainWindow,SW_MAXIMIZE);
        for(int page=0;page<5;page++){
            double requested=fxNow();switchView((view+1)%5);double prepared=fxNow()-requested;
            deadline=GetTickCount64()+2000;
            while(fxActive && GetTickCount64()<deadline){
                MSG pending;
                while(PeekMessage(&pending,NULL,0,0,PM_REMOVE)){
                    if(!IsDialogMessage(mainWindow,&pending)){TranslateMessage(&pending);DispatchMessage(&pending);}
                }
                MsgWaitForMultipleObjects(0,NULL,FALSE,1,QS_ALLINPUT);
            }
            maxGap=0;totalGap=0;maxCost=0;
            for(unsigned i=0;i<fxSamples;i++){
                if(fxFrameCosts[i]>maxCost)maxCost=fxFrameCosts[i];
                if(i){double gap=fxFrameTimes[i]-fxFrameTimes[i-1];totalGap+=gap;if(gap>maxGap)maxGap=gap;}
            }
            fprintf(log,"PAGE %d SIZE %dx%d: %u frames; mean %.2f ms, worst %.2f ms; render %.2f ms; preparation %.2f ms\n",
                view,fxWidth,fxHeight,fxSamples,fxSamples>1?totalGap/(fxSamples-1):0,maxGap,maxCost,prepared);
            GUI_CHECK(!fxActive && fxSamples>=20 && maxGap<50,
                      "each page at normal and maximized size meets the live frame pacing threshold");
        }
    }
    ShowWindow(mainWindow,SW_RESTORE);
    DWORD baseline=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<20;i++){switchView((view+1)%5);fxEpoch.QuadPart-=fxFrequency.QuadPart;
        SendMessage(fxOverlay,WM_TIMER,FX_FRAME_TIMER,0);}
    GUI_CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=baseline+4,"twenty native transitions do not accumulate GDI resources");
    switchView((view+1)%5);switchView((view+1)%5);
    GUI_CHECK(fxActive && fxOffset==S(72),"rapid navigation safely restarts the native transition");
    SendMessage(mainWindow,WM_SIZE,SIZE_RESTORED,0);
    GUI_CHECK(!fxActive && fxOffset==0,"resizing restores control positions and cancels animation");
    fxMotion=FALSE;switchView((view+1)%5);
    GUI_CHECK(!fxActive,"reduced-motion navigation skips movement");
    gui_click(mainWindow,172);
    GUI_CHECK(fxMotion && fxActive,"Preview explicitly enables motion and starts visible movement");
    gui_click(mainWindow,171);
    GUI_CHECK(!fxMotion && !fxActive,"Motion Off stops animation and restores controls");
    GUI_CHECK(db.count==effectCount && db.nextId==savedId && db.nextSession==savedSession,"animation and its controls do not alter workout data");
    ShowWindow(mainWindow,SW_HIDE);
finish:
    if (IsWindow(logDialog.window)) closeLog(logDialog.window);
    fprintf(log, "%s: %d native GUI integration checks.\n", failed ? "FAILED" : "PASSED", count);
    fprintf(log, "Confirmation dialogs and visual layout require separate manual checks.\n");
    fclose(log);
#undef GUI_CHECK
    return failed;
}
