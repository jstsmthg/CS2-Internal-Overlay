# Grenade Lineup Data

This folder stores per-map lineup files used by the in-game grenade helper.

Format:

```text
name;type;x;y;z;pitch;yaw
```

Types:

- `smoke`
- `flash`
- `molotov`
- `he`

Example:

```text
window_spawn_1;smoke;-812.500;456.125;32.000;-12.400;91.200
```

Recommended external sources to curate/import from:

- Stratbase: team stratbook + nade database
- quicknades: CS2 lineup app/web database
- jumpthrow.pro: automated pro-match nade library

Notes:

- The helper auto-loads `grenades\<current_map>.csv` using the map name read from CS2 GlobalVars.
- You can create new entries from the menu with `Save Current Position` while standing on the lineup spot and aiming correctly.
- Good workflow: use an external lineup source to find the setup, reproduce it in a practice server, then save the exact in-game position/angles into the local CSV so the helper uses coordinates verified in your own build and resolution setup.
