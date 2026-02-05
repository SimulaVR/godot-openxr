# Changes to `godot-openxr` Submodule

This document details the modifications made to this `godot-openxr` submodule to enable reproducible builds within the Nix environment for SimulaVR. The original upstream repository (version for Godot 3.x) suffered from C++ standard incompatibilities and build system assumptions that broke in our environment.

## 1. Patched `SConstruct` for Modern GCC

**The Problem:**
The original build script forced the use of `c++0x` (C++11). Modern GCC versions (like GCC 13/14 used in our Nixpkgs) combined with the C++ code in `godot-openxr` caused compilation errors, particularly regarding "designated initializers" (e.g., `.field = value` syntax in structs). While standard in C99, this syntax is restricted in C++ standards prior to C++20, and even then, GCC's pedantic implementation caused failures when mixing designated and non-designated initializers in aggregate types.

**The Fix:**
We replaced `-std=c++0x` with `-std=gnu++17` in the `SConstruct` file.
*   **Why `gnu++17`?** C++17 provides better support for modern language features used in the codebase. The `gnu` prefix (as opposed to strict `c++`) enables GNU extensions, which allows GCC to be more lenient with designated initializers in C++ structs, mimicking the behavior of C99 that the original authors likely relied upon with older compilers.

**Git Diff Chunk:**
```python
# In SConstruct
- env.Append(CXXFLAGS=["-std=c++0x"])
+ env.Append(CXXFLAGS=["-std=gnu++17"])
```

## 2. Patched `src/openxr/OpenXRApi.cpp` for C++ Compliance

**The Problem:**
Even with `gnu++17`, GCC was rejecting the initialization of the `XrApplicationInfo` struct inside `OpenXRApi.cpp`. The code used designated initializers (`.applicationName = ...`) mixed with implicit initialization of other fields (like `.type`). GCC 13+ is stricter about this, throwing:
`error: C99 designator ‘applicationName’ outside aggregate initializer`.

This happens because the code attempts to initialize a nested struct inside `XrInstanceCreateInfo` in a way that modern C++ compilers deem non-aggregate or unordered.

**The Fix:**
We removed the explicit field designators (e.g., `.applicationName =`) for the `XrApplicationInfo` struct initialization in `initialiseInstance()`.
*   **Why:** By removing the designators, we revert to standard *positional initialization*. Since `XrApplicationInfo` is a standard OpenXR C struct, the order of fields is fixed and guaranteed. Providing the values in the correct order without the `.field =` syntax is universally valid C++ and accepted by all compilers.

**Git Diff Chunk:**
```cpp
// In src/openxr/OpenXRApi.cpp (around line 1680)

// Before:
// .applicationName = "Godot OpenXR Plugin",
// .applicationVersion = 1,
// .engineName = "Godot Game Engine",
// .engineVersion = 1,
// .apiVersion = XR_CURRENT_API_VERSION

// After (conceptual):
/* .applicationName = */ "Godot OpenXR Plugin",
/* .applicationVersion = */ 1,
/* .engineName = */ "Godot Game Engine",
/* .engineVersion = */ 1,
/* .apiVersion = */ XR_CURRENT_API_VERSION
```

## 3. Added `default.nix` for Standalone Building

**The Problem:**
This repository is a raw source tree. It doesn't know how to build itself in a Nix environment. Specifically:
1.  It depends on `godot-cpp` (a submodule), which in turn depends on generated bindings.
2.  The binding generator script in `godot-cpp` was buggy/incomplete in this version (it defined a function `generate_bindings` but never called it when executed).
3.  It requires Godot engine headers (`api.json`) to generate those bindings.

**The Fix:**
We added a `default.nix` file that orchestrates the entire build process:
1.  **Dependencies:** It pulls in `scons`, `python3`, `openxr-loader`, `libglvnd`, and `libX11`.
2.  **API JSON:** It expects the path to the Godot engine source (to find `api.json`) via the `--arg godot` parameter.
3.  **Binding Generation:**
    *   It copies `api.json` into the `godot-cpp` submodule structure.
    *   It *patches* the `binding_generator.py` script on the fly to actually execute its main function (appending `if __name__ == "__main__": ...`).
    *   It runs the generator to populate `include/gen` and `src/gen`.
4.  **Compiling Dependencies:** It explicitly builds the `godot-cpp` library using SCons *before* building `godot-openxr`.
5.  **Installation:** It installs the resulting `libgodot_openxr.so` to `$out/bin/linux`.

### Why `--arg godot ./submodules/godot`?

The `godot-openxr` library is a GDExtension (or GDNative in Godot 3). It needs to know the exact API of the Godot engine it is running against. This API is defined in `api.json`.

*   **Ideally:** This file would be bundled with `godot-cpp` or `godot-openxr`.
*   **Reality:** In this version, `godot-cpp` expects you to provide the `api.json` from your target engine version.

We require the user (or the parent Nix expression) to provide the path to the Godot source code so we can extract `api.json`.

**What happens if we don't supply the `godot` arg?**
The build will fail immediately with:
`error: godot source must be provided` (thrown by our `default.nix`).
If we didn't have this check, the `preBuild` script would fail when trying to `cp .../api.json`, or the binding generator would fail, or (worst case) it would build against a default/stale API version and crash at runtime.

## 4. Integration with Simula's `flake.nix`

Currently, the main Simula `flake.nix` contains a monolithic definition for `godot-openxr` that manually patches the code and runs the build commands inline.

**Current (Monolithic) State:**
The `flake.nix` currently:
1.  Defines a `godot-openxr` package using `stdenv.mkDerivation`.
2.  Manually lists all `nativeBuildInputs` (scons, python, etc.).
3.  Contains a large `preBuild` script that:
    *   Copies `api.json`.
    *   Patches `binding_generator.py`.
    *   Patches `SConstruct`.
    *   Patches `OpenXRApi.cpp` with `sed`.
    *   Builds `godot-cpp`.

**Proposed (Modular) State:**
We can now simplify the main `flake.nix` drastically by delegating all this complexity to the submodule's new `default.nix`.

The new `flake.nix` entry for `godot-openxr` would look like this:

```nix
godot-openxr = pkgs.callPackage ./submodules/godot-openxr/default.nix {
  godot = inputs.godot;
};
```

**Benefits:**
1.  **Cleaner Flake:** The root `flake.nix` is no longer cluttered with C++ patch logic.
2.  **Separation of Concerns:** The `godot-openxr` submodule "knows how to build itself".
3.  **Reusability:** You can build `godot-openxr` independently (e.g., `nix-build submodules/godot-openxr/default.nix ...`) for debugging without building the entire Simula project.
