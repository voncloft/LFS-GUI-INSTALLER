# LFS Installer

Qt6 desktop GUI for a personal Linux From Scratch installer workflow.

## Current Status

This project is now a working Qt6 GUI prototype with an early script-driven install flow.

What exists now:
- A Qt Widgets application built with CMake
- A 4-page wizard-style flow
- Basic validation for required fields
- Drive discovery using `lsblk`
- Editable planned partition table with manual partition entry
- Mount point and local mount point selection
- Remaining-space calculation against the selected drive
- Generated `partition.sh`, `mount.sh`, `final_setup.sh`, `fstab`, `hostname`, and `clock` artifacts
- JSON config generation and export
- Page 3 script execution with progress tracking and terminal-style log output
- Page 4 VPacman-style package browser layout
- Page 4 GitHub-backed package listing from `Voncloft-OS/REPOS`
- Page 4 automatic version/description loading from each package `spkgbuild`

What does not exist yet:
- No finished end-to-end LFS install backend
- No completed page 4 chroot package-install stage
- No final scratchpkg bootstrap/install flow inside chroot
- No finished bootloader orchestration beyond generated script content
- No full failure-recovery/resume flow

Testing status:
- The project builds successfully
- The GUI still needs manual testing
- The agent did not run the GUI manually

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
- Manual add/remove partition controls
- Mount point and local mount point controls
- Auto-calculated remaining space
- Native Qt partition-style layout

### Page 3: Repo-Driven Install
- Script-driven install page
- Progress bar
- Terminal-style black/white log panel
- Reads `scripts/install.sh`
- Runs listed shell scripts in order
- Tracks progress per completed script
- Parses `echo "step:..."` output for current-step labels
- Writes config checkpoint and desktop setup summary

### Page 4: Additional Features
- VPacman-style package browser layout
- Search bar
- Checkable package list
- Action button row
- Details/output panel
- GitHub-backed package listing from `https://github.com/voncloft/Voncloft-OS/tree/master/REPOS`
- Package rows auto-fill from `REPOS/<category>/<package>/spkgbuild`

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

The current direction is to use the GUI as a front end for an LFS automation flow:
- collect config in the GUI
- prepare the target disk/layout from page 2 selections
- generate/run page 3 scripts from the live environment
- create the base LFS system and make it bootable in stage 3
- use page 4 as the post-bootstrap package stage
- chroot into the finished target system
- bootstrap `scratchpkg`
- install selected packages from the GitHub-backed repo browser

## Main Open Problem

The largest remaining backend hurdle is the stage 4 chroot package flow.

The intended split is now clearer:
- page 3 handles partitioning, mounting, base LFS setup, and bootable-system preparation
- page 4 should assume the target is already mounted and ready
- page 4 should chroot into the target root
- page 4 should fetch/use `scratchpkg`
- page 4 should install the user-selected packages inside that chroot environment
- logging and failure reporting still matter a lot

## Notes

- This repo now contains both GUI work and early generated-script/install-runner behavior
- Page 4 is currently display/selection oriented; package actions are not wired yet
- Manual VM testing is still required before trusting backend behavior
