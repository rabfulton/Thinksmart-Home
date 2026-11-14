# Editing the assistant

This directory lives at /etc/thinksmart/assistant on the device.

- system.md: system and tool instructions for the Realtime models.
- live.md: GPT Live's conversation, interruption and delegation policies.
- backend.md: GPT Live's reasoning and tool procedures, used by Luna or the selected backend.
- household.md: shared facts about people, pronunciation and preferences.
- personalities/*.md: one selectable personality per file.

Each personality file’s filename, including `.md`, is its dropdown label. The filename
without .md is its saved ID: use letters, numbers, hyphens and underscores, with a
letter or number first (up to 64 characters). Add a new .md file to add a choice.
Do not rename the currently selected file without selecting another afterwards.

Realtime receives system.md + the selected personality + household.md.
GPT Live receives live.md + the selected personality + household.md. Its reasoning
backend receives backend.md + household.md, without the personality or system.md.
Put delivery and character in a personality; put device workflows in backend.md.
Changes to system.md therefore affect Realtime only. Live's tools and allowed
actions are still defined in code; editing these files does not add capabilities.

Keep the policy headings and delegation labels in live.md intact, including
`Generate <|SpawnThinking|> when:` and `Do not generate <|SpawnThinking|> when:`.
The detailed station IDs and radio discovery procedure belong in backend.md.

The applicable parts are sent to OpenAI as instructions at the next Start.
Edits are read automatically for each new conversation. Stop an active conversation
before trying changes. Settings → Reload configuration refreshes the dropdown
without connecting. No restart or build is required. Files must be UTF-8, at most
32 KiB each, with a limit of 64 KiB for each assembled model prompt, including
section headings. Live's two policy files must contain instructions. Missing or
invalid files block conversations for the model that uses them and show the
filename in Settings; fix the file and reload. Live-only file errors do not block
Realtime, and Realtime's system.md is not read for Live.

Example household notes (replace the placeholders with real facts):

    ## Preferred name
    - Pronunciation: ...
    - Likes: ...
    - Radio preferences: ...
    - Answers: short / detailed / ...

The assistant cannot identify people from their voices or write memories here.
A person can introduce themselves during a conversation; the assistant is told
to ask when it needs a name to apply a personal preference.

Files are initially private and owned by thinksmart. When adding a file as root,
make it readable by the application, for example:

    chown thinksmart:thinksmart /etc/thinksmart/assistant/personalities/my-prompt.md
    chmod 600 /etc/thinksmart/assistant/personalities/my-prompt.md

Deployment creates missing defaults only. Existing files are never overwritten.
README.md is for you and is not included in the assistant's instructions.

## Sheila wake word

When Assistant and its module are enabled, Sheila is detected locally, including
with the screen asleep and with shared-reference media playback. Detection needs
the separately installed wake/audio components. Turning Assistant off closes the
listener and any cloud conversation. Local standby audio is not uploaded or saved.

See WAKEWORD.md for detection and tuning, and docs/VOICE.md in the source project
for installation. The public source release supplies no personal wake recordings
or personalised model. Public setup can use the pinned phonetic fallback.
