# Working on ThinkSmart Home

Read README.md, docs/INSTALL.md and docs/CONFIGURATION.md before setup work.
Use this repository's scripts and record the configuration you actually install.

- Collect the device model/panel variant, current OS, SSH destination, desired
  modules and weather location/timezone. Do not assume another user's IP address,
  SSH key, partition sizes, network names or account credentials.
- Put user configuration and credentials in ignored `private/`. Keep that
  directory mode 700 and credential files mode 600. Never print keys/passwords,
  put them in chat, commit them or include them in screenshots/logs.
- Build on the host with Docker. Inspect the existing Debian installation before
  changing runtime packages; preserve working device graphics/audio drivers.
- For stock firmware, use the pinned upstream documentation and inspect the
  actual device layout. Preserve calibration and generate fresh identities for
  a new device. Image building/validation and physical flashing are separate
  actions; obtain explicit authorization for the verified flash target/image.
- Select optional modules through private/device.json. Disabled modules must not
  poll services, open the microphone, start engines, or advertise unusable AI
  tools. Explain that runtime disabling does not strip code from the binary.
- Existing credentials and SSH-edited prompts belong to the user. Preserve them
  on upgrades. Inspect changes before invoking credential deployment scripts.
- Keep UI changes native to Qt Quick, theme-aware and usable by touch in both
  orientations. Preserve vertical news scrolling and the circular page order.
- Use local mocks for voice tests. Enabling Assistant alone must not create a
  cloud conversation; do not start paid API/microphone tests without authorization.
- Run checks appropriate to the change. Module/config changes need the focused
  module tests; voice changes need mocked Realtime/Live tests. See docs/DEVELOPMENT.md.
- Our original code is MIT. Imported code/artwork retains its own licence and
  attribution; see THIRD_PARTY.md. Do not relabel imported material as ours.
- Export a public release with tools/export-public.py, not the private working
  directory or its history. Run tools/check-public.py on the actual export and
  review every included binary asset before publishing.

Detailed guides live in docs/. Keep README concise: description, screenshots,
copyable starting prompt and links. Use demonstration data for public screenshots.
