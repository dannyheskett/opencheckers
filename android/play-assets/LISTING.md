# opencheckers — Google Play store listing

Copy/paste these into the Play Console (**Grow → Store presence → Main store
listing**, plus **Store settings** for category). The images in this folder are
the shipped assets. `scripts/play_release.py` pushes the fields below and the
images from this folder, so edit them here rather than in the console.

The screenshots regenerate with `scripts/gen_store_screenshots.mjs`, and the
icon and feature graphic with `scripts/gen_icons.py`.

## Assets (this folder)

| File | Play field | Spec |
|------|-----------|------|
| `icon-512.png` | App icon | 512×512 PNG (32-bit) |
| `feature-graphic-1024x500.png` | Feature graphic | 1024×500 PNG/JPG |
| `screenshots/phone/` | Phone screenshots | 4× 1080×1920 PNG (9:16, promo-eligible) |
| `screenshots/tablet/` | 7-inch and 10-inch tablet screenshots | 4× 2160×3840 PNG (9:16, same files fit both slots) |

## App name (≤30 chars)

```
opencheckers
```

## Short description (≤80 chars)

```
Classic checkers against the computer. Free, open source, no ads, no tracking.
```

## Full description (≤4000 chars)

```
Play classic American checkers against the computer, on a clean board with none of the junk that clutters this genre.

No ads. No tracking. No accounts. No in-app purchases. opencheckers requests zero permissions and never touches the network. It's just the game.

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

BUILT RIGHT
• Fully offline — perfect for flights, commutes, anywhere
• Tiny download, easy on your battery

FREE AND OPEN SOURCE
opencheckers is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opencheckers
```

## Categorization (Store settings)

- **App or game:** Game
- **Category:** Board
- **Tags:** board, strategy, checkers, classic
- **Email:** dan@danheskett.com
- **Website:** https://danheskett.com
- **Content rating:** Everyone (no objectionable content; IARC questionnaire —
  answer "no" to all violence/adult/gambling items)

## Data safety (Policy → App content)

- Data collected: **None**
- Data shared: **None**
- App has no `INTERNET` permission (verify in the manifest) → "no data
  transmitted off the device" is truthful.
- Privacy policy URL: **https://danheskett.com/app/privacy-policy/**

## Screenshots

Pushed via the Play API: `screenshots/phone/` (4x 1080x1920) in the phone slot,
`screenshots/tablet/` (4x 2160x3840) in both the 7-inch and 10-inch tablet
slots.

Captured from the web build's portrait renderer -- the same
src/render_portrait.c that Android runs -- in a headless browser:

    npm i playwright-core
    make web
    node scripts/gen_store_screenshots.mjs --src build/web

That one command also refreshes the App Store set under
`ios/app-store-assets/screenshots/`, so the two listings cannot drift apart.
Regenerate whenever the portrait layout, status band or font changes.
