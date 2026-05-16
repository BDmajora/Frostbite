"""
snowfall_integration.py

Single source of truth for snowfall configuration consumed by
yeti-build. Mirrors snowcone_integration.py so the two projects
feel consistent.
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class InstallFiles:
    binary_src:   str  # built artifact in the cloned repo
    binary_dst:   str  # absolute path inside the target rootfs
    openrc_src:   str  # OpenRC init file in the cloned repo
    openrc_dst:   str  # absolute path inside the target rootfs
    pam_src:      str  # PAM config in the cloned repo
    pam_dst:      str  # PAM config destination


FILES = InstallFiles(
    binary_src = "snowfall",
    binary_dst = "/usr/local/sbin/snowfall",
    openrc_src = "snowfall.openrc",
    openrc_dst = "/etc/init.d/snowfall",
    pam_src    = "snowfall.pam",
    pam_dst    = "/etc/pam.d/snowfall",
)


# Runlevel: 'default' starts after 'boot' (where snowcone lives),
# so the splash is already visible when snowfall takes over.
RUNLEVEL = "default"

# Build dependencies that must be installed in the build chroot.
BUILD_DEPS = [
    "libdrm-dev",
    "libinput-dev",
    "cairo-dev",
    "linux-pam-dev",
    "libxkbcommon-dev",
    "eudev-dev",
]
