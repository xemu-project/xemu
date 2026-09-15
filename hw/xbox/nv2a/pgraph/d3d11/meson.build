d3d11_clear_dep = cc.find_library('d3d11', required: true)
dxgi_clear_dep = cc.find_library('dxgi', required: true)
specific_ss.add([d3d11_clear_dep, dxgi_clear_dep,
                 files('clear.cc', 'clear-adapter.cc',
                       'clear-executor.cc', 'surface-adapter.cc',
                       'context.cc', 'surface.cc')])
