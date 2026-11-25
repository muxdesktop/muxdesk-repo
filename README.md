# muxdesk-repo

The official package repository and download hub for **MuxDesk**, an Arch-based Linux distribution.

## Pacman repository (GitHub Pages)

This git repository is served directly via GitHub Pages. Layout:

```
<arch>/              x86_64, aarch64, arm, i686
  *.pkg.tar.xz       our packages
  muxdesk.db.tar.gz  repo database (repo-add format)
  muxdesk.db
index.html, .nojekyll
```

pacman.conf:

```ini
[muxdesk]
SigLevel = Never
Server = https://muxdesktop.github.io/muxdesk-repo/$arch
```

## App APKs live in this repo's Releases

The MuxLike app APK is published here as well — rootfs images and APKs share the same Releases page (separate tags, e.g. `app-v1.0`).

## Rootfs images (GitHub Releases)

Images are large (~800 MB), so they are not kept in git — they are uploaded as Release assets: tag `rootfs-v3.2`, asset name `muxdesk-rootfs-<arch>-v3.2.tar.gz` (+ `.sha256`).

## Screenshots

Previews of the MuxLike desktop environment are in the `screenshots/` folder.
