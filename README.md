# opencheckers

Checkers (American / English draughts) written in C with Raylib: classic,
familiar controls, rules, layout, and visuals. Play the computer at three
difficulty levels.

The window is freely **resizable**, but the **board is a fixed size** with a
minimum window just big enough to play (the openklondike model).

## Building

### Linux/WSL2

```bash
./scripts/build_raylib_linux.sh
make
make run
```

### Release Build

```bash
make release
make run-release
```

### Windows Cross-Compile

Requires mingw-w64:

```bash
./scripts/build_raylib_windows.sh
make windows
```

Produces `build/opencheckers-x64.exe` and `build/opencheckers-x86.exe`.

### macOS

```bash
./scripts/build_raylib_mac.sh
make mac  # -> build/opencheckers-mac (universal arm64 + x86_64)
```

## Tests

```bash
make test
```

## Rules

American / English draughts on an 8×8 board, 12 pieces each:

- Men move and capture one square diagonally **forward**; kings move one square
  in any diagonal direction.
- **Captures are mandatory.** A multi-jump must continue with the same piece
  until it can jump no more.
- Reaching the far row crowns the piece a king and **ends the turn**.
- You lose when you have no pieces, or no legal move.

## Playing

- **Click** one of your pieces to select it; its legal destinations are dotted.
- **Click** a dotted square to move. During a multi-jump the piece stays
  selected until the chain ends.
- Choose **Easy / Medium / Hard** and which colour you play from the menu.

### Keys

- **Escape**: menu (your game is kept and can be resumed)
- **Alt+Enter**: toggle fullscreen

## Recording

Toggle **Record: On/Off** from the menu to capture your session to an H.264 MP4
(`opencheckers-YYYYMMDD-HHMMSS.mp4`). No external tools required. The capture is
supersampled so the video is as crisp as the live game.

## License

MIT. See [LICENSE](LICENSE).
