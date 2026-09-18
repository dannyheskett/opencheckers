# opencheckers — App Store listing

Copy/paste into App Store Connect. Mirrors `android/play-assets/LISTING.md`, with
the differences Apple requires (subtitle, keywords, promotional text).
`scripts/asc_release.py listing` pushes the fields below and the screenshots,
so edit them here rather than in the console.

One rule that differs from Play:

- **Never mention Android, Google Play, or another platform** in the description.
  Apple rejects listings that reference competing stores.

## New App form (My Apps -> + -> New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `opencheckers` (must be unique App Store-wide; see fallbacks below) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.opencheckers` |
| SKU | `opencheckers` |
| User Access | Full Access |

If `opencheckers` is taken, in order of preference: `opencheckers Checkers`,
`opencheckers Draughts`, `opencheckers Game`. The name is public, capped at 30
characters, and can be changed with any later version — the SKU and bundle ID
cannot.

## Subtitle (<=30 chars)

```
Classic American checkers
```

## Promotional text (<=170 chars)

Editable anytime without submitting a new build — use it for release notes or
seasonal copy.

```
No ads, no tracking, no accounts. Just classic checkers against the computer, free and open source.
```

## Keywords (<=100 chars, comma-separated, no spaces after commas)

Do not repeat the app name — it is already indexed.

```
checkers,draughts,board game,strategy,classic,offline,kings,jump,king me,brain
```

## Description (<=4000 chars)

```
Play classic American checkers against the computer, on a clean board with none of the junk that clutters this genre.

No ads. No tracking. No accounts. No in-app purchases. opencheckers never touches the network. It's just the game. You can even play it in airplane-mode.

CLASSIC RULES
• 8x8 board, 12 pieces each
• Captures are mandatory, and a multi-jump keeps going until the piece can jump no more
• Reach the far row to crown a king
• Win by taking every piece or leaving your opponent with no legal move

PLAY YOUR WAY
• Three difficulty levels: Easy, Medium, and Hard
• Play as Red or Black
• Tap a piece to see its legal moves, then tap where it should go
• The last move is highlighted, so you can always see what the computer just did
• Two-finger tap for the menu; your game is kept and can be resumed

FREE AND OPEN SOURCE
opencheckers is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opencheckers
```

## App Review notes

Sent to Apple's reviewer with every submission that has none yet
(`scripts/asc_release.py` sets them, with the team's review contact).

```
Thank you very much for reviewing my game. opencheckers is single-player checkers against the computer. It needs no account, sign-in or network access; tap a piece, then tap a highlighted square to move. A two-finger tap opens the menu, where Options sets the difficulty and your colour.
```

## App information

- **Category (primary):** Games -> Board
- **Category (secondary):** Games -> Strategy
- **Content Rights:** does not contain third-party content
- **Age Rating:** answer "None" to every question -> **4+**
- **Copyright:** `2026 Daniel Heskett`
- **Support URL:** https://danheskett.com
- **Marketing URL:** https://danheskett.com/projects/opencheckers/
- **Privacy Policy URL:** https://danheskett.com/app/privacy-policy/

## App Privacy (App Store Connect -> App Privacy)

Answer **"No, we do not collect data from this app."** — accurate and verified:
no network code, no analytics SDK, no permissions requested. This yields a
"Data Not Collected" privacy label. It has no API, so it is set once by hand.

## Pricing

Free. No in-app purchases. Requires the **Free Applications agreement** to be
Active under Business / Agreements, Tax, and Banking.

## Export compliance

opencheckers uses no encryption of any kind. `ITSAppUsesNonExemptEncryption =
false` in `ios/Info.plist` stops App Store Connect asking the encryption question
on every upload.
