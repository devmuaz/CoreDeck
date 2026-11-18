# Third-party notices

CoreDeck is MIT licensed. The components below are distributed with it, or used to build it, under their own terms.

## Dear ImGui

[Dear ImGui](https://github.com/ocornut/imgui) is Copyright (c) 2014-2026 Omar Cornut and contributors, licensed under the MIT License. See `extern/imgui/LICENSE.txt`.

## Dear ImGui memory editor

[`imgui_memory_editor.h`](https://github.com/ocornut/imgui_club) from imgui_club is Copyright (c) 2017-2024 Omar Cornut and contributors, licensed under the MIT License. See `extern/imgui_club/LICENSE.txt`.

## ImGuiColorTextEdit

[ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) is Copyright (c) 2017 BalazsJako and contributors, licensed under the MIT License. See `extern/ImGuiColorTextEdit/LICENSE`.

## GLFW

[GLFW](https://github.com/glfw/glfw) is Copyright (c) 2002-2006 Marcus Geelnard and Copyright (c) 2006-2019 Camilla Löwy, licensed under the zlib License. See `extern/glfw/LICENSE.md`.

## nativefiledialog-extended

[nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) is Copyright (c) Bernard Teo and contributors, licensed under the zlib License. It is based on [Native File Dialog](https://github.com/mlabbe/nativefiledialog) by Michael Labbe. See `extern/nativefiledialog-extended/LICENSE`.

## reflect-cpp

[reflect-cpp](https://github.com/getml/reflect-cpp) is Copyright (c) 2023-2025 Code17 GmbH, licensed under the MIT License. See `extern/reflect-cpp/LICENSE`.

## miniz

[miniz](https://github.com/richgel999/miniz) is Copyright (c) 2013-2014 RAD Game Tools and Valve Software, and Copyright (c) 2010-2014 Rich Geldreich and Tenacious Software LLC, licensed under the MIT License. See `extern/miniz/LICENSE`.

## XZ Embedded

[XZ Embedded](https://github.com/tukaani-project/xz-embedded) is Copyright (C) The XZ Embedded authors and contributors and is distributed under the 0BSD licence. See `extern/xz-embedded/COPYING`.

## Frida

[Frida](https://github.com/frida/frida), including the statically linked frida-core devkit, is Copyright (C) 2014-2026 Ole André Vadla Ravnås and contributors.

It is licensed under the wxWindows Library Licence, Version 3.1, which includes an exception that permits static linking. The full text is at https://github.com/frida/frida/blob/main/COPYING.

## frida-java-bridge

[frida-java-bridge](https://github.com/frida/frida-java-bridge) 7.0.13 is Copyright (C) Ole André Vadla Ravnås and contributors.

It is licensed under the GNU Lesser General Public License 2.0 with the wxWindows exception 3.1. The package source ships in `assets/frida/project/node_modules/frida-java-bridge`.

## GNU libintl

[GNU gettext](https://www.gnu.org/software/gettext/) provides `libintl`. The runtime library is Copyright (C) the Free Software Foundation and is licensed under the GNU Lesser General Public License 2.1 or later.

macOS and Windows builds copy this shared library next to CoreDeck when it is not already part of the C runtime. Linux builds use the copy provided by the system.

## JetBrains Mono

[JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) is Copyright (c) JetBrains and contributors and is licensed under the SIL Open Font License 1.1. The Regular and Bold files ship in `assets/fonts`.

## Noto Sans CJK

[Noto Sans CJK](https://github.com/notofonts/noto-cjk) is Copyright (c) Adobe and Google and is licensed under the SIL Open Font License 1.1. See `assets/fonts/NotoSansCJK-OFL.txt`.

## Font Awesome Free

[Font Awesome Free](https://github.com/FortAwesome/Font-Awesome) Solid is Copyright (c) Fonticons, Inc. and contributors. The font file `FontAwesome7Free-Solid-900.otf` is licensed under the SIL Open Font License 1.1 and ships in `assets/fonts`.

## sentry-native

Release builds that are configured with a Sentry DSN include [sentry-native](https://github.com/getsentry/sentry-native), Copyright (c) 2019 Sentry and individual contributors, licensed under the MIT License. See `extern/sentry-native/LICENSE`.

Those builds also include its Crashpad backend, [Crashpad](https://chromium.googlesource.com/crashpad/crashpad), which is licensed under the Apache License 2.0. See `extern/sentry-native/external/crashpad/LICENSE`.

## Catch2

[Catch2](https://github.com/catchorg/Catch2) is Copyright (c) Catch2 authors and contributors and is licensed under the Boost Software License 1.0. See `extern/catch2/LICENSE.txt`.

Catch2 is used only to build and run tests. It is not linked into the CoreDeck application.
