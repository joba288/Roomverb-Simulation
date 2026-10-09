# Roomverb Simulation

Application for simulating room acoustics and applying the resulting reverb to multiple audio files. 
Uses ray-based sound propagation to estimate stereo impulse responses, then FFT-convolves mulitple source audio files with those responses to produce a stereo WAV output.

The application is written in **C**, with a 3D interface for arranging sound sources, and positioning a listener.

## Usage

1.  Launch Roomverb.
2.  Select an existing sound source or use **+** to add another source
    and choose a WAV file.
3.  Position the sources and listener in the 3D scene using the
    selection and transform controls.
4.  Click **Render**.
5.  Choose a destination and filename in the save dialog.
6.  Wait for raycasting, convolution, and mixing to complete. The
    application writes the resulting WAV file to the selected location.

### Controls
```
  Control             Action
  ------------------- ----------------------------------------------
  Left mouse button   Select an object
  `G`                 Translation gizmo
  `R`                 Rotation gizmo when the listener is selected
  `S`                 Scale gizmo
  `+`                 Add a sound source
  Browse button       Select a source audio file
  Render button       Process sources and save the mixed WAV
```
Camera navigation uses an implementation of the Blender-style camera in raylib:
https://github.com/grplyler/raylib-blender-camera

## Technologies
```
  Component                     Technology
  ----------------------------- -----------------------------
  Language                      C
  3D graphics and windowing     raylib
  GUI                           raygui
  Transform gizmos              raygizmo
  Camera controls               raylib-blender-camera
  Audio decoding and encoding   FFmpeg libraries
  Parallel processing           OpenMP
  Native file dialogs           Native File Dialog (NFD)
```
## Project structure

``` text
main.c             Application lifecycle, input handling and render orchestration
roomverb.c / .h     Room, listener and sound-source setup
raytracer.c / .h    Ray propagation, intersections and impulse recording
audio.c / .h        Audio import, impulse conversion, convolution, mixing and export
ui.c / .h           GUI, scene drawing and selection
```

## Building
### Windows
Clone the repository:
```
git clone https://github.com/joba288/Roomverb-Simulation.git
cd Roomverb-Simulation
```
Create a build directory:
```
mkdir build
cd build
```
Configure the project:
```
cmake ..
```
Build:
```
cmake --build .
```

## Planned improvements

- Improve control over attributes affecting the simulation, like room size, wall absorption etc
- Improve user-interface
- Improvements to the acoustics simulation, to get closer to a more realistic final product.
