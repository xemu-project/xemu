d3d11_clear_dep = cc.find_library('d3d11', required: true)
dxgi_clear_dep = cc.find_library('dxgi', required: true)
specific_ss.add([d3d11_clear_dep, dxgi_clear_dep,
                 files('d3d11_bridge.c', 'clear.cc', 'clear-adapter.cc',
                       'clear-executor.cc', 'surface-adapter.cc',
                       'context.cc', 'surface.cc',
                       'state-adapter.cc', 'renderer.c', 'renderer.cc')])
specific_ss.add(files('draw.cc', 'draw-executor.cc', 'vertex-adapter.cc'))
specific_ss.add(files('psh_interpreter.cc', 'vsh_cpu_transform.cc'))
