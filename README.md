# 🧊 Minecraft Mod Manager

A **fast, native Windows GUI** for managing Minecraft mods — built with **C11 + GTK4**.

Scan your mods directory, detect available updates (from **Modrinth** & **CurseForge**), update in one click, roll back to any previous backup, or batch-delete with optional backup retention.

**Zero Java dependency** — just a single ~400 KB executable.

---

## ✨ Features

- **🔍 Smart Scan** — Unzips each `.jar` in memory and reads `fabric.mod.json`, `META-INF/mods.toml`, or `mcmod.info` to extract mod metadata automatically
- **⬆ Version Detection** — Queries Modrinth API (no key needed) and optionally CurseForge API for the latest compatible version, matching by Minecraft version and mod loader
- **⬆ One-Click Update** — Downloads, replaces, and optionally backs up the old version with configurable retention limits
- **↩ Rollback** — Browse all historical backups and restore any previous version instantly
- **🗑 Batch Delete** — Remove mods with or without keeping a backup
- **🎨 Theme System** — Choose between 7 themes: System, Light, Dark, Dark Red-Gold, Forest Green, Ocean Blue, and Deep Purple
- **🔍 Real-time Search & Filter** — Filter mods by name or platform, with keyboard shortcuts for fast workflow
- **⚙ Fully Configurable** — Custom mod/backup directories, CurseForge API key, auto-scan on startup, backup toggle, and max backup count

---

## 🚀 Quick Start Guide

### 1. Build

You'll need **MSYS2 MinGW64** with GTK4 and dependencies:

```bash
# Install dependencies (MSYS2 MinGW64)
pacman -S mingw-w64-x86_64-gtk4 mingw-w64-x86_64-cjson \
          mingw-w64-x86_64-curl-winssl mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja

# Build
cd /path/to/MinecraftModOption
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja

# Run
./MinecraftModOption.exe
```

> **Windows users**: You can run the pre-built `MinecraftModOption.exe` directly, but GTK4 runtime DLLs from MSYS2 must be in your `PATH`.

---

### 2. First Launch — Configure

1. Click **⚙ Settings** on the toolbar
2. Set your **Mod Directory** (e.g. `C:\Users\You\AppData\Roaming\.minecraft\mods`)
3. Set a **Backup Directory** (any folder with free space)
4. Keep **Backup Before Update** enabled — you'll thank yourself later
5. Choose a **🎨 Interface Theme** that suits your taste
6. Click **Save**

---

### 3. Scan Your Mods

Press **`F5`** or click **🔍 Scan**. The tool reads every `.jar` in your directory, extracts metadata, and checks both Modrinth and CurseForge for updates.

The status bar shows your current mod count and how many updates are available:
```
C:\Users\You\...\mods  │  📁 Mods: 47  │  ⬆ Updatable: 12
```

---

### 4. Update Mods

**Selected mods:** Check boxes → click **⬆ Update Selected**
**All updatable:** Click **⬆ Update All**

Each mod gets:
1. ✅ Backed up (if enabled)
2. ✅ Latest `.jar` downloaded
3. ✅ Old file replaced

---

### 5. Rollback (when something breaks)

Select a single mod → click **↩ Rollback** → pick a backup version → done.

---

### 6. Delete Mods

Check boxes → press **`Delete`** or click **🗑 Delete** → choose "Keep backup" or "Delete permanently".

---

## ⌨ Keyboard Shortcuts

| Key | Action |
|---|---|
| `F5` | Scan mod directory |
| `Ctrl+A` | Select all mods |
| `Ctrl+D` | Deselect all |
| `Delete` | Delete selected mods |
| **Double-click** | View full mod details |

---

## 🔑 CurseForge API Key (Optional)

Modrinth works out of the box with **no API key**. For CurseForge support:

1. Get a key from [CurseForge Console](https://console.curseforge.com/)
2. Paste it into **Settings → CurseForge API Key**

---

## 🧰 Tech Stack

| Layer | Technology |
|---|---|
| **Language** | C11 (GNU11) |
| **GUI** | GTK4 |
| **HTTP** | libcurl (WinSSL) |
| **JSON** | cJSON |
| **Build** | CMake + Ninja |

> ⚡ **Build size**: ~400 KB (Release), fully self-contained minus GTK4 runtime.

---

## 📦 Output Directory

All generated files (executable, config, build artifacts) live under `build/`. The config file `config.json` is created automatically on first launch.

---

## 📝 License

MIT — do whatever you want with it.
