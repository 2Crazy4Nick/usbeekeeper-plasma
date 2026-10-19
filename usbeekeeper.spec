%global upstream_version 0.1.0-dev.2
%global applet_id io.github._2crazy4nick.usbeekeeper
%global qml_module io/github/_2crazy4nick/usbeekeeper

# Qt's on-disk QML cache validates source timestamps, not source contents.
# Clamping successive builds to the same changelog day can reuse obsolete QML.
# Preserve the freshly assembled applet's mtimes; never delete user caches.
%global clamp_mtime_to_source_date_epoch 0

Name:           usbeekeeper
Version:        0.1.0~dev.2
Release:        2%{?dist}
Summary:        KDE Plasma USB inspection and topology widget

License:        MIT
URL:            https://github.com/2Crazy4Nick/usbeekeeper-plasma
Source0:        %{url}/archive/refs/tags/v%{upstream_version}.tar.gz#/%{name}-%{upstream_version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  cmake-rpm-macros
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-rpm-macros
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(KF6I18n)
BuildRequires:  cmake(KF6CoreAddons)
BuildRequires:  cmake(KF6Package)
BuildRequires:  cmake(Plasma)
BuildRequires:  python3

# QML imports are not all visible to the ELF dependency generator.
Requires:       plasma-workspace >= 6.0
Requires:       libplasma >= 6.0
Requires:       kf6-kirigami >= 6.0
Requires:       kf6-kcmutils >= 6.0
Requires:       qt6-qtdeclarative >= 6.0

%description
USBeeKeeper is a KDE Plasma 6 USB inspector with link information, companion
hub grouping, topology, physical profiles and local aliases. This package
contains both the Plasma applet and its native Qt 6 QML plugin.

The separately installed usbeehived daemon must be running and expose
org.usbeehive.Devices5 on the session D-Bus. It is an external prerequisite;
this package neither bundles nor builds usbeehive. The legacy WhatCable CLI
is not included.

Run usbeekeeper-check-installation as your desktop user to check for local
copies that could shadow the RPM. User configuration is never removed.

%prep
%autosetup -n usbeekeeper-plasma-%{upstream_version}

%build
export PYTHONDONTWRITEBYTECODE=1
%cmake -DBUILD_LEGACY_CLI=OFF -DBUILD_TESTING=ON \
    -DKDE_INSTALL_QMLDIR:PATH=%{_qt6_qmldir}
%cmake_build

%install
%cmake_install
install -Dpm 0755 tools/check-local-install.sh \
    %{buildroot}%{_bindir}/usbeekeeper-check-installation
# Fail rather than shipping a CLI-only or incomplete build.
test -f %{buildroot}%{_datadir}/plasma/plasmoids/%{applet_id}/metadata.json
test -f %{buildroot}%{_qt6_qmldir}/%{qml_module}/libusbeekeeper-plasmoid-plugin.so
test ! -e %{buildroot}%{_bindir}/whatcable-linux

%check
export PYTHONDONTWRITEBYTECODE=1
python3 tools/validate_profiles.py
%ctest

%files
%license %{_datadir}/licenses/usbeekeeper-plasma/
%doc README.md CONTRIBUTING_PROFILES.md packaging/fedora/INSTALL.md
%{_bindir}/usbeekeeper-check-installation
%{_datadir}/plasma/plasmoids/%{applet_id}/
%{_qt6_qmldir}/%{qml_module}/

%changelog
* Mon Oct 19 2026 USBeeKeeper contributors - 0.1.0~dev.2-2
- Packaging-only update; application version unchanged.

* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.2-1
- Prepare development release 0.1.0-dev.2.

* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.1-2
- Verify automatic COPR update delivery.

* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.1-1
- Initial Fedora package with the applet and native QML plugin together.
