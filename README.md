# VulkanRays

VulkanRays is a compact educational 3D renderer implemented with Vulkan. It demonstrates a small but complete Vulkan application pipeline: instance & device creation, swapchain and depth setup, pipeline creation, per-object buffers and descriptors, command buffer recording, synchronization, ImGui integration, and a simple camera and scene made of procedural geometry (grid + pyramids).

Table of contents
- Quick summary
- Features (what you added)
- Implementation details (where and how)
- Build & run (typical)
- Controls
- Troubleshooting

Quick summary
-------------
VulkanRays is a learning/demonstration renderer that shows how to wire Vulkan resources into a running app with:
- Modular Vulkan initialization (instance, debug messenger, surface)
- Physical device selection and logical device creation
- Swapchain + depth buffer management and recreation on resize
- Graphics pipelines (distinct pipelines for triangles and lines)
- Per-object uniform buffers (MVP) and descriptor sets
- Procedural geometry (grid and pyramids) with vertex/index buffers
- Command buffer recording and per-frame synchronization (multi-frame-in-flight)
- ImGui overlay rendered via Vulkan
- Basic first-person camera (WASD + mouse)

Features (analysis and explanation)
----------------------------------

1) Instance / Validation / Debugging
- Where: VulkanInstance.cpp / VulkanInstance.h
- What: The engine creates a Vulkan instance and, when enabled, sets up the standard validation layer (`VK_LAYER_KHRONOS_validation`) and a debug utils messenger. This surfaces API errors and warnings to stderr during development (see `debugCallback`).
- Why it matters: Validation layers catch many common Vulkan mistakes early and make the learning process manageable.

2) Device selection and memory utilities
- Where: VulkanDevice.cpp / VulkanDevice.h
- What: The engine enumerates physical devices and picks one that supports both graphics and present on the surface. It stores graphics/present queue family indices and creates the logical device with the necessary queue(s). The helper `findMemoryType` looks up a suitable memory type for allocations.
- Why it matters: Choosing compatible queue families and correct memory types is required for correct buffer/image allocation and queue submission.

3) Swapchain, depth buffer and recreation on resize
- Where: CoreRendering.cpp (helpers), VulkanApp.cpp (`recreateSwapchain`, `createFramebuffers`, `createRenderPass`)
- What: Swapchain creation logic selects surface format and present mode, creates image views and framebuffers, and constructs a depth image with an appropriately supported depth format (via `FindSupportedDepthFormat` and `CreateDepthResources`). When the window resizes or the swapchain becomes out-of-date, `recreateSwapchain` reinitializes these resources safely (waiting on device idle and cleaning old resources).
- Why it matters: Proper swapchain recreation and depth buffer handling are essential for a correct and robust renderer that can handle window changes.

4) Depth buffering
- Where: CoreRendering.cpp (`FindSupportedDepthFormat`, `CreateDepthResources`, `DestroyDepthResources`)
- What: The engine queries candidate depth formats and creates a depth image + memory + view suitable for use as a depth/stencil attachment. The depth attachment is cleared each frame in the render pass.
- Why it matters: Depth testing prevents triangles/lines from drawing incorrectly in 3D and is required for expected 3D rendering order.

5) Per-frame synchronization & multi-frame-in-flight
- Where: CoreRendering.cpp (`CreateSyncObjects` / `DestroySyncObjects`), VulkanApp.cpp (usage)
- What: The application uses a `MAX_FRAMES_IN_FLIGHT` constant (set to 3), and for each in-flight frame it allocates semaphores for image available / render finished and a fence to block CPU reuse of frame resources until the GPU finishes. The main loop waits for the frame's fence, acquires an image with `vkAcquireNextImageKHR` using the per-frame image-available semaphore, submits the command buffer and signals the render-finished semaphore, then presents.
- Why it matters: Multi-frame-in-flight increases GPU utilization and reduces idle time while keeping resource lifetimes predictable via fences.

6) Descriptor & per-object uniform buffers (per-object MVP)
- Where: VulkanApp.cpp (`createDescriptorSetLayout`, `createDescriptorSet`), RenderObject.* (mvpBuffer members)
- What: The engine creates a descriptor set layout with a single uniform buffer binding (binding 0). For each render object a dedicated `VulkanBuffer` is created to hold its Model-View-Projection matrix (sizeof(Mat4)) and a descriptor set is allocated from the UBO pool and updated to point to that buffer. During command buffer recording, the per-object MVP buffer is updated by calling `uploadData` before issuing the draw call.
- Why it matters: Per-object UBOs allow each mesh to have an independent transform and keep shader binding simple (one descriptor set per object). Uploading per-object MVPs right before draw keeps CPU/GPU coherency simple for a small demo.

7) Procedural geometry: Grid and Pyramid objects
- Where: RenderObject.cpp / RenderObject.h
- What:
  - PyramidObject: creates an indexed triangle mesh with 5 vertices forming a pyramidal shape and an index buffer for faces.
  - GridObject: procedurally generates a line-grid (as line segments) to act as a ground plane / reference.
  - Both create VkBuffers via the `VulkanBuffer` helper and upload vertex/index data to them. Each object implements `recordDraw` to bind vertex/index buffers and descriptor sets then issue `vkCmdDrawIndexed`.
- Why it matters: Demonstrates buffer creation, indexing, drawing both triangle and line topologies, and shows how to add new objects.

8) Two pipeline variants: triangles and lines
- Where: VulkanPipeline.cpp / VulkanPipeline.h
- What: `VulkanPipeline` encapsulates creation of a graphics pipeline. It accepts an enum `Topology` to choose between triangle list and line list topologies. Separate pipelines are created for normal triangle drawing (pyramids) and line drawing (grid). Vertex layout includes position and color (two vec3 attributes).
- Why it matters: Using different pipelines for different primitive topologies and states is the idiomatic Vulkan approach and demonstrates pipeline creation, vertex input, rasterization, depth testing and shader module creation.

9) Shader embedding / shader modules
- Where: VulkanPipeline.cpp includes `shaders/triangle.vert.inc` and `shaders/triangle.frag.inc`
- What: The code expects SPIR-V to be embedded as C arrays (the `.inc` files). Shader modules are created with those arrays rather than reading .spv at runtime. This implies a compile step or build step that produces `.inc` SPIR-V headers (or the repo contains them).
- Why it matters: Embedding SPIR-V simplifies distribution and avoids runtime file loading; it illustrates how to instantiate shader modules from memory.

10) Command buffers, per-frame recording and render pass
- Where: VulkanApp.cpp (`createCommandBuffers`, `recordCommandBuffer`)
- What: The app allocates one primary command buffer per framebuffer image and records render pass begin/clear, iterates all render objects, uploads per-object UBOs, binds the appropriate pipeline and issues draw calls. ImGui draw data is also recorded (via `ImGui_ImplVulkan_RenderDrawData`).
- Why it matters: Recording per-frame command buffers is the core of Vulkan rendering. Doing all draws and ImGui in one submission simplifies synchronization and ordering.

11) ImGui integration (Vulkan backend)
- Where: VulkanApp.cpp (`initImGui`, `shutdownImGui`, `recordImGui`)
- What: Initializes ImGui with SDL2+Vulkan backends, creates a large descriptor pool for ImGui, configures `ImGui_ImplVulkan_InitInfo` (instance, device, queues, image count, render pass), and renders a simple FPS overlay window each frame using ImGui.
- Why it matters: ImGui provides an immediate-mode overlay to display runtime info and tweak parameters. The integration demonstrates how to bootstrap ImGui with a Vulkan renderpass.

12) Input & camera
- Where: VulkanApp.cpp (`handleEvents`, `mainLoop`, `recordCommandBuffer` for camera math)
- What:
  - Input: SDL events drive key state (W/A/S/D), mouse capture on left-click (relative mouse), ESC releases capture, and mouse motion changes yaw/pitch. ImGui input capture is respected (events passed to ImGui and camera ignores input if ImGui wants it).
  - Camera math: camera forward/right vectors are computed from yaw/pitch. `MathUtils.cpp` contains `perspective`, `lookAt`, rotation helpers and `mat4_mul`. The per-frame view/projection matrices are constructed and used to build the MVP uploaded to each object's UBO.
- Why it matters: Demonstrates a simple FPS-style camera with smooth mouse look and movement, integrating input and UI.

13) Resource lifecycle & cleanup
- Where: VulkanApp.cpp destructor & `cleanupVulkanResources`, CoreRendering destroy helpers
- What: The app waits for device idle, frees and destroys all created Vulkan objects (framebuffers, command buffers, command pools, sync objects, descriptor pools, depth resources, render pass). Pipelines and swapchain objects are destroyed and recreated when necessary.
- Why it matters: Correct resource cleanup prevents leaks and makes swapchain recreation reliable.

Implementation details (where to look / key functions)
-------------------------------------------------------
- App entry and lifecycle: VulkanApp.cpp
  - run(), mainLoop(), handleEvents(), recreateSwapchain(), recordCommandBuffer()
- Vulkan initialization: VulkanInstance.cpp / VulkanDevice.cpp
  - instance creation, debug messenger, surface creation, device pick & logical device creation, memory type lookup
- Swapchain & depth: CoreRendering.cpp + VulkanApp.cpp swapchain/recreate flows
  - FindSupportedDepthFormat, CreateDepthResources, RecreateSwapchain
- Pipeline creation: VulkanPipeline.cpp
  - Vertex input formats, pipeline state, shader module creation using embedded SPIR-V
- Objects & geometry: RenderObject.cpp / RenderObject.h
  - PyramidObject, GridObject, getModelMatrix() and recordDraw()
- Math: MathUtils.cpp / MathUtils.h
  - perspective(), lookAt(), rotations, mat4_mul
- UI: ImGui integration in VulkanApp.cpp (`initImGui`, `recordImGui`, `shutdownImGui`)

Build & run (typical)
---------------------
Dependencies:
- Vulkan SDK (LunarG) with glslang/glslc if you need to recompile shaders
- SDL2 (with SDL2 Vulkan support)
- ImGui sources and Vulkan/SDL2 backends (project already references these headers)
- CMake and a C++17 compiler

Typical commands (from project root):
1. Configure:
   mkdir build && cd build
   cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
2. Build:
   cmake --build . --config Release
3. Run:
   ./VulkanRays   (or the produced binary under build/bin/)

Note: The repository uses embedded SPIR-V includes (e.g., shaders/*.inc). If the project expects `.inc` generated headers, make sure your build flow produces those from GLSL/HLSL SPIR-V binaries or the repo already contains them.

Controls
--------
- WASD: move forward / left / back / right
- Mouse (when captured): look around (relative motion)
- Left mouse button: capture mouse for camera control (if ImGui doesn't want the mouse)
- ESC: release mouse capture
- Window resize: triggers swapchain recreation
- FPS overlay: displayed via ImGui in the top-left (drawn each frame)

Troubleshooting
---------------
- Validation layer errors or runtime crashes:
  - Ensure the Vulkan SDK is installed and validation layers are available; enable validation in instance creation if debugging.
- Swapchain issues / VK_ERROR_OUT_OF_DATE_KHR:
  - The app already handles out-of-date/swapchain-suboptimal; ensure your window system supports Vulkan surface extensions (SDL2 Vulkan).
- Missing shader modules:
  - If the app tries to include `shaders/*.inc` and those files are missing, generate the SPIR-V and the `.inc` wrappers or modify the pipeline creation to load `.spv` files at runtime.
- No Vulkan devices found:
  - Update GPU drivers and ensure the system Vulkan ICD is installed.

Notes and suggestions for reading the code
------------------------------------------
- Start in VulkanApp.cpp to understand app flow, then follow into VulkanInstance/VulkanDevice for init and into VulkanPipeline for the pipeline state. The rendering loop, commands, ImGui integration and object draw calls are all in VulkanApp.cpp and RenderObject.* respectively.
- CoreRendering.cpp provides helpful modular functions used across the app (depth resources, descriptor pool helpers, sync object helpers, swapchain recreation logic).

If you want a brief walk-through of a particular file or function (for example, a detailed line-by-line of `CreateDepthResources` or `recordCommandBuffer`), tell me which one and I will explain it step-by-step.
