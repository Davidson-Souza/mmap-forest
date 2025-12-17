let
  pkgs = import <nixpkgs> {};

  # Use llvm env.
  stdenv = pkgs.llvmPackages_18.stdenv;

  # Needed for linking. Otherwise nix will use gnu binutils and will result in
  # a linking error.
  bintools = pkgs.llvmPackages_18.bintools;
in

with pkgs;
stdenv.mkDerivation rec {
  name = "bitcoin";
  myEnv = buildEnv { name = name; paths = buildInputs; };

  # Everything that goes in here is wrapped so that the libraries for these
  # are automatically linked.
  nativeBuildInputs = [
    # ld, ar and other stuff.
    bintools

    # For clangd linking.
    clang-tools

    # make stuff
    pkg-config
    util-linux
    cmake

    ccache
    openssl
  ];

  # anything that you'd `apt install` would go here.
  buildInputs = [
    # bitcoin dependencies
    boost
    zlib
    zeromq
    miniupnpc
    libevent
    sqlite

    # doxygen stuff
    doxygen
    graphviz

    # benchmarking stuff
    pkgs.python311Packages.pyperf
  ];

  # '$configureFlags' to access these in the shell.
  # You can add more flags here.
  configureFlags = [
  ];

  # These will be set in the shell.
  shellHook = ''
    # Just so that you can find the path for boost.  Not really needed.
    export BOOST_LIBDIR="${boost.out}/lib"
    export AR="llvm-ar"
    export LD="lld"
  '';
}
