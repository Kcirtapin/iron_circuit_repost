#!/bin/bash
# Iron Circuit - Setup Script for Debian/Ubuntu environments (should work with WSL)

echo "Welcome to the setup for compiling Iron Circuit! It should work on debian environments, tested on Ubuntu."
echo "[1/2] Installing system dependencies (SDL2, CMake, GCC)..."
echo "Note: certain WSL versions may have issues with apt-get update. If this gets stuck, either edit it out of the script or follow the manual installation."

sudo apt-get update
sudo apt-get install -y build-essential cmake wget \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    libsdl2-mixer-dev

echo "[2/2] Downloading single-header dependencies..."
mkdir -p include/nlohmann
wget -qO include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp

echo "Setup complete! You can now build the project."