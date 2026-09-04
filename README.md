[<img src="kurva.ico" alt="kurva-bober face logo" height="120">](https://github.com/ixpl0/kurva-switcher/releases)

# kurva-switcher

`kurva-switcher` is a **utility for switching keyboard layouts**, positioned as an alternative to Punto Switcher.

Typed `ghbdtn` instead of `привет`? Select it, press `Pause`, and it becomes `привет`. It works the other way round too.

Currently, the application only works on Windows. You can [download](https://github.com/ixpl0/kurva-switcher/releases) it if you trust the author. Alternatively, you can build it yourself using Visual Studio (see [Building](#building)). AI can assist you in this process, just as it helped the author during development in the unfamiliar language C++.

Originally, the author developed it for personal use. This was due to a lack of trust in the few existing alternatives to Punto Switcher.

## Functionality

- **Hotkeys**  
  Select text and press `Pause` or `Shift + Pause` to retype it in the other layout (customization planned).

- **Layout switching**  
  After a conversion the keyboard layout of the window switches to the language the text now belongs to, so you can just keep typing. Can be turned off in the tray menu.

- **Clipboard-friendly**  
  Whatever you had on the clipboard - text, images, files, formatted content - is still there after a conversion, and the converted text does not show up in the clipboard history (`Win + V`).

- **Tray menu**  
  Right-click the frog in the notification area to toggle layout switching, enable *Run at Windows startup*, or exit. The menu follows the light or dark app mode of Windows. Autostart records the full path of the executable you are running (per-user `Run` registry key, no administrator rights needed); if you later move or replace the file, the entry is repointed the next time you start the new copy.

- **Open source**  
  Build the application from source without relying on external binaries.

- **Offline operation**  
  The application doesn't use the network.

- **Languages**  
  Toggle between English and Russian.

## How it works

Punto-style utilities have to get the selected text out of another application and put the converted text back. Most of them press `Ctrl + C`, wait a few milliseconds, press `Ctrl + V`, wait a few milliseconds more, and hope for the best. When the application is slower than the wait, the conversion does nothing - or pastes the *previous* clipboard content. `kurva-switcher` does not guess:

1. **Reading the selection.** The selected text is read through [UI Automation](https://learn.microsoft.com/windows/win32/winauto/entry-uiauto-win32) whenever the focused control exposes it (Win32, WPF, UWP, Office, Chromium and Firefox based apps, Windows Terminal, Qt...). No `Ctrl + C` is involved, and if the control says nothing is selected, nothing happens at all.
2. **Fallback to `Ctrl + C`.** If the control does not expose its selection, the clipboard is backed up (all formats), emptied, and `Ctrl + C` is sent. The application's copy is detected through the clipboard *sequence number*, not through a timer, so it works no matter how slow the application is - and even if the selected text is exactly what was on the clipboard before.
3. **Pasting.** The converted text is put on the clipboard with *delayed rendering*: Windows asks `kurva-switcher` for the data at the very moment the target application reads the clipboard for `Ctrl + V`. Only after that read (plus a short quiet period) is the original clipboard restored. If nobody reads it, the original is restored after two seconds.
4. **Playing nice.** If some other program replaces the clipboard while all this happens, `kurva-switcher` does not overwrite it. Modifier keys held down for the hotkey are released before `Ctrl + C` / `Ctrl + V` is sent, so the application never sees `Ctrl + Shift + V`.

Each word of the selection is converted in the direction of its majority of characters, so mixed text such as `hello ghbdtn` becomes `руддщ привет`. Punctuation that sits on different keys in the two layouts (`,` / `б`, `?` / `,`, `@` / `"`...) is converted along with the letters.

### Troubleshooting

`kurva-switcher` writes a short log of every conversion to the debugger output. Run [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) from Sysinternals to see it: which application had focus, whether UI Automation or `Ctrl + C` was used, when the target read the clipboard, and so on. The log never contains the text itself.

Known limitations:

- Windows does not let a normal program send keystrokes to a program running **as administrator**. Run `kurva-switcher` as administrator too if you need conversions in such windows.
- In applications that neither expose their selection nor use `Ctrl + C` for copying (some terminals, games), `Pause` sends a `Ctrl + C` that the application interprets its own way.
- Applications that read the clipboard with more than a two second delay after `Ctrl + V` get the original clipboard content back.

## Building

Open `kurva-switcher.sln` in Visual Studio 2022 (Desktop development with C++ workload) and build the `Release | x64` configuration. The executable lands in `x64\Release\kurva-switcher.exe`; it is portable and needs no installation.

Every push is also built by [GitHub Actions](.github/workflows/build.yml); the workflow runs the unit tests for the conversion logic (`tests\LayoutConverterTest.cpp`) and publishes the executable as a build artifact.

## Future plans

- **New languages**  
  Support for additional languages will be added in future releases.

- **macOS support**  
  Upcoming releases will include support for macOS.

- **Installer**  
  Currently, only the portable version is available; autostart can be enabled from the tray menu.

> **Important Notice:**  
> The author is an experienced front-end developer and not a C++ expert. That's why contributions are highly appreciated! 🐸
