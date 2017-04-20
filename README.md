opam\_of\_packagejson
---

This is a simple executable that takes in a package.json and outputs an `opam` file. It can optionally generate the `META` file and `.install` file needed.

You can simply depend on it via npm. Add this to your `package.json`:
```
"opam_of_packagejson": "bsansouci/opam_of_packagejson"
```

To run you can just do add an npm script that runs `opam_of_packagejson -gen-meta packagejson`.

It'll need some help figure out the opam dependencies. Add this to your `package.json` if you need to depend on opam packages:
```
  "opam": {
    "dependencies": {
      "opamLibThatYouDependOn": "build >=  \"1.13.3\""
    },
  },
```

The constraints for the version numbers are opam constraints that you can read about [here](https://opam.ocaml.org/doc/1.1/Packaging.html#Versionconstraints). 

If you want to also generate the `META` and `.install` file, add this
```
  "opam": {
    "installPath": "_build/src"
  },
```
Though the default is `"_build/src"`

This is roughly the same as what you'd expect to see inside a `pkg/pkg.ml` when using `topkg` to generate the `.install` file.

You can add `-gen-install` and `-gen-meta` to the command to generate those.

You can add `libraryName` under `opam` to control the name of the opam version of your library. If not specified, the default is to use the package.json's `name`.

You can also add `mainModule` under `opam` to control the name of the main outputted library file when the library is built. If not specified, the default is to use the package.json's `name`.
