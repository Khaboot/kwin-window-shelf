# Window Shelf for KWin

Window Shelf is a native effect for KWin on KDE Plasma 6. It shows live
previews of open windows in a side panel, without sorting or grouping them.
Its interaction model is inspired in part by Apple's Stage Manager; Window
Shelf is an independent project and is not affiliated with or endorsed by
Apple.

Window Shelf is licensed under the GNU General Public License, version 3 or
any later version. See [LICENSE](LICENSE).

## Build and install

This is a native KWin plugin, not a KPackage script. `kpackagetool6` and the
KDE Store installation flow for scripted effects cannot install it. Building
requires the development packages matching the installed KWin version, Qt 6,
KF6, CMake, and Extra CMake Modules.

On Fedora, install the compiler and build dependencies first:

```sh
sudo dnf install gcc-c++ cmake extra-cmake-modules kwin-devel \
    kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kglobalaccel-devel \
    kf6-ki18n-devel \
    kf6-kcmutils-devel libepoxy-devel libdrm-devel
```

If `kwin-devel` is unavailable, enable the repository that provides
development packages for your installed KWin version.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

The install step places the plugin in KWin's Qt plugin directory. Log out and
back in so KWin discovers the new plugin, then enable **Window Shelf** in
**System Settings → Window Management → Desktop Effects**.

To uninstall the native plugin, remove the installed
`kwin4_effect_window_shelf.so` file from KWin's Qt plugin directory
(`kwin/effects/plugins`) and log out and back in.

## Use

- In hover mode, move the pointer to the selected screen edge to show the
  previews. The panel hides when the pointer leaves it.
- Choose hover or always-visible behavior, and the screen edge, in the effect's
  settings in **System Settings → Window Management → Desktop Effects**.
- **Meta+Ctrl+Space** toggles the panel.
- **Meta+Ctrl+Shift+Space** moves it to the other side.
- **Meta+Ctrl+Alt+Space** toggles hover and always-visible modes.
- Set preview transparency in the effect settings. The previews become fully
  opaque while the pointer is over the panel; only the card under the pointer
  grows slightly. The pointer remains visible above the cards and changes to a
  pointing hand over clickable previews.
- Click a preview to activate and restore that window, including when it is
  minimized. In hover mode, clicks outside the panel dismiss it. In
  always-visible mode, clicks outside the panel continue to the application
  and leave the panel open.
- Preview cards use a consistent size and a subtle drop shadow to distinguish
  them from the desktop.

Settings are stored in the `Effect-kwin4_effect_window_shelf` group in
`kwinrc`. Visible and minimized normal windows and dialogs on the current
desktop, activity, and screen are shown. `xwaylandvideobridge` is excluded.
