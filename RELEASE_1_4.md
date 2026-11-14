# GymTrack 1.4

Project branding is now GymTrack throughout the app, assets, source references, build tasks and executables. The original console main file has only its displayed project name changed; its logic and the other original modules remain unchanged.

Page transitions now render two cached page images through Direct2D. Live Windows controls stay at their final coordinates and are restored after the 420 ms transition. This removes per-frame control movement, full-window painting and glass-effect reconstruction. A GDI rendering fallback remains available if Direct2D cannot initialize. The ambient background is cached at the current window size. A temporary high-resolution timer request is released when motion ends or is interrupted.

The capture alignment was corrected for window borders and display scaling. Preview runs a 1.2-second transition and reports actual submitted frames and mean interval in the status bar. Its measurements describe application frame submissions, not a guarantee of monitor refresh or performance on every computer.

Final verification on 14 November 2026:

- 151 core regression checks passed.
- 129 native GUI checks passed, including invalid-date rejection, persistence, frame pacing on all five pages at 1823 x 952 and 1920 x 974 client sizes, interruption, resize, reduced motion and resource cleanup.
- The final automated page trials submitted 24-27 frames per 420 ms, averaging 15.47-17.40 ms between submissions. The worst observed interval was 31.99 ms. Page preparation is separate from that measurement and took 158-381 ms in those trials.
- The running release executable was inspected through desktop automation. A visible intermediate animation frame and the restored layout were observed. Its Preview reported 71 submitted frames with a 16.8 ms mean interval.
- Calendar validation still rejects 2026-02-29 and accepts 2024-02-29. Original C modules, record/session ID behavior and HT1 persistence remain unchanged.

The detailed current GUI log is in `verification/gui-test-results.txt`. Earlier release guides are omitted from this package. Direct2D and the timer APIs are supplied by Windows; their GCC link libraries are included in the build scripts.

Windows rendering reference: https://learn.microsoft.com/en-us/windows/win32/api/d2d1/ne-d2d1-d2d1_present_options
