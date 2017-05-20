opam\_of\_packagejson
---

This is a simple executable that takes in a package.json and outputs the `opam` file, the `META` file and the `.install` file depending on the options passed.

You can simply depend on it via npm. Add this to your `package.json`:
```
"opam_of_packagejson": "bsansouci/opam_of_packagejson"
```

Or depend on it on opam:
```
depends: [
  "opam_of_packagejson" { build & >= "0.1.2" }
]
```

To run you can just do add an npm script that runs `opam_of_packagejson -gen-meta -gen-install -gen-opam package.json`.


Here's a full package.json with all of the possible options commented:

```js
{
  /* `name` is used for the name field in the `opam` file.
     It's also used to determine the name of the main library file exposed for the `META` file and the `.install` file.
   */
  "name": "imitable-re",
  /* The `version` is used for the `.install` file mainly */
  "version": "0.0.15",
  /* `description` is used for the `.install` file */
  "description": "Small library that anyone could write",
  "main": "lib/js/src/immutable.js",
  /* the `homepage` field in the `opam` file. If not provided we'll use the url provided in the `repository` field. */
  "homepage" : "https://facebookincubator.github.io/immutable-re/",
  /* the `bug-reports` field in the `opam` file. */
  "bugs": "https://github.com/facebookincubator/immutable-re/issues",
  /* the `tags` field in the `opam` file. */
  "keywords": [
    "reason",
    "reasonml",
    "ocaml",
    "immutable"
  ],
  /* the `maintainer` field of the `opam` file. 
     If the `contributors` field isn't present in the package.json, we'll use the `author` field to fill its place. */
  "author": {
    "name": "Dave Bordoley",
    "email": "bordoley@gmail.com"
  },
  /* the `authors` field in the `opam` file */
  "contributors": [{ "name": "Dave Bordoley", "email": "bordoley@gmail.com" }],
  /* the `license` field in the `opam` file. */
  "license": "BSD-3-Clause",
  /* the `dev-repo` field mainly, but can be used for the `homepage` and the `bug-reports` fields too if their respective fields are missing from the package.json. */
  "repository": { "type": "git", "url" : "https://github.com/facebookincubator/immutable-re.git" },
  "dependencies": {
    // All of the deps!
  },
  "devDependencies": {
    "opam_of_packagejson": "*"
  },
  /* A bunch of properties only for `opam_of_packagejson`'s sake */
  "opam": {
    /* the `depends` field. Look at https://opam.ocaml.org/doc/1.1/Packaging.html#Versionconstraints for help on how the constrain solver works */  
    "dependencies": {
      "reason": "build & >=  \"1.13.3\"",
       "opam_of_packagejson": "build & >= \"0.1.2\""
    },
    /* path where the library artifacts (`.cma`/`cmxa`) are built */
    "installPath": "_build/src",
    /* Alternate control over the library name on opam */
    "libraryName": "immutable",
    /* main module name to determine the name of the main build artifact to expose */
    "mainModule": "Immutable",
    /* The type of the code, either a library or a binary */
    "type": "library"
  }
}
``` 

