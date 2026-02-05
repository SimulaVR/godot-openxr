{
  description = "Godot OpenXR plugin";

  nixConfig = {
    extra-substituters = [ "https://simula.cachix.org" ];
    extra-trusted-public-keys = [ "simula.cachix.org-1:Sr0SD5FIjc8cUVIeBHl8VJswQEJOBIE6u3wpmjslGBA=" ];
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    godot = {
      url = "github:SimulaVR/godot/3.x-simula";
      flake = false;
    };
    godot-cpp = {
      url = "git+https://github.com/godotengine/godot-cpp?submodules=1&rev=68ce78179f90be9bec8cc88106dba0c244bdc4f6";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, godot, godot-cpp }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      pkgsFor = system: nixpkgs.legacyPackages.${system};
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = (pkgs.callPackage ./default.nix {
            godot = godot;
          }).overrideAttrs (old: {
            preBuild = ''
              rm -rf thirdparty/godot-cpp
              cp -r --no-preserve=mode ${godot-cpp} thirdparty/godot-cpp
              chmod -R u+w thirdparty/godot-cpp
            '' + old.preBuild;
          });
        });

      devShells = forAllSystems (system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            nativeBuildInputs = [
              pkgs.scons
              pkgs.pkg-config
              pkgs.python3
              pkgs.just
            ];

            buildInputs = [
              pkgs.openxr-loader
              pkgs.libglvnd
              pkgs.xorg.libX11
            ];
          };
        });
    };
}