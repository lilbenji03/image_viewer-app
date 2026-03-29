# Image Viewer v2.0

A lightweight Windows image viewer written in C with no heavy dependencies.

## Supported Formats
- JPEG (.jpg, .jpeg)
- PNG (.png)
- BMP (.bmp) — 16-bit and 24-bit
- GIF (.gif) — first frame
- TIFF (.tiff, .tif)
- TGA (.tga)
- PPM / PGM (.ppm, .pgm)

## Features
- Toolbar with icon buttons
- Zoom (mouse wheel, +/- keys) toward cursor
- Pan (click and drag)
- Fit to window / 100% reset
- Rotate clockwise / counter-clockwise
- Flip horizontal / vertical
- Slideshow (auto-advance every 3 seconds)
- Fullscreen mode (F11)
- Image info panel (filename, format, dimensions, bit depth)
- Thumbnail sidebar (browse folder images)
- Recent files menu
- Drag & drop to open
- Previous/Next image in folder (← →)
- Double-buffered rendering (no flicker)

## Keyboard Shortcuts
| Key        | Action                  |
|------------|-------------------------|
| Ctrl+O     | Open file               |
| ← / →      | Previous / Next image   |
| F          | Fit to window           |
| R or 0     | Reset zoom to 100%      |
| + / -      | Zoom in / out           |
| S          | Toggle slideshow        |
| I          | Toggle image info       |
| T          | Toggle thumbnail panel  |
| F11        | Toggle fullscreen       |
| Escape     | Exit fullscreen / close |
| Scroll     | Zoom in/out             |
| Drag       | Pan image               |

## Project Structure
```
image_viewer/
├── include/
│   └── viewer.h          # Shared types, constants, declarations
├── src/
│   ├── stb_image.h        # ← YOU MUST DOWNLOAD THIS
│   ├── stb_image_resize2.h # ← YOU MUST DOWNLOAD THIS
│   ├── stb_image_impl.c   # Compiles stb_image once
│   ├── image_io.c         # Load any image format
│   ├── transform.c        # Rotate & flip
│   ├── folder.c           # Folder scanning & navigation
│   ├── thumbs.c           # Thumbnail sidebar
│   ├── recent.c           # Recent files list
│   ├── ui.c               # UI actions (open, zoom, fullscreen...)
│   ├── draw.c             # Rendering (toolbar, image, info)
│   └── main.c             # WinMain & window procedure
└── build.bat              # Build script
```

## Setup & Build

### Step 1 — Download stb headers (one-time)
Download these two files and place them in the `src/` folder:

- https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
- https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h

### Step 2 — Build
Open Command Prompt in the project folder and run:
```cmd
build.bat
```
Or manually:
```cmd
gcc src\stb_image_impl.c src\image_io.c src\transform.c src\folder.c src\thumbs.c src\recent.c src\ui.c src\draw.c src\main.c -I include -I src -o image_viewer.exe -lgdi32 -lcomdlg32 -lshell32 -lshlwapi -lmsimg32 -mwindows -O2
```

### Step 3 — Install as default viewer (optional)
1. Right-click any image file
2. Open with → Choose another app
3. Browse → select `image_viewer.exe`
4. Check "Always use this app" ✓
