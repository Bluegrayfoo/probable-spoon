# Trideroah Learn

A small macOS learning app written in C. It opens a native AppKit window and displays the Trideroah glyph assets from `assets/Assets.xcassets`.

Includes letters practice, words practice, stats, and a 15-question Daily Practice mode that unlocks harder problem types as progress is saved.

## Build

```sh
./build.sh
```

Or run the compiler directly:

```sh
clang -std=c11 -Wall -Wextra -pedantic trideroah.c -o trideroah -framework AppKit -lobjc
```

## Run

```sh
./trideroah learn
```

## GitHub Layout

Commit the files like this:

```text
trideroah.c
build.sh
README.md
assets/
  Assets.xcassets/
    a.imageset/
      a.heic
      Contents.json
    b.imageset/
      b.heic
      Contents.json
    ...
```

The app searches for assets in this order:

1. `TRIDEROAH_ASSETS`, if set
2. `./Assets.xcassets`
3. `./assets/Assets.xcassets`
4. `Assets.xcassets` beside the executable
5. `assets/Assets.xcassets` beside the executable
6. Tristan's original local development path, as a fallback

If someone downloads the repo from GitHub, they should be able to build and run it as long as `assets/Assets.xcassets` is included.
