# Pizlix

Pizlix is Linux From Scratch 12.2 with some added components, where userland is compiled with Fil-C. This means you get the most memory safe Linux-like OS currently available.

Caveats:

- The kernel is compiled with Yolo-C. So that you can compile the kernel, a copy of GCC is installed in `/yolo/bin/gcc`.

- The C compiler is compiled with Yolo-C++. Some more information about that:

    - It's likely that a production memory safe OS would still let you run unsafe programs in cases where the security/performance trade-off was warranted. The compiler might be a good example of that.

    - I haven't yet ported LLVM to Fil-C++, and so long as I haven't, the compiler will have to be Yolo-C++.

    - The compiler is called `/usr/bin/clang-20` but there are many symlinks to it (`gcc`, `g++`, `cc`, `c++`, `clang`, and `clang++` all point at `clang-20`).

    - All of the other building-related tools (like `ld`, `make`, `ninja`, etc) are compiled with Fil-C (or Fil-C++).

## Supported Systems

Pizlix has been tested inside VMware and Hyper-V on X86_64.

I have confirmed that it's possible to build Pizlix on Ubuntu 24.

## Installing Pizlix

Pizlix requires you to set up your machine thusly:

- You must have a `/mnt/lfs` partition mounted at /dev/sda4. If you have it mounted somewhere else, then make sure you edit the various scripts in this directory (and its subdirectories).

- You must have a swap partition at `/dev/sda3`. If you have one somewhere else (or don't have one), then make sure you edit the various scripts in this directory (and its subdirectories).

- You must have an `lfs` user as described in sections 4.3 and 4.4 of the [LFS book](LFS-12.2-SYSV-BOOK.pdf).

Once you have satisfied those requirements, **and you're happy with the contents of `/mnt/lfs` being annihilated**, just do:

    sudo ./build.sh

From this directory. Then, edit your grub config to include the `menuentry` in `etc/grub_custom` and reboot into Pizlix!

If you run into trouble, see the [Build Stages](#stages).

## Using Pizlix

Pizlix boots to a plain text login: it uses sysvinit with gettys on tty1 through tty6, and there is no graphical login manager. Log in as a non-root user (like `pizlo`, password `pizlo`).

Pizlix by default has the following configuration:

- `sshd` is running. It's a memory safe OpenSSH daemon!

- `seatd` is running.

- There is a `root` user with password `root`.

- There is a user called `pizlo` with password `pizlo`. This user is a sudoer.

- `dhcpcd` is set to connect you via DHCP on `eth0`, which is assumed to be ethernet, not wifi.

Please change the passwords, or better yet, replace the `pizlo` user with some other user, if your Pizlix install will face the network!

Once you get your internet to work (it will "just work" if `eth0` is DHCP capable), you'll need to run:

    make-ca -g

As `root`. Without this, `curl` and `wget` will have problems with HTTPS.

To see what this thing is really capable of, do:

    weston

And enjoy a totally memory safe GUI! `weston` is the simple Wayland compositor. It talks to seatd directly and coexists with the GNOME session (run it on one VT, GNOME on another).

To run the full GNOME desktop, do:

    pizlix-gnome

This starts the full GNOME Shell session: the mutter Wayland compositor with the GNOME shell on top — top bar, Activities overview, app grid, notifications. A weston-terminal is autostarted; launch GNOME apps from there or from the Activities search, e.g. `gnome-text-editor`.

See <a href="#gnome-services">GNOME services and how they're wired up</a> below for what the session needs from the boot scripts, PAM, and D-Bus.

<a name="gnome-services"></a>
## GNOME services and how they're wired up

Everything below is installed by `build_postlc7_chroot_setup_scripts_for_gnome.sh` (plus the postlc3 weston setup script) and was verified live.

### Boot services

The setup script copies the bootscripts and creates the rc symlinks (S20dbus/S25elogind in rc2.d through rc5.d, matching K links in rc0.d/rc1.d/rc6.d). Boot order in practice:

- `S20dbus` starts the D-Bus system message bus (`dbus-daemon --system`, daemonizes, pid in `/run/dbus/pid`). Everything else wants the bus up first.
- `S25elogind` starts elogind (`/usr/libexec/elogind --daemon`), which claims `org.freedesktop.login1` on the system bus and provides the logind API (sessions and seats).
- The rest follows: `S10sysklogd`, `S20network` (dhcpcd on eth0), `S30sshd`, `S80seatd` (used by weston).

The same setup script also fixes two prerequisites: it appends `USE_PAM yes` to `/etc/login.defs` (shadow's login is built with Linux-PAM in this stage), and it changes the `mountvirtfs` bootscript to create `/run/user` with mode 755 instead of 1777 (elogind refuses every session on a world-writable `/run/user`).

### Login

`/bin/login` is built with Linux-PAM, and `/etc/pam.d/login` loads `pam_elogind.so`, so every console login registers a session with elogind. After logging in on a getty, `loginctl list-sessions` shows the session as active on `seat0`. mutter/gnome-shell uses that elogind session to take control of the seat and become DRM master of `/dev/dri/card0`.

Note: shadow's login prints `configuration error - unknown item 'USE_PAM'` before the password prompt. That's cosmetic — the `USE_PAM` key only exists in `login.defs` tables for shadow builds *without* PAM; PAM itself runs fine (the session registers, `/run/user/$UID` gets created).

### Starting GNOME

`pizlix-gnome` execs `dbus-run-session -- gnome-session --session=pizlix-gnome`. The session definition (`/usr/share/gnome-session/sessions/pizlix-gnome.session`) requires exactly one component, `org.gnome.Shell`:

- gnome-session resolves it by desktop-file id to `/etc/xdg/autostart/org.gnome.Shell.desktop` (`X-GNOME-Autostart-Phase=DisplayServer`), which runs `/usr/bin/pizlix-shell`. That wrapper starts `gnome-shell --wayland`, waits for the Wayland socket, registers with `org.gnome.SessionManager.RegisterClient` using its `DESKTOP_AUTOSTART_ID` (GNOME 46 dropped the shell's own registration code and we run without systemd), and stays in the foreground for as long as the shell lives.
- Once the compositor is up, the remaining autostart entries launch: `pizlix-terminal.desktop` autostarts a weston-terminal.

### Compositor environment

The wrappers (`pizlix-gnome`, `pizlix-shell`) export what the session needs:

- `GBM_ALWAYS_SOFTWARE=true`: the only GPU is QEMU's display-only bochs-drm (no render node), so gbm/EGL must take the kms_swrast (llvmpipe) path.
- `NIR_DEBUG=novalidate`: disables Mesa's NIR shader validator, which bit-tags a pointer that Fil-C provenance tracks. Known limitation, see the postlc7 notes.
- `GI_TYPELIB_PATH`/`LD_LIBRARY_PATH` pointing at `/usr/lib/gnome-shell` and `/usr/lib/mutter-14` (the shell's introspection data and private libraries).
- `ADW_DISABLE_PORTAL=1`: no xdg-desktop-portal is built, and libadwaita only reads the gsettings color-scheme (Dark Style) when the portal is disabled.

### Odds and ends

- `XDG_RUNTIME_DIR` comes from `/etc/profile.d/xdg-runtime-dir.sh` (`/run/user/$(id -u)`, created mode 700).
- `/etc/machine-id` is a symlink to `/var/lib/dbus/machine-id`, created by `dbus-uuidgen --ensure`; dbus and elogind both want it.
- Settings are gsettings/dconf: `dconf-service` D-Bus-activates on the session bus on first write and persists to `~/.config/dconf/user`.

### weston vs GNOME

`weston` uses seatd directly (independent of elogind), which is why the simple compositor and the full GNOME session coexist.

### Current limitations

- No display manager: text login on the console only, no graphical login screen.
- No Gdm, so the shell disables screen locking ("Screen Lock disabled — Screen Locking requires the GNOME display manager" notification at session start).
- No NetworkManager: no wifi/bluetooth tiles in quick settings; dhcpcd on eth0 only.
- No PipeWire: no audio.
- No calendar server (evolution-data-server): the clock has no calendar dropdown.
- No librsvg: SVG icons degrade to a generic glyph; PNG icons render fine.
- No gnome-settings-daemon: gnome-session logs "Missing required core component Settings, expect trouble…" and some integration (e.g. color management) is absent.

<a name="stages"></a>
## Build Stages

The Pizlix build proceeds in the following stages.

The Pizlix build snapshots after each successful stage so that it's possible to restart the build at that stage later. This is great for troubleshooting!

### Pre-LC

This is the bootstrapping phase that uses a Yolo-C GCC to build a Yolo-C toolchain within the `/mnt/lfs` chroot environment.

If you want to just do this stage of the build and nothing more, do `sudo ./build_prelc.sh`.

If you want to start the build here, do `sudo ./build.sh`.

### LC

This is the phase where the chroot environment is pizlonated with a Fil-C compiler. This builds the Fil-C compiler and slams it into `/mnt/lfs`.

If you want to just do this stage of the build and nothing more, do `sudo ./build_lc.sh`.

If you want to start the build here, do `sudo ./build_with_recovered_prelc.sh`.

### Post-LC

This is the actual Linux From Scratch build (Chapters 8, 9, 10 of the LFS book) using the Fil-C toolchain. After this completes, the Yolo-C stuff produced in Pre-LC is mostly eliminated, except for what is necessary to run the Fil-C compiler and the GCC used for building the kernel.

If you want to just do this stage of the build and nothing more, do `sudo ./build_postlc.sh`.

If you want to start the build here, do `sudo ./build_with_recovered_lc.sh`.

### Post-LC 2

This builds BLFS (Beyond Linux From Scratch) components that I like to have, such as openssh, emacs, dhcpcd, and cmake.

If you want to just do this stage of the build and nothing more, do `sudo ./build_postlc2.sh`.

If you want to start the build here, do `sudo ./build_with_recovered_postlc.sh`.

### Post-LC 3

This builds the Wayland environment and Weston so that you can have a GUI.

If you want to just do this stage of the build and nothing more, do `sudo ./build_postlc3.sh`.

If you want to start the build here, do `sudo ./build_with_recovered_postlc2.sh`.

### Post-LC 7

This builds the GNOME desktop on top of the Wayland environment: dbus, elogind, dconf, mutter (the Wayland compositor), gnome-session, libadwaita, and a couple of GNOME apps. It also installs the session wiring (dbus/elogind bootscripts, the PAM bits that make logins register with elogind, and the `pizlix-gnome`/`pizlix-shell` launchers). At this point `pizlix-gnome` starts a bare mutter session; the shell UI comes in the next stage.

If you want to just do this stage of the build and nothing more, do `sudo ./build_postlc7.sh`.

If you want to start the build here, do `sudo ./build_with_recovered_postlc6.sh`.

### Post-LC 8

This builds GNOME Shell: the SpiderMonkey JavaScript engine (mozjs), gjs (the JS-to-GNOME bindings), and gnome-shell (the full GNOME desktop shell: top bar, activities overview, app grid, notifications). After this stage, `pizlix-gnome` starts the full GNOME Shell session — this is the full GNOME desktop experience. If you want to just do this stage of the build and nothing more, do `sudo ./build_postlc8.sh`. If you want to start the build here, do `sudo ./build_with_recovered_postlc7.sh`.

### Other Tricks

You can mount the chroot's virtual filesystems with `sudo ./build_mount.sh`. You can unmount the chroot's virtual filesystems with `sudo ./build_unmount.sh`.

You can hop into the chroot with `sudo ./enter_chroot.sh`. However, if you're past Post-LC, then you'll need to do `sudo ./enter_chroot_late.sh` instead.

If you want to just rebuild `libpizlo.so` runtime and slam it into the chroot, do `sudo ./rebuild_pas.sh`.

If you want to just rebuild the compiler, `libpizlo.so`, and user glibc, do `sudo ./rebuild_lc.sh`.

