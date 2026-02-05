godot_path := env_var_or_default("GODOT_PATH", "../godot")

default:
    just --list

build:
    @echo "Building godot-openxr..."
    @echo "Using Godot path: {{godot_path}}"
    
    # Prepare godot-cpp
    mkdir -p thirdparty/godot-cpp/include/gen
    mkdir -p thirdparty/godot-cpp/src/gen
    mkdir -p thirdparty/godot-cpp/godot-headers
    
    # Copy api.json
    cp {{godot_path}}/api.json thirdparty/godot-cpp/godot-headers/api.json
    
    # Patch binding_generator.py if needed
    if ! grep -q 'if __name__ == "__main__":' thirdparty/godot-cpp/binding_generator.py; then \
        echo 'if __name__ == "__main__":' >> thirdparty/godot-cpp/binding_generator.py; \
        echo '    import sys' >> thirdparty/godot-cpp/binding_generator.py; \
        echo '    # args: api.json output_dir' >> thirdparty/godot-cpp/binding_generator.py; \
        echo '    generate_bindings(sys.argv[1], True, sys.argv[2])' >> thirdparty/godot-cpp/binding_generator.py; \
    fi
    
    # Generate bindings
    cd thirdparty/godot-cpp && python3 binding_generator.py godot-headers/api.json .
    
    # Build godot-cpp
    cd thirdparty/godot-cpp && scons platform=linux target=release generate_bindings=no -j$(nproc)
    
    # Build godot-openxr
    scons platform=linux target=release godot_path={{godot_path}} CXXFLAGS=-std=gnu++17 -j$(nproc)

clean:
    scons -c
    cd thirdparty/godot-cpp && scons -c

