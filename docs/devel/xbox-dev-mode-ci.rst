Xbox Series X|S Developer Mode CI
=================================

``.github/workflows/build-xbox-uwp.yml`` is a manually triggered workflow for
the x64 Xbox Series X|S Developer Mode package. It is deliberately not a
normal branch or pull-request workflow: a package build is expensive and the
console deployment step requires a device-local certificate and Device Portal
credentials.

The workflow has two distinct paths:

* ``host-probe-diagnostic`` may build the checked-in
  ``tests/xbox/uwp-host-probe``. The probe validates selected AppContainer and
  D3D11 capabilities, but its artifact is explicitly diagnostic and can never
  satisfy the release gate.
* ``target-build`` is enabled only when
  ``scripts/ci/validate-xbox-uwp.ps1`` finds the complete xemu UWP target. It
  builds the full emulator, runs the CPU/VSH, pixel-shader, D3D11 and WARP
  tests, signs the full package, and emits symbols, hashes, an SPDX SBOM and
  build metadata.

Current repository state
------------------------

The full xemu UWP application project is not checked in yet. The required
contract is a native x64 project at
``ui/xui/uwp/xemu-uwp.vcxproj`` containing
``<XemuUwpTarget>true</XemuUwpTarget>`` and a single
``Package.appxmanifest``. The manifest must use a stable publisher, x64
architecture, ``Windows.Xbox`` (with parseable ``MinVersion`` and
``MaxVersionTested`` and a minimum ``MinVersion`` of ``10.0.14393.0``;
``Windows.Universal`` is allowed only as an additional explicitly supported
family) and a launchable application with an explicit
``.exe`` ``Executable`` and non-empty ``EntryPoint``. It must declare the
official x64-compatible ``Microsoft.VCLibs.140.00`` dependency. The
``UWPDesktop`` framework identity is rejected. The
host-probe project must not be copied or renamed to satisfy this check.
The pinned Microsoft dependency archive also contains a UWPDesktop package;
it is skipped during selection and is never copied into the bundle.
Selection, the target validator, and the signing step unpack the selected
package and require its identity ``Version`` to be at least the full
application manifest's VCLibs ``MinVersion`` (as well as the fixed framework
floor).

The workflow creates a clean, authentic ``build-xbox-uwp-tests`` directory,
runs the repository's ``configure``/Meson setup, compiles the test targets,
and then runs the executables named by
``scripts/ci/run-xbox-uwp-tests.ps1``. It never accepts a user-supplied or
pre-existing build directory. If configure is not possible, the separate
clang-cl smoke reports that fact and the full release gate remains red; no
fake headers or prebuilt test artifact is accepted.

Signing and Dev Mode installation
---------------------------------

For a real release, configure the repository secrets
``XBOX_DEV_PFX_BASE64`` and ``XBOX_DEV_PFX_PASSWORD``. The PFX certificate
subject must exactly equal the target manifest's ``Identity Publisher``.
The workflow signs with SHA-256 and publishes only the public ``.cer`` beside
the package; the PFX is never uploaded.

``dev_test_certificate`` creates a short-lived self-signed code-signing
certificate whose subject is taken from the target manifest. It is explicitly
labelled Xbox Dev Mode test material, expires after 30 days, and is never a
Store/production signing path. Install the emitted public certificate on the
development console through Device Portal before installing the package. Do
not use this mode for a public release.

The final artifact is one deterministic ZIP, using the checked-in
``new-deterministic-zip.ps1`` implementation with fixed sorted paths,
timestamps, and the runner's .NET ``ZipArchive`` implementation, containing
the signed full xemu package, verified x64 VCLibs dependency, public
certificate, symbols, test report, signing metadata, and an inner
``SHA256SUMS.txt``. The outer
``SHA256SUMS.txt`` covers only the final ZIP, SBOM, and build metadata; it
intentionally excludes itself to avoid a recursive hash. SPDX IDs are derived
from the canonical ``bundle/<normalized path>`` string, so same-named files in
different directories cannot collide within this bundle schema.

The official x64 Microsoft.VCLibs package is downloaded from the pinned
Microsoft winget-cli dependency archive and checked against the SHA-256 hash
before it is used. It is not committed to the repository.

Manual operation
----------------

From the Actions tab, select ``Xbox Series X|S Dev Mode UWP`` and
``Run workflow``. Leave ``create_release`` disabled while validating the
package. The optional release job requires a successful full target and test
gate, ``Release`` configuration, ``pfx_required`` signing with both PFX
secrets, and a tag matching
``vMAJOR.MINOR.PATCH-xbox-devmode.N``, and requests ``contents: write`` only
for that job. No release can be created from the host-probe artifact.

The workflow does not deploy to an Xbox. Use the checked-in
``tests/xbox/uwp-host-probe/scripts/deploy-devmode.ps1`` or the Device Portal
manually after inspecting the signed artifact and its hashes. Device Portal
credentials and console addresses are intentionally not stored in GitHub.

> **Agent Declaration**: This workflow and documentation were created with assistance from OpenAI Codex GPT-5 / Luna subagent.
