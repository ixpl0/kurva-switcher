# CLAUDE.md

Notes for Claude Code sessions working on this repository. The author reads
this file too; keep it short and keep it current.

## Project

`kurva-switcher` is a Windows-only Win32 tray utility that retypes selected
text in the other keyboard layout (C++20, MSVC, Visual Studio 2022 solution
`kurva-switcher.sln`). README.md explains how it works. The code lives in
`src/` and `include/`, the conversion-logic tests in `tests/`.

## Working agreement with the author

- **Branch.** Standing instruction from the author: work on `main` and push
  to `main`, unless the author asks for something else in the conversation.
  Do not open pull requests or create extra branches on your own initiative,
  even when the session was started on a `claude/...` branch.
- **Language.** The author writes in Russian; answer in Russian. Code,
  comments, commit messages, README and this file stay in English.
- **Building and testing are done by the author, by hand.** Your sandbox is
  Linux and cannot build or run the Windows executable. Do not try to
  cross-compile, install MinGW or Wine, or emulate Windows in any other way.
  After a task the author opens the solution in Visual Studio, builds
  `Release | x64` and tries `x64\Release\kurva-switcher.exe`. Do not poll or
  wait for the GitHub Actions build either; the author sees its result on
  GitHub.
- **What you can verify here.** The pure conversion logic has a
  self-contained test that compiles with g++, which is available in the
  sandbox (`out/` is gitignored):

  ```sh
  mkdir -p out && g++ -std=c++20 -Iinclude tests/LayoutConverterTest.cpp src/LayoutConverter.cpp -o out/LayoutConverterTest && out/LayoutConverterTest
  ```

  Run it whenever you touch `LayoutConverter.*`. Everything else (clipboard,
  UI Automation, hotkeys, tray, registry) can only be checked on Windows, so
  say plainly what you could not verify instead of claiming it works.
- **Keep the Visual Studio project in sync.** New, renamed or removed
  `.cpp`/`.h` files must be edited into `kurva-switcher.vcxproj` and
  `kurva-switcher.vcxproj.filters` by hand: there is no glob, and a missing
  entry only shows up when the author builds. Keep
  `tests/LayoutConverterTest.cpp` free of Windows-only headers so it keeps
  compiling with both `cl` and `g++`.

## Finishing a task ("switch the Linux off")

The Linux machine you work in is a temporary Claude Code cloud VM. The author
does not want it lingering after the work is done. The platform reclaims the
VM once the session goes idle, so "switching it off" means ending the task
cleanly, with nothing left behind and nothing keeping the session busy. A
task is finished only when all of this holds:

1. Every change is committed on `main` with a descriptive message and
   pushed. Nothing may remain uncommitted, untracked or unpushed; it would
   disappear together with the VM.
2. Nothing you started is still running: no background processes or servers,
   no `/loop`, no scheduled wake-ups or reminders (`send_later`, routines,
   triggers), no pull-request subscriptions, no artifact watches. Never
   create any of these unless the author explicitly asks for them.
3. You are not waiting for anything: not for CI, not for a review, not for a
   reply. Do not schedule yourself a "check back later".
4. Your final message says what changed, what could not be verified on
   Linux, and exactly what the author should try by hand in Visual Studio and
   in the running exe.
5. Then end the turn. Do not keep the session busy with extra exploration,
   refactoring or verification the author did not ask for.

Leave archiving the session to the author (sidebar at claude.ai/code): an
archived session can no longer take follow-up messages.
