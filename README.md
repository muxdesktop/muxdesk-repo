# muxdesk-repo

MuxDesk (Arch-ভিত্তিক নিজের distro) — pacman repo + rootfs hosting।

## Pacman repo (GitHub Pages)
এই git repo টাই Pages থেকে serve হয়। ফোল্ডার কাঠামো:
```
<arch>/            x86_64, aarch64, arm, i686
  *.pkg.tar.xz     আমাদের package গুলো
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

## Rootfs image (GitHub Releases)
Image গুলো বড় (~800 MB) বলে git-এ রাখা যায় না — সেগুলো এই repo-র
Releases-এ asset হিসেবে ওঠে: tag `rootfs-v3.2`, asset নাম
`muxdesk-rootfs-<arch>-v3.2.tar.gz` (+ `.sha256`)।
বানানো: `../muxdesk-rootfs/build-rootfs.sh <arch>` (parent project-এ)।
তোলা: `gh release upload rootfs-v3.2 <file>` অথবা browser দিয়ে
Releases → Draft a new release → asset টেনে দেওয়া।

## Package আবার বানানো
Parent project-এর `muxdesk-repo/build-repo.sh all` চালিয়ে
`repo/<arch>/` থেকে এই repo-র `<arch>/` ফোল্ডারে copy করলেই হবে।
