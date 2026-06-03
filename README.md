# CS2 Internal Overlay

An internal D3D11 overlay for Counter-Strike 2 implementing ESP, Aimbot, Triggerbot, RCS, and a Bomb Timer.

## Features
- **ESP**: 2D Box ESP, Health bars, and Names.
- **Aimbot**: Customizable bone targeting and smoothing.
- **Triggerbot**: Delay and cooldown checks for precise firing.
- **RCS**: Recoil control system mapped to held weapon patterns.
- **Bomb Timer**: Countdown display when C4 is planted.

## Getting Started

### Prerequisites
- Windows 10/11
- Visual Studio with C++ Desktop Development tools
- A DLL Injector (e.g. Process Hacker, Cheat Engine, etc.)

### Updating Offsets
When Counter-Strike 2 updates, you will need to regenerate game offsets:
1. Make sure Counter-Strike 2 is running.
2. Run `cs2-dumper.exe` to dump the latest offsets.
3. This creates or updates headers inside the `output/` folder.
4. Open the solution in Visual Studio and rebuild.

### Building
Open the solution (`CS2.slnx`) in Visual Studio or build via CLI using MSBuild:
```powershell
MSBuild.exe /p:Configuration=Release /p:Platform=x64
```
The output DLL will be generated in `x64/Release/CS2.dll` (or relative path). Inject this DLL into the `cs2.exe` process.

## Keybinds
- **INSERT**: Toggle Overlay UI
- **END**: Safe eject/detach cheat from game
