/* Presentation-only effects. No tracker state or persistence is changed here.
 * Timers run only during a transition/hover, and obey Windows reduced motion.
 * The blurred ambient backdrop is generated locally, never a desktop capture.
 */
#ifndef HT_GUI_EFFECTS_H
#define HT_GUI_EFFECTS_H

static BOOL fxMotion = TRUE;
static int fxUserOverride = -1;
static HBITMAP fxAmbient;
static HDC fxBackdropDC;
static HBITMAP fxBackdropBitmap;
static HGDIOBJ fxBackdropOld;
static int fxBackdropWidth, fxBackdropHeight;
static HWND fxOverlay;
static unsigned fxPaintCount, fxTickCount;
static const UINT_PTR FX_FRAME_TIMER = 71, FX_HOVER_TIMER = 72;
static const DWORD FX_PAGE_MS = 420, FX_HOVER_MS = 180;
static DWORD fxDuration = 420;

typedef struct {
  double hover, from, target;
  ULONGLONG started;
  BOOL tracking, pressed;
} HTButtonMotion;

static double fxEase(double t) {
  if (t <= 0)
    return 0;
  if (t >= 1)
    return 1;
  return 1 - (1 - t) * (1 - t) * (1 - t);
}
static COLORREF fxMix(COLORREF a, COLORREF b, double t) {
  return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
             (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
             (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
static void fxReadPreferences(void) {
  if (fxUserOverride >= 0) {
    fxMotion = fxUserOverride;
    return;
  }
  BOOL allowed = TRUE;
  if (SystemParametersInfoA(0x1042 /* SPI_GETCLIENTAREAANIMATION */, 0,
                            &allowed, 0))
    fxMotion = allowed;
#ifdef HT_GUI_TEST
  fxMotion = FALSE;
#endif
}
static void fxDrawAmbient(HDC dc, int width, int height) {
  enum { BW = 256, BH = 192 };
  if (!fxAmbient) {
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = BW;
    bi.bmiHeader.biHeight = -BH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD *pixels = NULL;
    fxAmbient =
        CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (fxAmbient && pixels) {
      for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
          double u = (double)x / BW, v = (double)y / BH;
          double a = exp(
              -((u - .72) * (u - .72) / .047 + (v - .12) * (v - .12) / .12));
          double b =
              exp(-((u - .94) * (u - .94) / .06 + (v - .83) * (v - .83) / .12));
          double c = exp(
              -((u - .08) * (u - .08) / .026 + (v - .16) * (v - .16) / .16));
          int red = (int)(10 + 10 * a + 27 * b + 2 * c);
          int green = (int)(14 + 28 * a + 8 * b + 12 * c);
          int blue = (int)(23 + 54 * a + 41 * b + 23 * c);
          pixels[y * BW + x] = (DWORD)((red << 16) | (green << 8) | blue);
        }
    }
  }
  if (!fxAmbient) {
    RECT r = {0, 0, width, height};
    FillRect(dc, &r, bgBrush);
    return;
  }
  if (!fxBackdropDC || width != fxBackdropWidth || height != fxBackdropHeight) {
    if (fxBackdropDC) {
      SelectObject(fxBackdropDC, fxBackdropOld);
      DeleteObject(fxBackdropBitmap);
      DeleteDC(fxBackdropDC);
    }
    fxBackdropDC = CreateCompatibleDC(dc);
    fxBackdropBitmap = CreateCompatibleBitmap(dc, width, height);
    fxBackdropOld = SelectObject(fxBackdropDC, fxBackdropBitmap);
    fxBackdropWidth = width;
    fxBackdropHeight = height;
    HDC memory = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(memory, fxAmbient);
    SetStretchBltMode(fxBackdropDC, HALFTONE);
    SetBrushOrgEx(fxBackdropDC, 0, 0, NULL);
    StretchBlt(fxBackdropDC, 0, 0, width, height, memory, 0, 0, BW, BH,
               SRCCOPY);
    SelectObject(memory, old);
    DeleteDC(memory);
  }
  BitBlt(dc, 0, 0, width, height, fxBackdropDC, 0, 0, SRCCOPY);
}
static void fxGlass(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border,
                    BYTE opacity) {
  int saved = SaveDC(dc);
  HRGN clip = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1,
                                 radius, radius);
  ExtSelectClipRgn(dc, clip, RGN_AND);
  DeleteObject(clip);
  HDC layer = CreateCompatibleDC(dc);
  BITMAPINFO bi = {0};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = 1;
  bi.bmiHeader.biHeight = -32;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  DWORD *pixels = NULL;
  HBITMAP strip =
      CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
  HGDIOBJ old = SelectObject(layer, strip);
  COLORREF top = fxMix(fill, RGB(181, 203, 239), .075),
           bottom = fxMix(fill, RGB(6, 9, 17), .14);
  for (int i = 0; i < 32; i++) {
    COLORREF c = fxMix(top, bottom, (double)i / 31);
    if (pixels)
      pixels[i] = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
  }
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, opacity, 0};
  AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, layer, 0, 0,
             1, 32, blend);
  SelectObject(layer, old);
  DeleteObject(strip);
  DeleteDC(layer);
  RestoreDC(dc, saved);
  HPEN pen = CreatePen(PS_SOLID, 1, border);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(pen);
}
static void fxHoverTo(HWND w, HTButtonMotion *motion, double target) {
  motion->from = motion->hover;
  motion->target = target;
  motion->started = GetTickCount64();
  if (!fxMotion || !SetTimer(w, FX_HOVER_TIMER, 10, NULL)) {
    motion->hover = target;
    InvalidateRect(w, NULL, FALSE);
  }
}
static LRESULT CALLBACK fxButtonProc(HWND w, UINT m, WPARAM wp, LPARAM lp,
                                     UINT_PTR id, DWORD_PTR data) {
  HTButtonMotion *motion = (HTButtonMotion *)data;
  (void)id;
  if (m == WM_MOUSEMOVE && !motion->tracking) {
    TRACKMOUSEEVENT track = {sizeof track, TME_LEAVE, w, 0};
    TrackMouseEvent(&track);
    motion->tracking = TRUE;
    fxHoverTo(w, motion, 1);
  } else if (m == WM_MOUSELEAVE) {
    motion->tracking = FALSE;
    fxHoverTo(w, motion, 0);
  } else if (m == WM_LBUTTONDOWN) {
    motion->pressed = TRUE;
    InvalidateRect(w, NULL, FALSE);
  } else if (m == WM_LBUTTONUP || m == WM_CAPTURECHANGED) {
    motion->pressed = FALSE;
    InvalidateRect(w, NULL, FALSE);
  } else if (m == WM_TIMER && wp == FX_HOVER_TIMER) {
    double t = (double)(GetTickCount64() - motion->started) / FX_HOVER_MS;
    motion->hover = motion->from + (motion->target - motion->from) * fxEase(t);
    InvalidateRect(w, NULL, FALSE);
    if (t >= 1 || !fxMotion) {
      motion->hover = motion->target;
      KillTimer(w, FX_HOVER_TIMER);
    }
    return 0;
  } else if (m == WM_NCDESTROY) {
    KillTimer(w, FX_HOVER_TIMER);
    RemoveWindowSubclass(w, fxButtonProc, 1);
    free(motion);
  }
  return DefSubclassProc(w, m, wp, lp);
}
static void fxAttachButton(HWND w) {
  HTButtonMotion *motion = calloc(1, sizeof *motion);
  if (motion && !SetWindowSubclass(w, fxButtonProc, 1, (DWORD_PTR)motion))
    free(motion);
}
static double fxButtonHover(HWND w) {
  DWORD_PTR value = 0;
  return GetWindowSubclass(w, fxButtonProc, 1, &value)
             ? ((HTButtonMotion *)value)->hover
             : 0;
}

typedef struct {
  HWND window;
  RECT rect;
} HTMovingControl;
static HTMovingControl fxControls[96];
static int fxControlCount, fxOffset;
static BOOL fxActive;
static BOOL fxPreview;
/* Three reusable top-down DIBs: outgoing page, incoming page, composed frame.
 * Native controls never move. Only one bitmap is presented during motion. */
typedef struct {
  HDC dc;
  HBITMAP bitmap;
  HGDIOBJ old;
  DWORD *pixels;
} HTSurface;
static HTSurface fxFrom, fxTo, fxFrame;
static int fxWidth, fxHeight, fxLeft;
static BOOL fxCaptured;
static double fxFrameTimes[256], fxFrameCosts[256];
static unsigned fxSamples;
static LARGE_INTEGER fxFrequency, fxEpoch;
static ID2D1Factory *fxFactory;
static ID2D1HwndRenderTarget *fxTarget;
static ID2D1Bitmap *fxFromTexture, *fxToTexture;
static BOOL fxDirect;
static BOOL fxFineTimer;
static double fxProgress;
static HRESULT fxGraphicsResult;
static void fxFreeGraphics(void) {
  if (fxFromTexture)
    ID2D1Bitmap_Release(fxFromTexture);
  if (fxToTexture)
    ID2D1Bitmap_Release(fxToTexture);
  if (fxTarget)
    ID2D1HwndRenderTarget_Release(fxTarget);
  fxFromTexture = NULL;
  fxToTexture = NULL;
  fxTarget = NULL;
  fxDirect = FALSE;
}
static BOOL fxPrepareGraphics(void) {
  if (!fxFactory &&
      FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                               &IID_ID2D1Factory, NULL, (void **)&fxFactory)))
    return FALSE;
  D2D1_SIZE_U size = {(UINT32)(fxWidth - fxLeft), (UINT32)fxHeight};
  if (fxTarget) {
    if (FAILED(ID2D1HwndRenderTarget_Resize(fxTarget, &size)))
      fxFreeGraphics();
  }
  if (!fxTarget) {
    D2D1_RENDER_TARGET_PROPERTIES props = {0};
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.dpiX = props.dpiY = 96;
    /* DWM composes the window at display refresh. Do not block the UI
     * thread waiting for presentation; an elapsed-time timer paces frames. */
    D2D1_HWND_RENDER_TARGET_PROPERTIES window = {
        fxOverlay, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY};
    if (FAILED(ID2D1Factory_CreateHwndRenderTarget(fxFactory, &props, &window,
                                                   &fxTarget)))
      return FALSE;
  }
  if (fxFromTexture)
    ID2D1Bitmap_Release(fxFromTexture);
  if (fxToTexture)
    ID2D1Bitmap_Release(fxToTexture);
  fxFromTexture = NULL;
  fxToTexture = NULL;
  D2D1_SIZE_U imageSize = {(UINT32)fxWidth, (UINT32)fxHeight};
  D2D1_BITMAP_PROPERTIES bitmapProps = {
      {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE}, 96, 96};
  if (FAILED(ID2D1HwndRenderTarget_CreateBitmap(
          fxTarget, imageSize, fxFrom.pixels, fxWidth * 4, &bitmapProps,
          &fxFromTexture)) ||
      FAILED(ID2D1HwndRenderTarget_CreateBitmap(fxTarget, imageSize,
                                                fxTo.pixels, fxWidth * 4,
                                                &bitmapProps, &fxToTexture))) {
    fxFreeGraphics();
    return FALSE;
  }
  return TRUE;
}
static HRESULT fxDrawGraphics(void) {
  float eased = (float)fxEase(fxProgress), offset = S(72) * (1 - eased);
  D2D1_RECT_F source = {(float)fxLeft, 0, (float)fxWidth, (float)fxHeight};
  D2D1_RECT_F dest = {0, 0, (float)(fxWidth - fxLeft), (float)fxHeight};
  ID2D1HwndRenderTarget_BeginDraw(fxTarget);
  ID2D1HwndRenderTarget_DrawBitmap(fxTarget, fxFromTexture, &dest, 1,
                                   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                   &source);
  dest.left += offset;
  dest.right += offset;
  ID2D1HwndRenderTarget_DrawBitmap(fxTarget, fxToTexture, &dest, eased,
                                   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                   &source);
  return ID2D1HwndRenderTarget_EndDraw(fxTarget, NULL, NULL);
}
static double fxNow(void) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  if (!fxFrequency.QuadPart)
    QueryPerformanceFrequency(&fxFrequency);
  return (double)now.QuadPart * 1000.0 / (double)fxFrequency.QuadPart;
}
static void fxFreeSurface(HTSurface *s) {
  if (s->dc) {
    SelectObject(s->dc, s->old);
    DeleteObject(s->bitmap);
    DeleteDC(s->dc);
  }
  memset(s, 0, sizeof *s);
}
static BOOL fxMakeSurface(HTSurface *s, int width, int height) {
  BITMAPINFO bi = {0};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = width;
  bi.bmiHeader.biHeight = -height;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  s->dc = CreateCompatibleDC(NULL);
  s->bitmap = CreateDIBSection(s->dc, &bi, DIB_RGB_COLORS, (void **)&s->pixels,
                               NULL, 0);
  if (!s->dc || !s->bitmap) {
    fxFreeSurface(s);
    return FALSE;
  }
  s->old = SelectObject(s->dc, s->bitmap);
  return TRUE;
}
static BOOL fxPrepareSurfaces(void) {
  RECT r;
  GetClientRect(mainWindow, &r);
  fxLeft = S(237);
  if (fxWidth == r.right && fxHeight == r.bottom && fxFrom.dc && fxTo.dc &&
      fxFrame.dc)
    return TRUE;
  fxFreeSurface(&fxFrom);
  fxFreeSurface(&fxTo);
  fxFreeSurface(&fxFrame);
  fxWidth = r.right;
  fxHeight = r.bottom;
  return fxWidth > fxLeft && fxHeight > 0 &&
         fxMakeSurface(&fxFrom, fxWidth, fxHeight) &&
         fxMakeSurface(&fxTo, fxWidth, fxHeight) &&
         fxMakeSurface(&fxFrame, fxWidth, fxHeight);
}
static void fxCapture(HTSurface *s) {
  paintMain(s->dc);
  /* Print each child in client coordinates. DefWindowProc(WM_PRINT) on a
   * top-level window adds the non-client offset to its children at high DPI. */
  for (HWND child = GetWindow(mainWindow, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    if (child == fxOverlay || !IsWindowVisible(child))
      continue;
    RECT r;
    GetWindowRect(child, &r);
    MapWindowPoints(NULL, mainWindow, (POINT *)&r, 2);
    int saved = SaveDC(s->dc);
    SetViewportOrgEx(s->dc, r.left, r.top, NULL);
    IntersectClipRect(s->dc, 0, 0, r.right - r.left, r.bottom - r.top);
    SendMessage(child, WM_PRINT, (WPARAM)s->dc,
                PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    RestoreDC(s->dc, saved);
  }
  GdiFlush(); /* DIB memory must not be read until GDI has finished writing. */
}
static void fxCompose(double progress) {
  double eased = fxEase(progress);
  unsigned a = (unsigned)(eased * 256), b = 256 - a;
  fxOffset = (int)(S(72) * (1 - eased));
  for (int y = 0; y < fxHeight; y++) {
    DWORD *out = fxFrame.pixels + (size_t)y * fxWidth;
    const DWORD *from = fxFrom.pixels + (size_t)y * fxWidth;
    const DWORD *to = fxTo.pixels + (size_t)y * fxWidth;
    for (int x = fxLeft; x < fxWidth; x++) {
      if (x < fxLeft + fxOffset) {
        out[x] = from[x];
        continue;
      }
      DWORD p = from[x], q = to[x - fxOffset];
      /* Packed red/blue and green interpolation, entirely in memory. */
      out[x] = ((((p & 0xff00ff) * b + (q & 0xff00ff) * a) >> 8) & 0xff00ff) |
               ((((p & 0x00ff00) * b + (q & 0x00ff00) * a) >> 8) & 0x00ff00);
    }
  }
}
static void fxPresent(HDC dc) {
  BitBlt(dc, 0, 0, fxWidth - fxLeft, fxHeight, fxFrame.dc, fxLeft, 0, SRCCOPY);
}
static void fxStopTransition(void) {
  if (fxOverlay)
    KillTimer(fxOverlay, FX_FRAME_TIMER);
  if (fxFineTimer) {
    timeEndPeriod(1);
    fxFineTimer = FALSE;
  }
  if (fxActive) {
    /* Restore only the controls visible on this page, with their original
     * geometry. */
    for (int i = 0; i < fxControlCount; i++)
      SetWindowPos(fxControls[i].window, NULL, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_SHOWWINDOW | SWP_NOREDRAW);
    ShowWindow(fxOverlay, SW_HIDE);
    RECT r = {fxLeft, 0, fxWidth, fxHeight};
    RedrawWindow(mainWindow, &r, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
  }
  fxActive = FALSE;
  fxPreview = FALSE;
  fxControlCount = 0;
  fxOffset = 0;
}
static void fxAdvance(void) {
  ++fxTickCount;
  double start = fxNow();
  double t =
      (start - (double)fxEpoch.QuadPart * 1000.0 / fxFrequency.QuadPart) /
      fxDuration;
  if (t >= 1 || !fxMotion) {
    BOOL reportPreview = fxPreview && t >= 1;
    fxStopTransition();
    if (reportPreview) {
      char note[256];
      double mean = fxSamples > 1
                        ? (fxFrameTimes[fxSamples - 1] - fxFrameTimes[0]) /
                              (fxSamples - 1)
                        : 0;
      snprintf(note, sizeof note,
               "Motion check: %s | %u frames submitted | average %.1f ms "
               "between frames",
               fxDirect ? "Direct2D" : "GDI fallback", fxSamples, mean);
      status(note);
    }
    return;
  }
  fxProgress = t;
  fxOffset = (int)(S(72) * (1 - fxEase(t)));
  if (fxDirect) {
    if (IsIconic(mainWindow)) {
      fxStopTransition();
      return;
    }
    fxGraphicsResult = fxDrawGraphics();
    if (FAILED(fxGraphicsResult)) {
      fxStopTransition();
      fxFreeGraphics();
      return;
    }
  } else {
    fxCompose(t);
    HDC dc = GetDC(fxOverlay);
    fxPresent(dc);
    ReleaseDC(fxOverlay, dc);
    GdiFlush();
  }
  if (fxSamples < 256) {
    fxFrameTimes[fxSamples] = start;
    fxFrameCosts[fxSamples++] = fxNow() - start;
  }
  ++fxPaintCount;
}
static LRESULT CALLBACK fxOverlayProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
  if (m == WM_TIMER && wp == FX_FRAME_TIMER) {
    fxAdvance();
    return 0;
  }
  if (m == WM_ERASEBKGND)
    return 1;
  if (m == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w, &ps);
    if (fxActive) {
      if (fxDirect)
        fxDrawGraphics();
      else
        fxPresent(dc);
    }
    EndPaint(w, &ps);
    return 0;
  }
  return DefWindowProcA(w, m, wp, lp);
}
static BOOL fxBeforeChange(void) {
  fxStopTransition();
  fxCaptured = fxMotion && IsWindowVisible(mainWindow) && fxPrepareSurfaces();
  if (fxCaptured)
    fxCapture(&fxFrom);
  return fxCaptured;
}
static void fxAfterChange(BOOL animate) {
  if (!animate || !fxCaptured)
    return;
  fxCaptured = FALSE;
  fxControlCount = 0;
  for (HWND child = GetWindow(mainWindow, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    if (child == fxOverlay || !IsWindowVisible(child))
      continue;
    RECT r;
    GetWindowRect(child, &r);
    MapWindowPoints(NULL, mainWindow, (POINT *)&r, 2);
    if (r.left < S(237) || fxControlCount >= 96)
      continue;
    fxControls[fxControlCount].window = child;
    fxControls[fxControlCount++].rect = r;
  }
  if (!fxControlCount)
    return;
  if (!fxOverlay) {
    WNDCLASSEXA c = {0};
    c.cbSize = sizeof c;
    c.hInstance = instance;
    c.lpfnWndProc = fxOverlayProc;
    c.lpszClassName = "HTMotionSurface";
    RegisterClassExA(&c);
    fxOverlay = CreateWindowExA(0, c.lpszClassName, "", WS_CHILD, 0, 0, 0, 0,
                                mainWindow, NULL, instance, NULL);
  }
  if (!fxOverlay) {
    fxControlCount = 0;
    return;
  }
  fxCapture(&fxTo);
  for (int i = 0; i < fxControlCount; i++)
    SetWindowPos(fxControls[i].window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_HIDEWINDOW | SWP_NOREDRAW);
  fxActive = TRUE;
  fxDuration = FX_PAGE_MS;
  fxProgress = 0;
  fxDirect = fxPrepareGraphics();
  fxCompose(0);
  SetWindowPos(fxOverlay, HWND_TOP, fxLeft, 0, fxWidth - fxLeft, fxHeight,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  UpdateWindow(fxOverlay);
  RedrawWindow(mainWindow, NULL, NULL,
               RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
  QueryPerformanceFrequency(&fxFrequency);
  QueryPerformanceCounter(&fxEpoch);
  fxSamples = 0;
  fxFineTimer = timeBeginPeriod(1) == TIMERR_NOERROR;
  if (!SetTimer(fxOverlay, FX_FRAME_TIMER, 10, NULL))
    fxStopTransition();
}
static void fxRelease(void) {
  fxStopTransition();
  if (fxAmbient) {
    DeleteObject(fxAmbient);
    fxAmbient = NULL;
  }
  fxFreeSurface(&fxFrom);
  fxFreeSurface(&fxTo);
  fxFreeSurface(&fxFrame);
  fxFreeGraphics();
  if (fxFactory) {
    ID2D1Factory_Release(fxFactory);
    fxFactory = NULL;
  }
  if (fxBackdropDC) {
    SelectObject(fxBackdropDC, fxBackdropOld);
    DeleteObject(fxBackdropBitmap);
    DeleteDC(fxBackdropDC);
    fxBackdropDC = NULL;
  }
}
#endif
