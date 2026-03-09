# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Jungle Week 1 Team 2 — a 2D physics ball simulation and pinball game built with Direct3D 11 and ImGui. Written in C++ as a Visual Studio 2022 (v143 toolset) project targeting Windows 10+.

## Build

- Open `Jungle_Week1_Team2.sln` in Visual Studio
- Build configuration: **Debug|x64** (the x64 Debug config uses `SubSystem: Windows`; other configs use `Console`)
- The HLSL shader (`ShaderW0.hlsl`) is excluded from the MSVC build pipeline — it is compiled at runtime via `D3DCompileFromFile`, so it must be present in the working directory alongside the executable
- Linked libraries (via `#pragma comment`): `user32`, `d3d11`, `d3dcompiler`

## Architecture

Everything lives in a single compilation unit (`main.cpp`) with one header (`Sphere.h`):

- **`FVertexSimple`** — Vertex format: position (xyz) + color (rgba)
- **`FVector`** — Minimal 3-component vector (no operators, math done inline)
- **`URenderer`** — Owns all D3D11 state: device, swap chain, frame buffer, rasterizer, shaders, constant buffer. Handles shader compilation from `ShaderW0.hlsl` at startup. Provides `UpdateConstant()` to pass per-object Offset/Scale/Angle/ChargeSign to shaders.
- **`UPrimitive`** — Abstract base with virtual `Update`, `Render`, `CheckCollision`, `ResolveCollision`
- **`UBall : UPrimitive`** — Ball entity with physics. All balls share a single vertex buffer (`SharedVertexBuffer`); per-ball transform is applied via the constant buffer in the vertex shader. Physics features controlled by static flags:
  - `bUseAngularVelocity` — enables rotational physics with friction
  - `bUseMagnetism` — enables Coulomb-law magnetic attraction/repulsion (O(n^2) in main loop)
  - `Restitution` — coefficient of restitution for wall and ball-ball collisions
- **`PinballGame`** — Pinball mode with angled side walls and two flippers (left/right). Uses 4 separate vertex buffers for wall/wing geometry. Flipper controls: Z/Left Arrow (left), M/Right Arrow (right). When active, gravity is forced on and balls are destroyed when they fall below the bottom wall.
- **`Sphere.h`** — Pre-generated unit sphere vertex data (large array of `FVertexSimple`)

## Shader Pipeline

`ShaderW0.hlsl` contains both vertex and pixel shaders:
- **Vertex shader (`mainVS`)**: Applies Z-axis rotation by `Angle`, then scales by `Scale`, then offsets by `Offset`
- **Pixel shader (`mainPS`)**: Tints color based on `ChargeSign` (red for N-pole +1, blue for S-pole -1, unchanged for 0)

The constant buffer layout (`FConstants`) must stay 16-byte aligned: `{float3 Offset, float Scale, float Angle, float ChargeSign, float[2] Pad}`.

## Main Loop Order

1. Input / message pump
2. ImGui frame (property window with all physics toggles)
3. Ball count adjustment (add/remove to match `desiredBallCount`)
4. Pinball dead ball removal
5. Flipper update
6. Gravity application
7. Magnetic force application (O(n^2) pairwise)
8. Physics update per ball (movement + wall collision)
9. Ball-ball collision detection and resolution (O(n^2))
10. Pinball collision (walls + flippers)
11. Velocity clamping
12. Render (pinball geometry, then all balls, then ImGui)
13. VSync present + FPS cap at 60

## Key Conventions

- Memory management uses raw `new[]`/`delete[]` for the `PrimitiveList` array — resized by copying on every add/remove
- All physics math is done with raw floats, no math library
- Wall boundaries are the NDC box [-1, 1] on both axes
- Korean comments throughout the codebase
- `imgui/` directory contains vendored ImGui source (not to be modified)
