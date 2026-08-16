# personal-webpage-in-C

A personal site written in C. No HTML markup, no CSS — the whole page is laid
out in [Clay](https://github.com/nicbarker/clay), compiled to WebAssembly, and
drawn by Clay's HTML renderer.

Built on top of Clay's [`clay-official-website`](https://github.com/nicbarker/clay/tree/main/examples/clay-official-website)
example, which supplies `clay.h`, the WASM build and the renderer in
`index.html`. The layout in `main.c` was replaced with this site; the
scaffolding is upstream's.

## Build

```sh
bash build.sh          # needs clang with a wasm32 target
```

Everything lands in `build/`. It needs to be served over HTTP — opening
`index.html` from disk won't load the `.wasm`:

```sh
cd build && python3 -m http.server 8000
```

## Layout

| | |
|---|---|
| `main.c` | The entire site — content, layout, theming, interaction |
| `clay.h` | Clay, vendored unchanged |
| `index.html` | Clay's HTML/canvas renderer + the WASM loader |
| `build.sh` | clang → `build/clay/index.wasm` (~111kb) |
| `fonts/`, `images/` | IBM Plex Mono; everything the page serves |
| `assets-src/` | Full-res masters and SVG sources. Not shipped |

## Design

Two themes from the Lamplight palette — **Paper** (default) and **Dusk** —
toggled from the header. Colours are semantic tokens in a `Theme` struct, so
every reference goes through `T->`:

| Role | Dusk | Paper |
|---|---|---|
| Ground | `#0B2740` | `#FBF7EF` |
| Text | `#FBF3DE` | `#0B2740` |
| Secondary | `#ACD0D0` | `#2B566D` |
| Selected tab | `#F3D28A` | `#7A5217` |
| Interaction | `#DFA230` | `#B07A2E` |
| Link hover | `#C89D92` | `#8C3F47` |

Projects, Experience and Interests are tabs — only the selected one renders.

Projects can filter by language, but `FILTERS_SHOWN` currently caps the row
at `All`; every project still carries a `.lang`, so raising it to `LANG_COUNT`
brings the chips back.

Thumbnails take their height from each image's own aspect ratio, so nothing is
cropped or letterboxed. Diagrams in `images/` are generated from the SVGs in
`assets-src/` via `rsvg-convert`.

## Notes

- `index.html` scales every font size by `0.8`, so font sizes in `main.c` are
  design pixels ÷ 0.8. Every other measurement is design pixels.
- `build.sh` deletes `build/clay/{fonts,images}` before copying. `cp -r` merges
  without removing, so assets deleted from `images/` used to keep shipping.
  **Re-run the build after changing any asset** — the served copy is a copy.

Three local divergences from upstream Clay's renderer, all in `index.html`:

- **`letterSpacing`** is measured but never drawn upstream; it is applied here in
  the measure path and both renderers.
- **`object-fit: cover`** on images, so a source of a different shape crops
  rather than stretches.
- **Link navigation.** Upstream sets `window.location` on mousedown *and* leaves
  the `<a href>` to fire, navigating twice; bot-protected hosts reject the
  aborted-then-repeated request. The anchor now handles it alone, and external
  links get `target="_blank" rel="noopener noreferrer"`.

## Credits

- [Clay](https://github.com/nicbarker/clay) by Nic Barker — zlib/libpng licence
- [IBM Plex Mono](https://github.com/IBM/plex) — SIL Open Font License 1.1
