tcg_ss = ss.source_set()
tcg_ss.add(files(
  'tcg-all.c',
  'cpu-exec-common.c',
  'cpu-exec.c',
  'tb-maint.c',
  'tcg-runtime-gvec.c',
  'tcg-runtime.c',
  'translate-all.c',
  'translator.c',
))
tcg_ss.add(when: 'CONFIG_USER_ONLY', if_true: files('user-exec.c'))
tcg_ss.add(when: 'CONFIG_SOFTMMU', if_false: files('user-exec-stub.c'))
tcg_ss.add(when: 'CONFIG_PLUGIN', if_true: [files('plugin-gen.c')])
tcg_ss.add(when: 'CONFIG_VTUNE_JITPROFILING', if_true: [vtune_jitprofiling])

specific_ss.add_all(when: 'CONFIG_TCG', if_true: tcg_ss)

specific_ss.add(when: ['CONFIG_SOFTMMU', 'CONFIG_TCG'], if_true: files(
  'cputlb.c',
  'hmp.c',
))

tcg_module_ss.add(when: ['CONFIG_SOFTMMU', 'CONFIG_TCG'], if_true: files(
  'tcg-accel-ops.c',
  'tcg-accel-ops-mttcg.c',
  'tcg-accel-ops-icount.c',
  'tcg-accel-ops-rr.c',
))
