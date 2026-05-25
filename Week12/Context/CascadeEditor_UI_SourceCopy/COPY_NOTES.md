# Cascade Editor UI Source Copy

Original source root:

`Engine/Plugins/FX/Cascade/Source/Cascade`

This folder contains a focused copy of the Cascade Editor UI-related source files found in this Unreal Engine tree.

## Confirmed UI Files

- `Private/Cascade.cpp`: editor toolkit frame, tabs, layout, menus, toolbar extenders, and command bindings.
- `Private/Cascade.h`: `FCascade` toolkit declarations.
- `Private/CascadeActions.cpp`
- `Private/CascadeActions.h`: UI command declarations and registration.
- `Private/SCascadeEmitterCanvas.cpp`
- `Private/SCascadeEmitterCanvas.h`: emitter panel Slate viewport widget.
- `Private/CascadeEmitterCanvasClient.cpp`
- `Private/CascadeEmitterCanvasClient.h`: emitter/module panel drawing and interaction client.
- `Private/CascadeEmitterHitProxies.cpp`
- `Private/CascadeEmitterHitProxies.h`: hit proxies used by the emitter panel.
- `Private/SCascadePreviewViewport.cpp`
- `Private/SCascadePreviewViewport.h`: preview viewport Slate widget.
- `Private/SCascadePreviewToolbar.cpp`
- `Private/SCascadePreviewToolbar.h`: preview viewport toolbar and menus.
- `Private/CascadePreviewViewportClient.cpp`
- `Private/CascadePreviewViewportClient.h`: preview viewport client.
- `Private/AssetTypeActions_ParticleSystem.cpp`
- `Private/AssetTypeActions_ParticleSystem.h`: opens `UParticleSystem` assets in Cascade.
- `Private/CascadeModule.cpp`: module entry point that creates Cascade toolkits.
- `Public/CascadeModule.h`
- `Public/ICascade.h`
- `Cascade.Build.cs`

## Files From The Requested List Not Present In This Tree

- `Private/CascadeMenus.cpp`
- `Private/CascadeToolbar.cpp`
- `Private/CascadeEmitterEdDrawHelper.cpp`
- `Public/Cascade.h`
- `Public/CascadeActions.h`

In this UE5 source tree, menu and toolbar code is folded into `Private/Cascade.cpp` and `Private/SCascadePreviewToolbar.cpp`, while emitter/module panel drawing lives primarily in `Private/CascadeEmitterCanvasClient.cpp`.
