# The UWP host is intentionally a separate target: the desktop xemu UI keeps
# its SDL/OpenGL entry point, while Xbox Dev Mode uses this CoreWindow target.
if host_os != 'windows'
  error('xbox_uwp requires a Windows host')
endif

if cxx.get_argument_syntax() != 'msvc'
  error('xbox_uwp requires a MSVC-compatible C++/CX compiler')
endif

windowsapp = cc.find_library('windowsapp', required: true)
xemu_uwp_host = static_library('xemu-uwp-host',
                               files('corewindow-host.cc',
                                     'storage-lifecycle.cc'),
                               cpp_args: ['/ZW', '/EHsc'],
                               dependencies: [d3d11, dxgi, windowsapp],
                               include_directories: include_directories('..'))

# Keep this dependency available to the eventual UWP application target. It
# is not linked into the SDL desktop executable by this subdirectory.
xemu_uwp_host_dep = declare_dependency(link_with: xemu_uwp_host,
                                        dependencies: [d3d11, dxgi,
                                                       windowsapp])

# This executable is a WinRT-independent ABI/contract check.  It deliberately
# links only the storage/lifecycle implementation, so it can run on a normal
# Windows build machine without a CoreWindow or an installed package.
xemu_uwp_contract_test = executable(
  'test-xemu-uwp-storage-lifecycle',
  files('storage-lifecycle-contract-test.cc',
        'storage-lifecycle-c-abi-test.c', 'storage-lifecycle.cc'),
  cpp_args: ['/EHsc'],
)
test('xemu-uwp-storage-lifecycle', xemu_uwp_contract_test)
