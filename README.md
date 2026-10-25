# Note

This game was originally made during the Spring 2026 semester at William & Mary for the class CSCI 437: Game Design. The creators of this project are Alex Anderson, Aglae Hunter, Patrick Breslin, and Shraddha Anup. This project was made on William and Mary's gitlab server, so this copy is to allow public viewing. No changes have been made to the project besides this note.

# Building Iron Circuit

To build the game from source on a Debian/Ubuntu-based machine (or WSL):

## 1. Install Dependencies
Run the included setup script to install all necessary compiler toolchains and SDL2 development libraries. This will also pull down the `nlohmann_json` single-header library automatically.

```bash
chmod +x setup.sh
./setup.sh
```

### 1b. Alternative: Manual Installation
If you prefer not to execute a shell script or if your network environment requires you to run commands manually, you can execute the following steps locally in your terminal:

```bash
# 1. Update your package manager
sudo apt-get update

# 2. Install all necessary C++ compilation and SDL2 libraries
sudo apt-get install -y build-essential cmake wget libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev

# 3. Create the include directory for third-party files
mkdir -p include/nlohmann

# 4. Download the nlohmann_json single-header dependency
wget -qO include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
```

## 2. Compile the Game
Compile out-of-source using standard CMake instructions. The project is fully compatible with CMake 3.5 and above.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build
```

## 3. Run
Execute the compiled binary from the exact directory you launched the build:

```bash
./build/bin/iron_circuit
```
