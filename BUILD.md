# Build Instructions

## Windows Build (with vcpkg)

### Prerequisites
- **Visual Studio 2022** (Community Edition is fine)
- **CMake 3.27.1** or newer

### 1. Clone the repository with vcpkg submodule
- Recommended: `git clone --recursive <repo-url>`
- If you already cloned, run:
  ```sh
  git submodule init
  git submodule update
  ```

### 2. Bootstrap vcpkg and install dependencies
```sh
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install
```

### 3. Configure the project with CMake
```sh
cd ..
mkdir build
cd build
# You may need to provide the absolute path to the toolchain file
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
```
> **Tip:** If you get errors about not finding the toolchain file, try using the absolute path.

### 4. Build the executable
- Open `splatapult.sln` in Visual Studio and build, **or**
- From the command line:
  ```sh
  cmake --build . --config=Release
  ```

---

## Windows Shipping Builds

To create a release (shipping) build:
```sh
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DSHIPPING=ON
cmake --build . --config=Release
```
- The build will copy all required data folders (`font`, `shader`, `texture`) into the build directory.
- The resulting folder can be zipped and distributed to users.

---

## Linux Build (with vcpkg)

### Prerequisites
- `clang`
- `cmake`
- `freeglut3-dev`
- `libopenxr-dev`

Install dependencies:
```sh
sudo apt-get install clang cmake freeglut3-dev libopenxr-dev
```

### 1. Clone the repository with vcpkg submodule
- See Windows instructions above.

### 2. Bootstrap vcpkg
```sh
cd vcpkg
./bootstrap-vcpkg.sh
```

### 3. Configure and build
```sh
cd ..
mkdir build
cd build
cmake .. -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config=Release
```

---

## Meta Quest Build (Experimental, Out of Date)
> **Note:** The Quest build is experimental and slow for large scenes (max ~25k splats).

### Steps
1. Use vcpkg to install:
   ```sh
   vcpkg install glm:arm64-android libpng:arm64-android nlohmann-json:arm64-android
   ```
2. Set the `ANDROID_VCPKG_DIR` environment variable to point to `vcpkg/installed/arm64-android`.
3. Download the [Meta OpenXR Mobile SDK 59.0](https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/).
4. Install [Android Studio Bumble Bee, Patch 3](https://developer.android.com/studio/archive) (newer versions may not work).
   - Follow the [Meta guide](https://developer.oculus.com/documentation/native/android/mobile-studio-setup-android/).
5. Copy the `ovr_openxr_mobile_sdk_59.0` directory into `meta-quest`.
6. Copy `meta-quest/splatapult` to `ovr_openxr_mobile_sdk_59.0/XrSamples/splatapult`.
7. Open in Android Studio, sync, and build.

---

## Troubleshooting
- If CMake cannot find the vcpkg toolchain, use the **absolute path**.
- If dependencies are missing, ensure you ran `vcpkg install` after editing `vcpkg.json`.
- For Windows, always use forward slashes (`/`) in CMake paths.
- If you have multiple vcpkg folders, make sure you are using the correct one for your project.

---

Happy building!

