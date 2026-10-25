# Iron Circuit config files

Edit these JSON files to change the player and enemies without recompiling.

- **player.json** — Player stats and program cards. `max_hp`, `attack_power`, `defense`, `max_heat`; `start.x`, `start.y`; `cards` array: each card has `name`, `description`, `move_type` (`"attack"`, `"move"`, `"special"`), `damage`, `range`.
- **enemies.json** — List of enemies in `enemies` array. Each has `x`, `y`, `max_hp`, `attack_power`, `behavior` (`"melee"`, `"ranged"`, `"boss"`). For `"ranged"` add `"range": 4` (or desired tile range).

Run the game from the project root (or from `Debug/`) so `resource/config/` is found.
