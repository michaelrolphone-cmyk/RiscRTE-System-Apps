# Wi-Fi Settings 1.1.1: large Nova controls

The actual Settings palette, typography and shared rounded buttons are retained.
The list now has two54px cards. Every character and keyboard action has a49×44
hit rectangle. All95 printable ASCII characters remain reachable in12 pages,
starting with lowercase letters. Prev/Next, Delete and Done have distinct buttons;
Back cancels the editor. Entries, passwords, limits, masking, credentials and
explicit Save/Forget behavior remain controller-owned and unchanged. Long help
values wrap, and any title abbreviation is marked with an ellipsis.

[Actual after pages](screens/wifi-after.png) · [Before](screens/before.png).
These are production-source RGB565 renders with fake peripherals, not physical
qualification or final Xtensa ELF execution.

Validation:384 app/controller/adapter scenarios (48×2 UI profiles×2 touch
orientations×normal/ASan+UBSan), plus existing credential-interruption and saved
network suites. Coverage includes all95 characters,32/63-character limits,
masked entry, nested Back, held input, Save/Forget, scan/connect/cancel, radio
cleanup failure, alarm interruptions and retained sleep. Pinned GCC8.4 Nova
Wi-Fi ELF passes structural/import/export checks. No real credentials, RF,
network changes, merge, release, or device writes.

## Stable Watch 1.0.1 keyboard preservation

Wi-Fi 1.1.2 retains the exact Points/Spectrum standard 32-key, three-page Watch keyboard while keeping the enlarged Nova non-keyboard pages. Both UI profiles use the same bounded printable-ASCII mapping, ABC/#, DELETE, DONE ordering and hit cells. Credential masking, 32/63-byte limits, transactional Back, explicit Save and uncertain cleanup are unchanged. The real app/adapter regression covers all 95 characters, three-page cycling and every key hit in both orientations with sanitizers. Update apps have no text-entry keyboard. The final Points app uses its identical standard helper; its obsolete eight-key header is unused.
