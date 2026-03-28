{
  description = "LDD3 RULKC Edition development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
    nix-vm-test.url = "github:numtide/nix-vm-test";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      nix-vm-test,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            nix-vm-test.overlays.default
          ];
        };
        lib = pkgs.lib;

        makeTest =
          {
            distro,
            distroVersion,
            driverDir,
            testScript,
          }:
          nix-vm-test.lib.x86_64-linux.${distro}.${distroVersion} {
            sharedDirs = {
              ldd-dir = {
                source = "${./${driverDir}}";
                target = "/mnt/ldd3-rulkc";
              };
            };
            inherit testScript;
          };
      in
      {
        packages = {
          debian = builtins.foldl' lib.mergeAttrs { } (
            lib.forEach [ "12" "13" ] (distroVersion: {
              ${distroVersion} =
                (makeTest {
                  distro = "debian";
                  inherit distroVersion;
                  driverDir = "hello_world";
                  testScript = ''
                    vm.succeed("apt-get update")
                    vm.succeed("apt-get install -y build-essential linux-headers-amd64")
                    vm.succeed("cp -r /mnt/ldd3-rulkc/ ./")
                    vm.succeed("make -C ./ldd3-rulkc/")
                    vm.succeed("insmod ./ldd3-rulkc/hello_world.ko")
                    vm.succeed("rmmod hello_world")
                    vm.succeed("dmesg | grep 'Hello, world'")
                    vm.succeed("dmesg | grep 'Goodbye, cruel world'")
                  '';
                }).driver;
            })
          );
        };
      }
    );
}
