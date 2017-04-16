opam\_of\_packagejson
---

This is a simple executable that takes in a package.json and outputs an `opam` file. It can optionally generate the `META` file and `.install` file needed.

You can simply depend on it via npm. Add this to your `package.json`:
```
"opam_of_packagejson": "bsansouci/opam_of_packagejson"
```

It'll need some help figure out the opam dependencies. Add this to your `package.json` if you need to depend on opam packages:
```
  "opam": {
    "dependencies": {
      "libThatYouDependOn": "{build >=  \"1.13.3\"}"
    },
  },
```

If you want to also generate the `META` and `.install` file, add this
```
  "opam": {
    "install": {
      "path": "_build/src",
      "extensions": [".cmo", ".cmx", ".cmi", ".cmt", ".o", ".cma", ".cmxa", ".a"]
    }
  },
```

This is roughly the same as what you'd expect to see inside a `pkg/pkg.ml` when using `topkg` to generate the `.install` file.
