# Planet Wars C Engine & Visualizer

A lightweight, high-performance, cross-platform engine and visualizer written in C for Planet Wars, according to the **Google Planet Wars 2010 AI Challenge** specification with some new additions.

This project simulates multi-bot space strategy battles in real time, executing bot binaries as asynchronous child processes over standard IPC, tracking the simulation log, and rendering the battle using [raylib](https://www.raylib.com/).

![Planet Wars Engine Demo](assets/demo.gif)

## Getting Started

### Prerequisites

* A standard C compiler (`gcc` or `clang`)
* Raylib dependencies. Make sure to read raylibs compilation page before this.
  If you don't want to compile raylib from scratch, you can also download a raylib release from [here](https://github.com/raysan5/raylib/releases) and place the `libraylib.a`(Linux) or `libraylib.lib`(windows) file inside the `build` folder.

### Building the Project

The project uses `nob.c` to compile dependencies, including Raylib, alongside the main target automatically.

1. Bootstrap the build system and compile the project:
```bash
gcc -o nob nob.c
./nob
```


2. Subsequent builds:
The build script supports auto-rebuilding via `NOB_GO_REBUILD_URSELF`. Execute `./nob` whenever modifications are made to `nob.c` or project source files.

## Running the Simulation

Upon a successful build, the compiled executable `planet_wars` will be generated in the root directory.

Run the binary to launch the engine and visualizer:

```bash
./planet_wars -help
```

#### Keyboard controls

- **right/left** - jump one turn forewards/backwards
- **up/down**    - increase/decrease replay speed
- **space**      - pause/unpause replay

## Configuration file

You can specify a config file by using the `-config` flag and providing a path to a `.ini` file.

This is an example config file with comments explaining everything:

```ini
[application]
write_log = true             ; Whether to write a log.txt file containing the log of the battle.
tournament = true            ; Whether to run a tournament between all supplied bots.
                             ; this requires more bots specified then the map requires.

[simulation]
map = map.txt                ; Path to the map file that will be used.

[bot]
name = Okay Bot              ; Name for the bot. Optional, but good for tournament mode.
command = python okay_bot.py ; Actuall command that will be invoked by the manager.

; Each bot can be defined in it's own section
[bot]
name = Worst bot
command = node worst_bot.js

[bot]
name = Best bot
command = ./best_bot
```

## License

Distributed under the GNU AFFERO License. See the `LICENSE` file for more information.
