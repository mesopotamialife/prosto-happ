# Prosto Happ

Small online bootstrapper for Happ Desktop.

It downloads the latest official Happ x64 installer, installs it silently,
applies the required HKCU `OrganizationDefaults` settings and forces Chrome
to use the Windows system proxy through Chrome policy.

The Happ installer is **not embedded** in this EXE, so the bootstrapper stays small.

Build: GitHub Actions / Windows Server 2019 / MSVC x64.
The bootstrapper is compiled with Win7-era API targeting; actual compatibility
of the current Happ release with Windows 7 must be tested separately.
