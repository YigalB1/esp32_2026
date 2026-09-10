#!/usr/bin/env python3
"""
pio_project_launcher.py

A small Tkinter GUI that scans a root folder (e.g. esp32_2026/projects) for
subfolders containing a platformio.ini, and gives each one buttons to:
  - Build       (pio run)
  - Upload      (pio run --target upload)
  - Clean       (pio run --target clean)
  - Monitor     (pio device monitor, opened in a separate terminal window)
  - Open in VS Code (code <path>)

Requirements on PATH:
  - `pio`  (PlatformIO Core CLI)  - https://platformio.org/install/cli
  - `code` (VS Code CLI)          - VS Code > Cmd/Ctrl+Shift+P > "Shell Command: Install 'code' command in PATH"

Run:
    python3 pio_project_launcher.py

First run will ask you to pick the root folder to scan (e.g. your
esp32_2026/projects directory). That choice is remembered in
~/.pio_project_launcher.json for next time. Use the "Change root..."
button to pick a different folder later.
"""

import json
import os
import platform
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, scrolledtext

CONFIG_PATH = Path.home() / ".pio_project_launcher.json"


def find_pio_executable():
    """Locate the PlatformIO CLI executable. Tries system PATH first, then
    falls back to PlatformIO's own default install location - this matters
    because the VS Code PlatformIO extension installs `pio` into its own
    isolated environment (~/.platformio/penv/...) and does not always put
    it on the system PATH, even though it works fine inside VS Code's own
    terminal (which gets the extension's env injected). A plain script
    launched outside VS Code won't see that. Returns a path/command string
    to use as args[0], or None if nothing was found anywhere."""
    on_path = shutil.which("pio")
    if on_path:
        return on_path

    home = Path.home()
    candidates = [
        home / ".platformio" / "penv" / "Scripts" / "pio.exe",  # Windows
        home / ".platformio" / "penv" / "bin" / "pio",          # macOS/Linux
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    return None


PIO_EXECUTABLE = find_pio_executable()


def load_config():
    if CONFIG_PATH.exists():
        try:
            return json.loads(CONFIG_PATH.read_text())
        except Exception:
            return {}
    return {}


def save_config(cfg):
    try:
        CONFIG_PATH.write_text(json.dumps(cfg, indent=2))
    except Exception as e:
        print(f"Warning: could not save config: {e}", file=sys.stderr)


def find_pio_projects(root: Path, max_depth: int = 3):
    """Find every folder under root containing a platformio.ini, up to
    max_depth levels deep. Returns a sorted list of Paths."""
    found = []
    root = root.resolve()
    if not root.exists():
        return found

    def walk(path: Path, depth: int):
        if depth > max_depth:
            return
        try:
            entries = list(path.iterdir())
        except PermissionError:
            return
        if (path / "platformio.ini").exists():
            found.append(path)
            return  # don't descend into a project's own subfolders
        for entry in entries:
            if entry.is_dir() and not entry.name.startswith(".") and entry.name != "node_modules":
                walk(entry, depth + 1)

    walk(root, 0)
    return sorted(found)


def open_terminal_running(command: str, cwd: Path):
    """Open a new terminal window running `command` in `cwd`. Best-effort
    cross-platform; falls back to running in the background if no known
    terminal emulator is found (output then goes to the launcher's own
    console, not a separate window)."""
    system = platform.system()
    if system == "Windows":
        subprocess.Popen(
            ["cmd", "/c", "start", "cmd", "/k", command],
            cwd=str(cwd),
        )
    elif system == "Darwin":
        # AppleScript to open Terminal.app running the command in cwd
        script = (
            f'tell application "Terminal" to do script '
            f'"cd {cwd} && {command}"'
        )
        subprocess.Popen(["osascript", "-e", script])
    else:
        # Linux: try a few common terminal emulators
        candidates = [
            ["x-terminal-emulator", "-e", f"bash -c 'cd \"{cwd}\" && {command}; exec bash'"],
            ["gnome-terminal", "--", "bash", "-c", f'cd "{cwd}" && {command}; exec bash'],
            ["konsole", "-e", "bash", "-c", f'cd "{cwd}" && {command}; exec bash'],
            ["xterm", "-e", f"bash -c 'cd \"{cwd}\" && {command}; exec bash'"],
        ]
        for cmd in candidates:
            try:
                subprocess.Popen(cmd, cwd=str(cwd))
                return
            except FileNotFoundError:
                continue
        # Nothing found - fall back to running in background, no visible window
        subprocess.Popen(command, cwd=str(cwd), shell=True)


# ---------------- Build / upload freshness tracking ----------------
#
# PlatformIO already leaves real, meaningful timestamps on disk (the
# compiled binary's mtime), so freshness is derived from file mtimes
# rather than the GUI keeping its own in-memory state - that way it
# survives restarts and stays correct even if a build/upload was run
# from VS Code instead of this launcher.
#
# Upload has no equivalent artifact from PlatformIO itself, so a small
# marker file is written next to the binary the moment an upload
# succeeds. It lives under .pio/build/<env>/, so "Clean" wiping that
# directory also (correctly) resets the upload status to unknown.

SOURCE_SUBDIRS = ["src", "include", "lib"]
UPLOAD_MARKER_NAME = ".pio_launcher_uploaded_at"

COLOR_STALE = "#fff3b0"  # yellow - needs action
COLOR_OK = "#b6f2b6"     # green - up to date


def _newest_mtime_under(path: Path):
    newest = None
    if not path.exists():
        return newest
    for p in path.rglob("*"):
        if p.is_file():
            m = p.stat().st_mtime
            if newest is None or m > newest:
                newest = m
    return newest


def newest_source_mtime(project_path: Path):
    newest = None
    for sub in SOURCE_SUBDIRS:
        m = _newest_mtime_under(project_path / sub)
        if m is not None and (newest is None or m > newest):
            newest = m
    ini = project_path / "platformio.ini"
    if ini.exists():
        m = ini.stat().st_mtime
        if newest is None or m > newest:
            newest = m
    return newest


def find_newest_firmware_bin(project_path: Path):
    """Returns (bin_path, env_dir) for the most recently built firmware.bin
    across all environments, or (None, None) if nothing has been built."""
    build_root = project_path / ".pio" / "build"
    if not build_root.exists():
        return None, None
    best_bin, best_env_dir, best_mtime = None, None, None
    for env_dir in build_root.iterdir():
        if not env_dir.is_dir():
            continue
        bin_path = env_dir / "firmware.bin"
        if bin_path.exists():
            m = bin_path.stat().st_mtime
            if best_mtime is None or m > best_mtime:
                best_bin, best_env_dir, best_mtime = bin_path, env_dir, m
    return best_bin, best_env_dir


def compute_build_status(project_path: Path):
    """Returns COLOR_STALE or COLOR_OK for the Build button."""
    bin_path, _ = find_newest_firmware_bin(project_path)
    if bin_path is None:
        return COLOR_STALE  # never built
    src_mtime = newest_source_mtime(project_path)
    bin_mtime = bin_path.stat().st_mtime
    if src_mtime is not None and src_mtime > bin_mtime:
        return COLOR_STALE  # edited since last build
    return COLOR_OK


def compute_upload_status(project_path: Path):
    """Returns COLOR_STALE or COLOR_OK for the Upload button."""
    bin_path, env_dir = find_newest_firmware_bin(project_path)
    if bin_path is None:
        return COLOR_STALE  # nothing built, nothing to upload
    marker = env_dir / UPLOAD_MARKER_NAME
    if not marker.exists():
        return COLOR_STALE  # never uploaded
    try:
        uploaded_at = float(marker.read_text().strip())
    except (ValueError, OSError):
        return COLOR_STALE
    if bin_path.stat().st_mtime > uploaded_at:
        return COLOR_STALE  # rebuilt since the last successful upload
    return COLOR_OK


def mark_uploaded(project_path: Path):
    """Called after a successful upload - writes the marker file next to
    whichever firmware.bin was just uploaded, recording the current time."""
    import time
    _, env_dir = find_newest_firmware_bin(project_path)
    if env_dir is None:
        return
    try:
        (env_dir / UPLOAD_MARKER_NAME).write_text(str(time.time()))
    except Exception:
        pass


class ProjectRow(tk.Frame):
    def __init__(self, master, project_path: Path, log_fn, **kwargs):
        super().__init__(master, **kwargs)
        self.project_path = project_path
        self.log_fn = log_fn

        name = project_path.name
        tk.Label(self, text=name, width=28, anchor="w", font=("TkDefaultFont", 10, "bold")).grid(
            row=0, column=0, padx=(4, 8), pady=4, sticky="w"
        )

        self.build_button = tk.Button(self, text="Build", command=self.build, width=14)
        self.build_button.grid(row=0, column=1, padx=2, pady=4)

        self.upload_button = tk.Button(self, text="Upload", command=self.upload, width=14)
        self.upload_button.grid(row=0, column=2, padx=2, pady=4)

        tk.Button(self, text="Clean", command=self.clean, width=14).grid(row=0, column=3, padx=2, pady=4)
        tk.Button(self, text="Monitor", command=self.monitor, width=14).grid(row=0, column=4, padx=2, pady=4)
        tk.Button(self, text="Open in VS Code", command=self.open_vscode, width=14).grid(
            row=0, column=5, padx=2, pady=4
        )

        self.refresh_status()

    def refresh_status(self):
        """Recompute Build/Upload button colors from on-disk file mtimes.
        Safe to call repeatedly (periodic refresh) or after a command finishes."""
        try:
            self.build_button.config(bg=compute_build_status(self.project_path))
            self.upload_button.config(bg=compute_upload_status(self.project_path))
        except Exception:
            pass  # don't let a stray filesystem hiccup break the UI loop

    def _run_async(self, args, label, on_success=None):
        """Run a subprocess in a background thread, streaming output to the log.
        on_success(), if given, runs after a zero exit code, before the
        button colors are refreshed."""
        def worker():
            self.log_fn(f"\n--- {label}: {self.project_path.name} ---\n")
            try:
                proc = subprocess.Popen(
                    args,
                    cwd=str(self.project_path),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    bufsize=1,
                )
                for line in proc.stdout:
                    self.log_fn(line)
                proc.wait()
                self.log_fn(f"--- {label} finished (exit code {proc.returncode}) ---\n")
                if proc.returncode == 0 and on_success:
                    on_success()
            except FileNotFoundError:
                self.log_fn(
                    f"ERROR: command not found ({args[0]}). PlatformIO CLI was not "
                    f"found on PATH or in its default install location "
                    f"(~/.platformio/penv/...). Install it with 'pip install "
                    f"platformio', or make sure the VS Code PlatformIO extension's "
                    f"environment has actually finished installing, then restart "
                    f"this launcher.\n"
                )
            except Exception as e:
                self.log_fn(f"ERROR running {label}: {e}\n")
            finally:
                self.after(0, self.refresh_status)

        threading.Thread(target=worker, daemon=True).start()

    def _pio_args(self, *extra):
        if PIO_EXECUTABLE is None:
            self.log_fn(
                "ERROR: PlatformIO CLI ('pio') was not found on PATH or in its "
                "default install location. Install it with 'pip install "
                "platformio', or check that ~/.platformio/penv exists, then "
                "restart this launcher.\n"
            )
            return None
        return [PIO_EXECUTABLE, *extra]

    def build(self):
        args = self._pio_args("run")
        if args:
            self._run_async(args, "Build")

    def upload(self):
        args = self._pio_args("run", "--target", "upload")
        if args:
            self._run_async(args, "Upload", on_success=lambda: mark_uploaded(self.project_path))

    def clean(self):
        args = self._pio_args("run", "--target", "clean")
        if args:
            self._run_async(args, "Clean")

    def monitor(self):
        # Monitor is interactive/long-running - give it its own terminal
        # window rather than the shared log pane.
        if PIO_EXECUTABLE is None:
            self.log_fn(
                "ERROR: PlatformIO CLI ('pio') was not found on PATH or in its "
                "default install location. Install it with 'pip install "
                "platformio', or check that ~/.platformio/penv exists, then "
                "restart this launcher.\n"
            )
            return
        self.log_fn(f"\n--- Monitor: {self.project_path.name} (opened in new terminal) ---\n")
        open_terminal_running(f'"{PIO_EXECUTABLE}" device monitor', self.project_path)

    def open_vscode(self):
        try:
            subprocess.Popen(["code", str(self.project_path)])
            self.log_fn(f"\nOpened {self.project_path.name} in VS Code.\n")
        except FileNotFoundError:
            self.log_fn(
                "ERROR: 'code' command not found. In VS Code, run "
                "Cmd/Ctrl+Shift+P > 'Shell Command: Install code command in PATH'.\n"
            )


class LauncherApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PlatformIO Project Launcher")
        self.geometry("820x560")

        self.config_data = load_config()
        self.root_dir = None
        self.project_rows = []
        if self.config_data.get("root_dir"):
            candidate = Path(self.config_data["root_dir"])
            if candidate.exists():
                self.root_dir = candidate

        self._build_ui()

        if PIO_EXECUTABLE is None:
            self.log(
                "WARNING: PlatformIO CLI ('pio') was not found on PATH or in "
                "its default install location (~/.platformio/penv/...). "
                "Build/Upload/Clean/Monitor will fail until this is resolved. "
                "Install it with 'pip install platformio', or verify the "
                "VS Code PlatformIO extension finished installing, then "
                "restart this launcher.\n"
            )
        else:
            self.log(f"Using PlatformIO CLI: {PIO_EXECUTABLE}\n")

        if self.root_dir:
            self.refresh_projects()
        else:
            self.after(100, self.choose_root_dir)

        self.after(3000, self._periodic_status_refresh)

    def _build_ui(self):
        top = tk.Frame(self)
        top.pack(fill="x", padx=8, pady=8)

        self.root_label = tk.Label(top, text="Root: (none selected)", anchor="w")
        self.root_label.pack(side="left", fill="x", expand=True)

        tk.Button(top, text="Change root...", command=self.choose_root_dir).pack(side="right", padx=4)
        tk.Button(top, text="Refresh", command=self.refresh_projects).pack(side="right", padx=4)
        tk.Button(top, text="Exit", command=self.destroy).pack(side="right", padx=4)

        self.projects_frame = tk.Frame(self)
        self.projects_frame.pack(fill="x", padx=8, pady=4)

        tk.Label(self, text="Output log:", anchor="w").pack(fill="x", padx=8)
        self.log_widget = scrolledtext.ScrolledText(self, height=18, state="disabled", font=("Courier", 10))
        self.log_widget.pack(fill="both", expand=True, padx=8, pady=(0, 8))

    def log(self, text):
        def append():
            self.log_widget.configure(state="normal")
            self.log_widget.insert("end", text)
            self.log_widget.see("end")
            self.log_widget.configure(state="disabled")
        # Safe to call from worker threads - schedule on the Tk main loop
        self.after(0, append)

    def choose_root_dir(self):
        chosen = filedialog.askdirectory(title="Select the folder containing your PlatformIO projects")
        if chosen:
            self.root_dir = Path(chosen)
            self.config_data["root_dir"] = str(self.root_dir)
            save_config(self.config_data)
            self.refresh_projects()

    def refresh_projects(self):
        for widget in self.projects_frame.winfo_children():
            widget.destroy()

        if not self.root_dir:
            self.root_label.config(text="Root: (none selected)")
            return

        self.root_label.config(text=f"Root: {self.root_dir}")
        projects = find_pio_projects(self.root_dir)

        if not projects:
            tk.Label(
                self.projects_frame,
                text="No platformio.ini found under this root (searched up to 3 levels deep).",
                fg="gray",
            ).pack(anchor="w", padx=4, pady=4)
            self.project_rows = []
            return

        self.project_rows = []
        for project_path in projects:
            row = ProjectRow(self.projects_frame, project_path, self.log)
            row.pack(fill="x", pady=2)
            self.project_rows.append(row)

        self.log(f"Found {len(projects)} project(s) under {self.root_dir}\n")

    def _periodic_status_refresh(self):
        for row in self.project_rows:
            row.refresh_status()
        self.after(3000, self._periodic_status_refresh)


def main():
    app = LauncherApp()
    app.mainloop()


if __name__ == "__main__":
    main()
