# LFS Installer

Qt6 desktop GUI for a personal Linux From Scratch installer workflow.

## Current Status

This project is currently a GUI-only prototype.

What exists now:
- A Qt Widgets application built with CMake
- A 4-page wizard-style flow
- Basic validation for required fields
- Drive discovery using `lsblk`
- Editable planned partition table
- JSON config generation and export
- A GUI-only `Install` checkpoint on page 3

What does not exist yet:
- No real install backend
- No partitioning or formatting
- No chroot logic
- No package build/install logic
- No bootloader setup
- No post-install configuration execution

Testing status:
- The project builds successfully
- The GUI still needs manual testing
- No installer actions have been tested by the agent

## Pages

### Page 1: System Details
- PC name
- Username
- Password
- Time zone

### Page 2: Storage Planner
- Target drive selector
- Detected disks/partitions view
- Editable partition plan
- Add/remove partition controls
- Auto layout helper

### Page 3: Repo-Driven Install
- Repo URL
- Branch
- Script path
- Working directory
- Generated config preview
- Install log panel

Important:
In the current build, pressing `Install` does not run an installer.
It only validates inputs, creates a timestamped run directory, and writes `install-config.json`.
After that, the button changes back to `Next` so page 4 can be opened.

### Page 4: Additional Features
- Checkbox-based feature selection
- Summary preview
- Export config JSON

## Build

```bash
cmake -S . -B build
cmake --build build
```

Binary:

```text
build/lfs_installer
```

## Current Files

- `CMakeLists.txt`
- `src/main.cpp`
- `src/installerwindow.h`
- `src/installerwindow.cpp`
- `STEPS TAKEN.txt`
- `TODO.txt`

## Planned Direction

The current idea in `TODO.txt` is to use the GUI as a front end for an LFS automation repo:
- collect config in the GUI
- prepare the target disk/layout
- perform pre-chroot setup from the live environment
- chroot into the target system
- continue the LFS steps inside chroot
- install extra features afterward

## Main Open Problem

The largest backend hurdle is chroot orchestration.

This is not a blocker to the overall concept. It is possible, but it needs to be designed carefully:
- pre-chroot steps and in-chroot steps should be split cleanly
- the installer should generate data once and pass that data into both phases
- the chroot phase should usually be entered once for a scripted sequence, not manually step-by-step from the GUI
- mounting `/dev`, `/proc`, `/sys`, and `/run` correctly is part of that handoff
- logging and failure recovery will matter a lot

## Notes

- This repo currently represents the GUI shell only
- Backend install behavior was intentionally removed for now
- Manual testing in a VM is still required before backend work should be trusted
