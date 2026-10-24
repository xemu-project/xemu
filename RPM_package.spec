Name:           xemu
Version:        0.7.84
Release:        1%{?dist}
Summary:        xemu: Original Xbox Emulator (RPM)

License:        LGPL-2.1, GPL.2.0
URL:            https://github.com/xemu-project/xemu
Source0:        https://github.com/xemu-project/xemu/releases/latest/download/src.tar.gz

BuildRequires: libdrm-devel            
BuildRequires: libslirp-devel
BuildRequires: mesa-libGLU-devel
BuildRequires: gtk3-devel
BuildRequires: libpcap-devel
BuildRequires: libsamplerate-devel
BuildRequires: libaio-devel
BuildRequires: SDL2-devel
BuildRequires: libepoxy-devel 
BuildRequires: pixman-devel
BuildRequires: gcc-c++
BuildRequires: ninja-build
BuildRequires: openssl-devel
BuildRequires: python3-pyyaml

#To update
Requires: libdrm-devel libslirp-devel mesa-libGLU-devel gtk3-devel libpcap-devel libsamplerate-devel libaio-devel SDL2-devel libepoxy-devel pixman-devel gcc-c++ ninja-build openssl-devel python3-pyyaml      
#To update
%description
Xemu original xbox emulator package release for RPM based distributions.

%prep
rm -fr src
wget https://github.com/xemu-project/xemu/releases/lastes/download/src.tar.gz -O $HOME/rpmbuild/SOURCES/src.tar.gz
tar -xzf %{SOURCE0}

%build
./build.sh

%files
%license LICENSE
#TODO: Apply icon.png to executable, possibly using a .desktop file.
/icon.png
/xemu

%install
mkdir -p $HOME/.local/bin
cp ./ui/icons/xemu_128x128.png $RPM_BUILD_ROOT/icon.png
cp ./dist/xemu $RPM_BUILD_ROOT/xemu
install -m755  $RPM_BUILD_ROOT/xemu $HOME/.local/bin/xemu

%changelog
* Wed Mar 01 2023 f
- 
