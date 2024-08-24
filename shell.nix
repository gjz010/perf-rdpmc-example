let pkgs = import <nixpkgs> {};in
with pkgs;
let libpfm_latest = libpfm.overrideAttrs (final: prev: {
    version = "4.13.0-dirty";
    src = fetchgit {
        url = "git://git.code.sf.net/p/perfmon2/libpfm4";
        rev = "874ed7cff57271c5d4e530650eadce76e3dcaa14";
        sha256 = "qddYLKLbe9prqAWQXP1mlMydfhSRUJodhbBYtYihMT8=";
    };
});
in
mkShell{
    buildInputs = [linuxHeaders libpfm_latest ];
    nativeBuildInputs = [ clang-tools linuxKernel.packages.linux_6_6.perf];
}