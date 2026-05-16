# Snowfall — YetiOS Login Manager

A minimal, DRM-native graphical login manager for YetiOS.

## Architecture

Snowfall is a framebuffer login manager that renders directly to
DRM/KMS (no X11, no Wayland compositor required). It authenticates
users via PAM, then execs the selected Wayland compositor as the
authenticated user.

```
snowfall (DRM master)
  ├── drm.c          KMS/DRM framebuffer setup
  ├── input.c        libinput keyboard handling
  ├── renderer.c     Cairo-based UI drawing
  ├── auth.c         PAM authentication
  ├── session.c      Session discovery + launch
  ├── ui_state.c     UI state machine (field focus, navigation)
  └── main.c         Lifecycle: init → render loop → auth → exec
```

When snowfall starts, it grabs DRM master (which causes snowcone to
exit). After successful authentication it drops DRM master, switches
to the user's VT, and execs the compositor.

## Dependencies

- libdrm
- libinput + libudev
- cairo
- libpam
- libxkbcommon (keymap handling)

### Alpine / YetiOS

```sh
apk add libdrm-dev libinput-dev cairo-dev linux-pam-dev libxkbcommon-dev eudev-dev
```

## Build

```sh
make
sudo make install
```

## OpenRC

```sh
sudo rc-update add snowfall default
```

The service depends on `udev` (for libinput) and starts after
`snowcone` (the boot splash). When snowfall grabs DRM master,
snowcone detects the loss and exits.

## Session files

Snowfall reads `.desktop` files from `/usr/share/wayland-sessions/`.
Each file must have an `Exec=` line. Example:

```ini
[Desktop Entry]
Name=Sway
Exec=sway
Type=Application
```

## License

GNU GPL v3.0 — YetiOS Project
