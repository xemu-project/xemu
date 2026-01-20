if host_os == 'windows'
	libnvapi = static_library('nvapi', files(['nvapi.c']))
	nvapi = declare_dependency(link_with: libnvapi, include_directories: ['.'])
endif
