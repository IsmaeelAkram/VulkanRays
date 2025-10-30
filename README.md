# VulkanRays

A tiny 3D renderer I wrote to learn Vulkan the “real” way—no helper frameworks, just SDL2 + Vulkan + ImGui. It draws a procedural scene (grid + pyramids), has a first-person camera, and shows the full life cycle: instance/device, swapchain + depth, pipelines, per-object UBOs, command buffers, and synchronization.

I built this as a student project to demystify Vulkan by wiring everything myself and documenting the choices (and mistakes) along the way.

---

## Quick summary

* Minimal but complete Vulkan app
* Two pipelines (triangles for meshes, lines for the grid)
* Per-object UBOs (MVP) using descriptor sets
* Per-frame command buffers + fences/semaphores (triple-buffered)
* ImGui overlay for FPS and toggles
* WASD + mouselook camera
* Clean teardown and safe swapchain recreation on resize

Tested on **Windows 10 (RTX)** and **Arch Linux (Wayland/Hyprland, Mesa/Vulkan 1.3)**.

---

## Why I built it

I kept bouncing between “big engine” codebases and toy samples that hid the hard parts. VulkanRays sits in the middle: small enough to read in an evening, but explicit enough to show what actually happens each frame. If you’re learning Vulkan, this is the level of detail I wish I’d found sooner.

---

## Features I implemented (and why)

1. **Validation + debug messenger**
   Turn on `VK_LAYER_KHRONOS_validation` and print messages to stderr. This saved me from a *lot* of foot-guns (wrong image layouts, missing barriers, etc.).

2. **Device + queues selection**
   Picks a device that supports graphics + present and caches the family indices. Also includes a tiny `findMemoryType` helper because memory flags are easy to mess up.

3. **Swapchain + depth with safe recreation**
   Chooses formats/present mode, creates image views, a depth image/view, and tears it all down/rebuilds on `VK_ERROR_OUT_OF_DATE_KHR`. Resize is a first-class citizen.

4. **Two graphics pipelines**
   One pipeline for triangles (pyramids) and one for lines (grid). Different primitive topologies, same render pass. Keeps state obvious instead of shoving toggles everywhere.

5. **Per-object UBOs via descriptor sets**
   Each renderable gets its own UBO (MVP). I update right before issuing the draw. It’s simple and keeps the shaders clean while I’m learning.

6. **Procedural geometry**

   * **Grid**: generated line segments on the fly.
   * **Pyramid**: tiny indexed triangle mesh.
     Both exercise vertex/index buffers and show how to add your own objects.

7. **Command buffer recording per frame**
   Allocate one primary CB per swapchain image, begin render pass, draw objects, then hand over to ImGui’s Vulkan backend, and submit with per-frame sync objects.

8. **ImGui overlay (SDL2 + Vulkan backends)**
   Handy for FPS, toggles, and quick params without writing a UI system.

9. **Deterministic cleanup**
   Everything created is destroyed. I wait on device idle in the right places so swapchain recreation doesn’t implode.

---

## What the code looks like

```
/src
  VulkanApp.*          // app loop, input, recordCommandBuffer, swapchain recreate
  VulkanInstance.*     // instance + debug messenger + surface
  VulkanDevice.*       // physical device pick, logical device, queues, memory helper
  VulkanPipeline.*     // pipeline creation (triangles | lines), shader modules
  RenderObject.*       // GridObject, PyramidObject, per-object UBO, recordDraw()
  CoreRendering.*      // depth resources, descriptor pool, sync objects
  MathUtils.*          // perspective(), lookAt(), rotations, mat4 ops
/shaders
  triangle.vert/frag   // compiled to SPIR-V and embedded as .inc
```

---

## Build & run

**Dependencies**

* Vulkan SDK (LunarG). Make sure validation layers are installed.
* SDL2 with Vulkan support.
* ImGui sources + SDL2/Vulkan backends (included/linked by the project).
* CMake + a C++17 compiler.
* (Optional) `glslc` if you recompile shaders.

**Typical**

```bash
# From project root
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
./VulkanRays   # or build/bin/VulkanRays on your toolchain
```

**Shaders**

* I embed SPIR-V as `.inc` arrays. If you edit GLSL, run your compile step (e.g. `glslc triangle.vert -o triangle.vert.spv`) and rebuild the `.inc` headers (simple Python/CMake step in the project).
* Alternatively, swap the pipeline loader to read `.spv` files at runtime.

---

## Controls

* **W/A/S/D** — move
* **Mouse** — look (capture with LMB, release with **Esc**)
* **Resize window** — triggers swapchain recreation
* **ImGui** — FPS overlay (top-left)

---

## Implementation notes (where to look)

* **App flow**: `VulkanApp::run`, `mainLoop`, `recordCommandBuffer`
* **Init**: `VulkanInstance.*`, `VulkanDevice.*`
* **Swapchain/depth**: `CoreRendering.*` + `VulkanApp::recreateSwapchain`
* **Pipelines**: `VulkanPipeline.*` (topology enum)
* **Objects**: `RenderObject.*` (`recordDraw`, per-object UBO)
* **Math**: `MathUtils.*` (`perspective`, `lookAt`, rotations)

---

## Troubleshooting (stuff I broke so you don’t have to)

* **Validation spam / crashes**
  Make sure the Vulkan SDK is installed and the validation layer is available. If not, disable validation or fix your SDK install.

* **`VK_ERROR_OUT_OF_DATE_KHR` on present**
  That’s normal after a resize or alt-tab. The app recreates the swapchain. If you changed windowing code, double-check you’re waiting on the device in the right spots.

* **“Missing shaders” / includes not found**
  If `.inc` files aren’t generated, compile GLSL → SPIR-V and rebuild the `.inc`. Or switch to loading `.spv` at runtime.

* **No Vulkan device**
  Update GPU drivers and confirm a working ICD (e.g., `vulkaninfo` on Linux).

---

## Design choices (and alternatives I considered)

* **Per-object UBOs**: simple and explicit for a small scene. For bigger scenes I’d move to a ring buffer or dynamic UBOs/SSBOs.
* **Two pipelines vs. toggling topology**: separate pipelines keep state immutable and closer to Vulkan’s design.
* **Embedded SPIR-V**: fewer runtime dependencies. If hot-reloading shaders is your thing, switch to `.spv` file IO.

---

## Known issues / TODO

* No descriptor indexing or bindless; one set per object is fine at this scale.
* No frustum culling or multi-threaded recording (future experiment).
* SSAO/lighting would be fun next; this is unlit color for clarity.

---

## FAQ

**Why Vulkan instead of OpenGL?**
I wanted to understand modern explicit APIs: synchronization, memory, and pipeline state. Once those clicked, everything else felt less magical.

**Why SDL2?**
It gives me windows + input + Vulkan surfaces without dragging in a full engine.

**Can I add my own mesh?**
Yes—copy `PyramidObject` as a template: create vertex/index buffers, add a per-object UBO, and implement `recordDraw()`.

---

## Acknowledgments

* Khronos Vulkan tutorial/docs, Sascha Willems samples, and countless validation messages that humbled me.
* ImGui for saving me from writing a UI.
