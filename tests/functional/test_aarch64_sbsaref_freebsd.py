#!/usr/bin/env python3
#
# Functional test that boots a kernel and checks the console
#
# SPDX-FileCopyrightText: 2023-2024 Linaro Ltd.
# SPDX-FileContributor: Philippe Mathieu-Daudé <philmd@linaro.org>
# SPDX-FileContributor: Marcin Juszkiewicz <marcin.juszkiewicz@linaro.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os

from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from qemu_test import interrupt_interactive_console_until_pattern
from unittest import skipUnless
from test_aarch64_sbsaref import fetch_firmware


class Aarch64SbsarefFreeBSD(QemuSystemTest):

    ASSET_FREEBSD_ISO = Asset(
        ('https://download.freebsd.org/releases/arm64/aarch64/ISO-IMAGES/'
         '14.1/FreeBSD-14.1-RELEASE-arm64-aarch64-bootonly.iso'),
        '44cdbae275ef1bb6dab1d5fbb59473d4f741e1c8ea8a80fd9e906b531d6ad461')

    # This tests the whole boot chain from EFI to Userspace
    # We only boot a whole OS for the current top level CPU and GIC
    # Other test profiles should use more minimal boots
    def boot_freebsd14(self, cpu=None):
        fetch_firmware(self)

        img_path = self.ASSET_FREEBSD_ISO.fetch()

        self.vm.set_console()
        self.vm.add_args(
            "-drive", f"file={img_path},format=raw,snapshot=on",
        )
        if cpu:
            self.vm.add_args("-cpu", cpu)

        self.vm.launch()
        wait_for_console_pattern(self, 'Welcome to FreeBSD!')

    def test_sbsaref_freebsd14_cortex_a57(self):
        self.boot_freebsd14("cortex-a57")

    def test_sbsaref_freebsd14_default_cpu(self):
        self.boot_freebsd14()

    def test_sbsaref_freebsd14_max_pauth_off(self):
        self.boot_freebsd14("max,pauth=off")

    @skipUnless(os.getenv('QEMU_TEST_TIMEOUT_EXPECTED'),
                'Test might timeout due to PAuth emulation')
    def test_sbsaref_freebsd14_max_pauth_impdef(self):
        self.boot_freebsd14("max,pauth-impdef=on")

    @skipUnless(os.getenv('QEMU_TEST_TIMEOUT_EXPECTED'),
                'Test might timeout due to PAuth emulation')
    def test_sbsaref_freebsd14_max(self):
        self.boot_freebsd14("max")


if __name__ == '__main__':
    QemuSystemTest.main()
