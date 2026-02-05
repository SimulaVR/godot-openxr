{ pkgs ? import <nixpkgs> {}
, godot ? null
}:

let
  godot-path = if godot != null then godot else builtins.throw "godot source must be provided";

in
pkgs.stdenv.mkDerivation {
  pname = "godot-openxr";
  version = "0.0.0-dev";
  
  src = ./.;

  nativeBuildInputs = [
    pkgs.scons
    pkgs.pkg-config
    pkgs.python3
  ];

  buildInputs = [
    pkgs.openxr-loader
    pkgs.libglvnd
    pkgs.xorg.libX11
  ];

  # We need to manually generate bindings because the SCons build expects them
  preBuild = ''
    set -e
    # Generate godot-cpp bindings
    mkdir -p thirdparty/godot-cpp/include/gen
    mkdir -p thirdparty/godot-cpp/src/gen
    
    echo "Copying api.json from provided Godot source..."
    cp ${godot-path}/api.json thirdparty/godot-cpp/godot-headers/api.json
    
    # Ensure gdnative_api.json is present (it is part of the godot-cpp submodule usually)
    if [ ! -f thirdparty/godot-cpp/godot-headers/gdnative_api.json ]; then
        echo "Warning: gdnative_api.json not found in godot-headers."
    fi
    
    echo "Patching and running binding generator..."
    cd thirdparty/godot-cpp
    
    # The binding_generator.py in this version needs to be called explicitly
    # We append the main execution block if it's missing (which it seems to be for this version)
    if ! grep -q 'if __name__ == "__main__":' binding_generator.py; then
      cat <<EOF >> binding_generator.py

if __name__ == "__main__":
    import sys
    # args: api.json output_dir
    generate_bindings(sys.argv[1], True, sys.argv[2])
EOF
    fi

    python3 binding_generator.py godot-headers/api.json .
    cd ../..
    
    echo "Bindings generated."
    
    # Explicitly build godot-cpp first
    echo "Building godot-cpp..."
    cd thirdparty/godot-cpp
    # We use same flags as godot-openxr will use
    scons platform=linux target=release generate_bindings=no
    cd ../..
  '';

  sconsFlags = [
    "platform=linux"
    "target=release"
    "godot_path=${godot-path}"
    "CXXFLAGS=-std=gnu++17" 
  ];

  installPhase = ''
    mkdir -p $out/bin/linux
    # SCons outputs to demo/addons/...
    cp demo/addons/godot-openxr/bin/linux/libgodot_openxr.so $out/bin/linux/
  '';
}
