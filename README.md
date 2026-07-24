# Planet Wars C Engine & Visualizer

A lightweight, high-performance, cross-platform engine and visualizer written in C for Planet Wars, according to the **Google Planet Wars 2010 AI Challenge** specification with few additions. 

This project simulates multi-bot space strategy battles in real time, executing bot binaries as asynchronous child processes over standard IPC, tracking the simulation log, and rendering the battle using [raylib](https://www.raylib.com/).

![Planet Wars Engine Demo](assets/demo.gif)

## Getting Started

### Prerequisites

* A standard C compiler (`gcc` or `clang`)
* Raylib dependencies. Make sure to read raylibs compilation page before this. You can also download a raylib release and place the `libraylib.a` file inside the `build` folder.

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

Upon a successful build, the compiled executable `main` will be generated in the root directory.

Run the binary to launch the engine and visualizer:

```bash
./main <map_file> <bot1> <bot2>...
```

#### Keyboard controls

- **right/left** - jump one turn forewards/backwards
- **space** - pause/unpause replay

## License

Distributed under the GNU AFFERO License. See the `LICENSE` file for more information.
