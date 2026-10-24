Name:       harbour-pinch
Summary:    Noise control for Nothing earbuds
Version:    1.0.0
Release:    1
License:    GPLv3+
URL:        https://github.com/t-sappl/harbour-pinch
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  desktop-file-utils

%description
Pinch controls Nothing and CMF earbuds: noise control (off, transparency,
noise cancellation with low, mid, high or adaptive strength), battery of both
earbuds and the case, gestures, find my earbuds, ear tip fit test, dual
connection, low latency, in-ear detection and – depending on the model – bass
boost, spatial audio, Super Mic and personalized noise cancellation. Pinch
only shows what the earbuds support. Developed with Ear (2), Ear (3) and
Ear (open); other models should work too.

Unofficial app, not affiliated with Nothing Technology.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
