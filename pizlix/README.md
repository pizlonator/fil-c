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

To see what this thing is really capable of, log in as a non-root user (like `pizlo`) and do:

    weston

And enjoy a totally memory safe GUI!

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

### Other Tricks

You can mount the chroot's virtual filesystems with `sudo ./build_mount.sh`. You can unmount the chroot's virtual filesystems with `sudo ./build_unmount.sh`.

You can hop into the chroot with `sudo ./enter_chroot.sh`. However, if you're past Post-LC, then you'll need to do `sudo ./enter_chroot_late.sh` instead.

If you want to just rebuild `libpizlo.so` runtime and slam it into the chroot, do `sudo ./rebuild_pas.sh`.

If you want to just rebuild the compiler, `libpizlo.so`, and user glibc, do `sudo ./rebuild_lc.sh`.

