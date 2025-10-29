from __future__ import print_function
#
# Test the SVE ZReg reports the right amount of data. It uses the
# sve-ioctl test and examines the register data each time the
# __sve_ld_done breakpoint is hit.
#
# This is launched via tests/guest-debug/run-test.py
#

import gdb
from test_gdbstub import main, report

initial_vlen = 0


class TestBreakpoint(gdb.Breakpoint):
    def __init__(self, sym_name="__sve_ld_done"):
        super(TestBreakpoint, self).__init__(sym_name)
        # self.sym, ok = gdb.lookup_symbol(sym_name)

    def stop(self):
        val_i = gdb.parse_and_eval('i')
        global initial_vlen
        try:
            for i in range(0, int(val_i)):
                val_z = gdb.parse_and_eval("$z0.b.u[%d]" % i)
                report(int(val_z) == i, "z0.b.u[%d] == %d" % (i, i))
            for i in range(i + 1, initial_vlen):
                val_z = gdb.parse_and_eval("$z0.b.u[%d]" % i)
                report(int(val_z) == 0, "z0.b.u[%d] == 0" % (i))
        except gdb.error:
            report(False, "checking zregs (out of range)")

        # Check the aliased V registers are set and GDB has correctly
        # created them for us having recognised and handled SVE.
        try:
            for i in range(0, 16):
                val_z = gdb.parse_and_eval("$z0.b.u[%d]" % i)
                val_v = gdb.parse_and_eval("$v0.b.u[%d]" % i)
                report(int(val_z) == int(val_v),
                       "v0.b.u[%d] == z0.b.u[%d]" % (i, i))
        except gdb.error:
            report(False, "checking vregs (out of range)")


def run_test():
    "Run through the tests one by one"

    print ("Setup breakpoint")
    bp = TestBreakpoint()

    global initial_vlen
    vg = gdb.parse_and_eval("$vg")
    initial_vlen = int(vg) * 8

    gdb.execute("c")


main(run_test, expected_arch="aarch64")
