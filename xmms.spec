# XMMS Resurrection — RPM spec for GTK3 + CMake build
#
# Version is injected by CI via:
#   sed "s/^Version:.*/Version:  $PKG_VER/" xmms.spec
# Or pass  --define "_version X.Y.Z"  to rpmbuild directly.
#
# Sub-packages:
#   xmms          — core player binary, libxmms, all built-in plugins
#   xmms-devel    — headers, pkg-config, m4 macro
#   xmms-alsa     — ALSA output plugin (optional, needs alsa-lib)
#   xmms-pulse    — PulseAudio output plugin (optional, needs pulseaudio-libs)

%global _hardened_build 1

Name:           xmms
Version:        1.3.0
Release:        1%{?dist}
Summary:        X Multimedia System — classic Winamp-style audio player (GTK3 resurrection)

License:        GPL-2.0-or-later
URL:            https://github.com/tacitness/xmms
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  pkg-config
BuildRequires:  gtk3-devel
BuildRequires:  glib2-devel
BuildRequires:  cairo-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  libvorbis-devel
BuildRequires:  libogg-devel
BuildRequires:  mpg123-devel
BuildRequires:  flac-devel
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  libXxf86vm-devel
BuildRequires:  gettext-devel
BuildRequires:  desktop-file-utils

Requires:       hicolor-icon-theme

%description
XMMS (X Multimedia System) is a classic Winamp-style audio player resurrected
with GTK3, faithfully reproducing the original Winamp 2.x skin-based interface
while adding PulseAudio, ALSA, MP3 (mpg123), OGG Vorbis, and FLAC support.

Features:
  - Winamp 2.x skin support (ZIP-based .wsz skins)
  - 10-band graphic equalizer
  - Scrolling playlist editor
  - Visualization plugins (blurscope, spectrum analyser)
  - Extensible plugin architecture (input, output, effect, general, vis)
  - MPRIS2 D-Bus interface for desktop integration

%package        devel
Summary:        X Multimedia System — development headers and pkg-config
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       glib2-devel

%description    devel
Header files, pkg-config data, and the xmms.m4 Autoconf macro required
for compiling XMMS input, output, effect, general, and visualization plugins.

%package        alsa
Summary:        X Multimedia System — ALSA output plugin
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       alsa-lib

%description    alsa
ALSA (Advanced Linux Sound Architecture) output plugin for XMMS.
Install this package to use ALSA directly for audio playback.

%package        pulse
Summary:        X Multimedia System — PulseAudio output plugin
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       pulseaudio-libs

%description    pulse
PulseAudio output plugin for XMMS.
Install this package to use PulseAudio for audio playback.

# ─────────────────────────────────────────────────────────────────────────────
%prep
%autosetup

# ─────────────────────────────────────────────────────────────────────────────
%build
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBDIR=%{_libdir} \
    -DXMMS_GTK_VERSION=3 \
    -DXMMS_ENABLE_ALSA=ON \
    -DXMMS_ENABLE_PULSE=ON \
    %{nil}

cmake --build build -j%{_smp_build_ncpus}

# ─────────────────────────────────────────────────────────────────────────────
%install
DESTDIR=%{buildroot} cmake --install build

# Validate .desktop file
desktop-file-validate \
    %{buildroot}%{_datadir}/applications/xmms.desktop || :

# Remove static libs (not installed by cmake, safety guard)
find %{buildroot} -name '*.a' -delete

# ─────────────────────────────────────────────────────────────────────────────
%post
/sbin/ldconfig
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database -q %{_datadir}/applications &>/dev/null || :

%postun
/sbin/ldconfig
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
/usr/bin/update-desktop-database -q %{_datadir}/applications &>/dev/null || :

# ─────────────────────────────────────────────────────────────────────────────
%files
%license COPYING
%doc AUTHORS ChangeLog README NEWS FAQ
%{_bindir}/xmms
%{_bindir}/wmxmms
%{_libdir}/libxmms.so.*
# Core input plugins
%{_libdir}/xmms/Input/mpg123.so
%{_libdir}/xmms/Input/vorbis.so
%{_libdir}/xmms/Input/wav.so
%{_libdir}/xmms/Input/cdaudio.so
%{_libdir}/xmms/Input/tonegen.so
# OSS output (no extra deps)
%{_libdir}/xmms/Output/OSS.so
%{_libdir}/xmms/Output/disk_writer.so
# Effect + general + vis plugins
%{_libdir}/xmms/Effect/
%{_libdir}/xmms/General/
%{_libdir}/xmms/Visualization/
# Data
%{_datadir}/xmms/
%{_datadir}/applications/xmms.desktop
%{_datadir}/icons/hicolor/*/apps/xmms.png

%files devel
%{_libdir}/libxmms.so
%{_includedir}/xmms/

%files alsa
%{_libdir}/xmms/Output/ALSA.so

%files pulse
%{_libdir}/xmms/Output/Pulse.so

# ─────────────────────────────────────────────────────────────────────────────
%changelog
* Mon Apr 28 2025 XMMS Resurrection Team <xmms-dev@example.com> - 1.3.0-1
- Rewrite spec for GTK3 + CMake build system (replaces GTK+1/2 + autotools)
- Remove ESD sub-package (dead dependency)
- Add PulseAudio output plugin sub-package
- Update BuildRequires: cmake >= 3.20, ninja-build, gtk3-devel, cairo-devel,
  pulseaudio-libs-devel, mpg123-devel, flac-devel
- Add freedesktop icon set and .desktop entry to files
- Use autosetup and license macros
- Set global _hardened_build 1
