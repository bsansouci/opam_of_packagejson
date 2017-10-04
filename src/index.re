module Run
       (
         Io: {
           type err;
           let readJSONFile: string => (Json_types.t => unit) => unit;
           let writeFile: string => string => unit;
         }
       ) => {
  let start () => {
    let (+|+) = Filename.concat;
    let should_gen_install = ref false;
    let should_gen_meta = ref false;
    let should_gen_opam = ref false;
    let destination = ref ".";
    let batch_files = ref [];
    let collect_files filename => batch_files := [filename, ...!batch_files];
    let usage = "Usage: opam_of_packagejson.exe [options] package.json";
    Arg.parse
      [
        (
          "-destination",
          Arg.String (fun str => destination := str),
          "Path to where the files will be generated."
        ),
        (
          "-gen-install",
          Arg.Set should_gen_install,
          "Generates a <projectName>.install file used by opam to know how to install your library."
        ),
        (
          "-gen-meta",
          Arg.Set should_gen_meta,
          "Generates a META file used by ocamlfind to know where your library is installed after being downloaded from opam."
        ),
        (
          "-gen-opam",
          Arg.Set should_gen_opam,
          "Generates an opam file used by opam to know how to publish your library."
        )
      ]
      collect_files
      usage;

    /** @GoodDefaults If there are no arguments passed we'll do the right thing ! */
    switch Sys.argv {
    | [|_|] =>
      should_gen_install := true;
      should_gen_meta := true;
      should_gen_opam := true
    | _ => ()
    };

    /** Double check that the directory given is all good. */
    if (not (!destination == ".")) {
      switch (Sys.is_directory !destination) {
      | exception (Sys_error _) =>
        failwith @@ "Direction passed '" ^ !destination ^ "' couldn't be found."
      | false => failwith @@ "Direction passed '" ^ !destination ^ "' isn't a directory."
      | true => ()
      }
    };

    /** Check the packagejson file */
    let packagejson =
      switch !batch_files {
      | [] => "package.json" /* good defaults baby */
      | [packagejson, ..._] => packagejson
      };
    Io.readJSONFile
      packagejson
      (
        fun json => {
          open Json_types;
          if !should_gen_opam {
            let b = Buffer.create 1024;
            let pr b fmt => Printf.bprintf b fmt;
            let pr_field b key value => pr b "%s: \"%s\"\n" key value;
            let (||>>) m field =>
              switch (StringMap.find field m) {
              | exception Not_found => m
              | Str {str} =>
                pr_field b field str;
                m
              | _ => assert false
              };
            switch json {
            | Obj {map} =>
              /** Yup hardcoding the version number */
              pr b "opam-version: \"1.2\"\n";
              let opamMap =
                switch (StringMap.find "opam" map) {
                | exception Not_found => None
                | Obj {map: innerMap} => Some innerMap
                | _ => assert false
                };
              switch opamMap {
              | Some opamMap =>
                let name =
                  switch (StringMap.find "libraryName" opamMap) {
                  | exception Not_found =>
                    switch (StringMap.find "name" map) {
                    | exception Not_found => failwith "Field `name` doesn't exist."
                    | Str {str} => str
                    | _ => failwith "Field `name` doesn't exist."
                    }
                  | Str {str} => str
                  | _ => failwith "Field `libraryName` isn't a string."
                  };
                pr b "name: \"%s\"\n" name
              | _ => ignore (map ||>> "name")
              };

              /** Print the simple fields first */
              ignore @@ (map ||>> "version" ||>> "license" ||>> "tags");
              let homepage_found =
                switch (StringMap.find "homepage" map) {
                | exception Not_found => false
                | Str {str} =>
                  pr b "homepage: \"%s\"\n" str;
                  true
                | Obj {map: innerMap} =>
                  let url =
                    switch (StringMap.find "url" innerMap) {
                    | Str {str} => str
                    | exception Not_found => failwith "Field `url` not found inside `homepage`"
                    | _ => failwith "Field `url` should be a string."
                    };
                  pr b "homepage: \"%s\"\n" url;
                  true
                | _ => failwith "Field `homepage` wasn't a string."
                };
              let bugs_found =
                switch (StringMap.find "bugs" map) {
                | exception Not_found => false
                | Str {str} =>
                  pr b "bug-reports: \"%s\"\n" str;
                  true
                | Obj {map: innerMap} =>
                  let url =
                    switch (StringMap.find "url" innerMap) {
                    | Str {str} => str
                    | exception Not_found => failwith "Field `url` not found inside `bugs`"
                    | _ => failwith "Field `url` should be a string."
                    };
                  pr b "bug-reports: \"%s\"\n" url;
                  true
                | _ => failwith "Field `bugs` wasn't a string."
                };

              /** Example:
                     "repository": {
                        "type": "git",
                        "url" : "https://github.com/facebookincubator/immutable-re"
                      },
                  */
              switch (StringMap.find "repository" map) {
              | Obj {map: innerMap} =>
                let url =
                  switch (StringMap.find "url" innerMap) {
                  | Str {str} => str
                  | exception Not_found => failwith "Field `url` not found inside `repository`"
                  | _ => failwith "Field `url` should be a string."
                  };
                let t =
                  switch (StringMap.find "type" innerMap) {
                  | Str {str} => str
                  | exception Not_found => failwith "Field `type` not found inside `repository`"
                  | _ => failwith "Field `type` should be a string."
                  };
                let ending = ref true;
                for i in 0 to (String.length t - 1) {
                  let j = String.length url - 1 - i;
                  if (t.[String.length t - 1 - i] != url.[j]) {
                    ending := false
                  }
                };
                let homepage =
                  if (not !ending) {
                    pr b "dev-repo: \"%s.%s\"\n" url t;
                    url
                  } else {
                    pr b "dev-repo: \"%s\"\n" url;
                    /* Remove the end and the dot. */
                    String.sub url 0 (String.length url - String.length t - 1)
                  };
                if (not homepage_found) {
                  pr b "homepage: \"%s\"\n" homepage
                };
                if (not bugs_found) {
                  pr b "bug-reports: \"%s/issues\"\n" homepage
                }
              | exception Not_found => failwith "Field `repository` not found."
              | _ => failwith "Field `repository` should be an object with a `type` and a `url`."
              };

              /** Contributors */
              let printAuthors key =>
                switch (StringMap.find key map) {
                | exception Not_found => false
                | Arr {content} =>
                  pr b "authors: [\n";
                  Array.iter
                    (
                      fun x =>
                        switch x {
                        | Str {str} => pr b "  \"%s\"\n" str
                        | Obj {map: innerMap} =>
                          let name =
                            switch (StringMap.find "name" innerMap) {
                            | exception Not_found =>
                              failwith @@ Printf.sprintf "Field `name` inside `%s` not found." key
                            | Str {str} => str
                            | _ => failwith "Field `name` should have type string."
                            };
                          let email =
                            switch (StringMap.find "email" innerMap) {
                            | exception Not_found => ""
                            | Str {str} => str
                            | _ => failwith "Field `email` should have type string."
                            };
                          pr b "  \"%s <%s>\"\n" name email
                        | _ =>
                          failwith @@
                          Printf.sprintf
                            "Field `%s` should be an array of strings or objects with a `name` and `email` fields."
                            key
                        }
                    )
                    content;
                  pr b "]\n";
                  true
                | Str {str} =>
                  pr b "authors: [ \"%s\" ]\n" str;
                  true
                | Obj {map: innerMap} =>
                  let name =
                    switch (StringMap.find "name" innerMap) {
                    | exception Not_found =>
                      failwith @@ Printf.sprintf "Field `name` inside `%s` not found." key
                    | Str {str} => str
                    | _ => failwith "Field `name` should have type string."
                    };
                  let email =
                    switch (StringMap.find "email" innerMap) {
                    | exception Not_found => ""
                    | Str {str} => str
                    | _ => failwith "Field `email` should have type string."
                    };
                  pr b "authors: [ \"%s <%s>\" ]\n" name email;
                  true
                | _ =>
                  failwith @@
                  Printf.sprintf
                    "Field `%s` should be a string or an object containing the fields `name` and `email`."
                    key
                };
              let hasContributors = printAuthors "contributors";
              let hasContributors = not hasContributors && printAuthors "author";
              let hasMaintainers =
                if (not hasContributors) {
                  let key = "maintainers";
                  switch (StringMap.find key map) {
                  | exception Not_found => false
                  | Arr {content: [||]} =>
                    failwith "Empty array found for field `maintainers` please add something in there!"
                  | Arr {content} =>
                    switch content.(0) {
                    | Str {str} => pr b "maintainer: \"%s\"\n" str
                    | Obj {map: innerMap} =>
                      let name =
                        switch (StringMap.find "name" innerMap) {
                        | exception Not_found =>
                          failwith @@ Printf.sprintf "Field `name` inside `%s` not found." key
                        | Str {str} => str
                        | _ => failwith "Field `name` should have type string."
                        };
                      let email =
                        switch (StringMap.find "email" innerMap) {
                        | exception Not_found =>
                          failwith @@ Printf.sprintf "Field `email` inside `%s` not found." key
                        | Str {str} => str
                        | _ => failwith "Field `email` should have type string."
                        };
                      pr b "maintainer: \"%s <%s>\"\n" name email
                    | _ =>
                      failwith @@
                      Printf.sprintf
                        "Field `%s` should be an array of strings or objects with a `name` and `email` fields."
                        key
                    };
                    true
                  | _ =>
                    failwith @@
                    Printf.sprintf
                      "Field `%s` should be a string or an object containing the fields `name` and `email`."
                      key
                  }
                } else {
                  false
                };

              /** `author` can either be "author": "Benjamin San Souci <benjamin.sansouci@gmail.com>" or
                  "author": {
                    "name": "Benjamin San Souci",
                    "email": "benjamin.sansouci@gmail.com"
                  }

                  Same for contributor.
                  */
              if (not hasMaintainers) {
                switch (StringMap.find "author" map) {
                | exception Not_found => failwith "Field `author` not found but required."
                | Str {str} =>
                  pr b "maintainer: \"%s\"\n" str;
                  if (not hasContributors && not hasMaintainers) {
                    pr b "authors: [ \"%s\" ]\n" str
                  }
                | Obj {map: innerMap} =>
                  let name =
                    switch (StringMap.find "name" innerMap) {
                    | exception Not_found => failwith "Field `name` inside `author` not found."
                    | Str {str} => str
                    | _ => failwith "Field `name` should have type string."
                    };
                  let email =
                    switch (StringMap.find "email" innerMap) {
                    | exception Not_found =>
                      hasContributors || hasMaintainers ?
                        "" : failwith "Field `email` inside `author` not found."
                    | Str {str} => str
                    | _ => failwith "Field `email` should have type string."
                    };
                  pr b "maintainer: \"%s <%s>\"\n" name email;
                  if (not hasContributors && not hasMaintainers) {
                    pr b "authors: [ \"%s <%s>\" ]\n" name email
                  }
                | _ =>
                  failwith "Field `author` should be a string or an object containing the fields `name` and `email`."
                }
              };

              /** Array fields */
              switch (StringMap.find "keywords" map) {
              | exception Not_found => ()
              | Arr {content} =>
                pr b "tags: [";
                Array.iter
                  (
                    fun x =>
                      switch x {
                      | Str {str} => pr b " \"%s\"" str
                      | _ => assert false
                      }
                  )
                  content;
                pr b " ]\n"
              | _ => assert false
              };

              /** Dependencies */
              let defaultBuildCommand = "[ \"make build\" ]";
              let buildCommand =
                switch opamMap {
                | Some opamMap =>
                  switch (StringMap.find "dependencies" opamMap) {
                  | exception Not_found => ()
                  | Obj {map: innerMap} =>
                    pr b "depends: [\n";
                    StringMap.iter
                      (
                        fun key v =>
                          switch v {
                          | Str {str} => pr b "  \"%s\" { %s }\n" key str
                          | _ => assert false
                          }
                      )
                      innerMap;
                    pr b "]\n"
                  | _ => ()
                  };
                  switch (StringMap.find "buildCommand" opamMap) {
                  | exception Not_found =>
                    switch (StringMap.find "scripts" map) {
                    | exception Not_found => defaultBuildCommand
                    | Obj {map: innerMap} =>
                      switch (StringMap.find "postinstall" innerMap) {
                      | exception Not_found => defaultBuildCommand
                      | Str {str} => Printf.sprintf "[ \"%s\" ]" str
                      | _ => failwith "Field `postinstall` under `scripts` was not a string."
                      }
                    | _ => failwith "Field `scripts` was not an object."
                    }
                  | Str {str} => Printf.sprintf "[ \"%s\" ]" str
                  | _ => failwith "Field `buildCommand` only supports a string for now. See "
                  }
                | None => defaultBuildCommand
                };

              /** Build command */
              pr b "build: [\n";
              pr b "  %s\n" buildCommand;
              pr b "]\n";
              let ocamlVersion =
                switch opamMap {
                | Some opamMap =>
                  switch (StringMap.find "ocamlVersion" opamMap) {
                  | exception Not_found => "ocaml-version >= \"4.02\" & ocaml-version < \"4.05\""
                  | Str {str} => str
                  | _ => failwith "Field `ocamlVersion` isn't a string."
                  }
                | _ => "ocaml-version >= \"4.02\" & ocaml-version < \"4.05\""
                };
              pr b "available: [ %s ]\n" ocamlVersion
            | _ => assert false
            };
            Io.writeFile (!destination +|+ "opam") (Buffer.contents b)
          };
          if !should_gen_install {
            let default_extensions = [".cmo", ".cmx", ".cmi", ".o", ".cma", ".cmxa", ".a"];
            switch json {
            | Obj {map} =>
              let (libraryName, path, mainModule, installType, installedBinaries) =
                switch (StringMap.find "opam" map) {
                | exception Not_found =>
                  let name =
                    switch (StringMap.find "name" map) {
                    | exception Not_found => failwith "Field `name` didn't exist."
                    | Str {str} => str
                    | _ => failwith "Field `name` didn't exist."
                    };
                  (
                    name,
                    "_build" +|+ "src",
                    name,
                    `Lib,
                    [
                      Install.(
                        `Bin,
                        {
                          src: "_build" +|+ "src" +|+ (name ^ ".native"),
                          dst: Some (name ^ ".exe"),
                          maybe: false
                        }
                      )
                    ]
                  )
                | Obj {map: innerMap} =>
                  let libraryName =
                    switch (StringMap.find "libraryName" innerMap) {
                    | exception Not_found =>
                      switch (StringMap.find "name" map) {
                      | exception Not_found => failwith "Field `name` didn't exist."
                      | Str {str} => str
                      | _ => failwith "Field `name` didn't exist."
                      }
                    | Str {str} => str
                    | _ =>
                      failwith "Field `libraryName` inside field `opam` wasn't a simple string."
                    };
                  let path =
                    switch (StringMap.find "installPath" innerMap) {
                    /* Default install path is _build/src */
                    | exception Not_found => "_build" +|+ "src"
                    | Str {str} => str
                    | _ => failwith "Couldn't find a `installPath` field or isn't a simple string."
                    };
                  let installType =
                    switch (StringMap.find "type" innerMap) {
                    | exception Not_found => `Lib
                    | Str {str} when str == "binary" => `Bin
                    | Str {str} when str == "library" => `Lib
                    | _ => failwith "Field `type` should be either \"binary\" or \"library\"."
                    };
                  let mainModule =
                    switch (StringMap.find "mainModule" innerMap) {
                    | exception Not_found =>
                      switch (StringMap.find "name" map) {
                      | exception Not_found => failwith "Field `name` doesn't exist."
                      | Str {str} => str
                      | _ => failwith "Field `name` isn't a simple string."
                      }
                    | Str {str} => str
                    | _ => failwith "Field `mainModule` inside field `opam` isn't a simple string."
                    };
                  let installedBinaries =
                    switch (StringMap.find "binaries" innerMap) {
                    | exception Not_found => [
                        Install.(
                          `Bin,
                          {
                            src: path +|+ (mainModule ^ ".native"),
                            dst: Some (mainModule ^ ".exe"),
                            maybe: false
                          }
                        )
                      ]
                    | Obj {map: innerMap} =>
                      StringMap.fold
                        (
                          fun srcPath execName acc =>
                            switch execName {
                            | Str {str: execName} => [
                                Install.(`Bin, {src: srcPath, dst: Some execName, maybe: false}),
                                ...acc
                              ]
                            | _ =>
                              failwith @@
                              Printf.sprintf
                                "The value of the key `%s` inside `binaries` should be a string."
                                srcPath
                            }
                        )
                        innerMap
                        []
                    | _ =>
                      failwith "Field `binaries` should be an object with {'path/to/binary': 'binaryName' }"
                    };
                  (libraryName, path, mainModule, installType, installedBinaries)
                | _ => failwith "Field `opam` was not an object."
                };
              let rest =
                switch installType {
                | `Lib =>
                  List.map
                    Install.(
                      fun str => (
                        `Lib,
                        {
                          src: path +|+ (mainModule ^ str),
                          dst: Some (mainModule ^ str),
                          maybe: false
                        }
                      )
                    )
                    default_extensions
                | `Bin => installedBinaries
                };

              /** Generate the .install file */
              let thing =
                Install.(
                  `Header (Some libraryName),
                  [
                    (`Lib, {src: !destination +|+ "opam", dst: Some "opam", maybe: false}),
                    (`Lib, {src: !destination +|+ "META", dst: Some "META", maybe: false}),
                    ...rest
                  ]
                );
              Io.writeFile
                (!destination +|+ (libraryName ^ ".install"))
                (Buffer.contents (Install.to_buffer thing))
            | _ => assert false
            }
          };
          if !should_gen_meta {
            let pr b fmt => Printf.bprintf b fmt;
            switch json {
            | Obj {map} =>
              let mainModule =
                switch (StringMap.find "opam" map) {
                | exception Not_found =>
                  switch (StringMap.find "name" map) {
                  | exception Not_found => failwith "Field `name` doesn't exist."
                  | Str {str} => str
                  | _ => failwith "Field `name` isn't a simple string."
                  }
                | Obj {map: innerMap} =>
                  switch (StringMap.find "mainModule" innerMap) {
                  | exception Not_found =>
                    switch (StringMap.find "name" map) {
                    | exception Not_found => failwith "Field `name` doesn't exist."
                    | Str {str} => str
                    | _ => failwith "Field `name` isn't a simple string."
                    }
                  | Str {str} => str
                  | _ =>
                    failwith "Couldn't find a `mainModule` field inside `opam` or isn't a simple string"
                  }
                | _ => assert false
                };
              let metab = Buffer.create 1024;
              let version =
                switch (StringMap.find "version" map) {
                | exception Not_found => failwith "Field `version` doesn't exist. "
                | Str {str: version} => version
                | _ => failwith "Field `version` was not a simple string."
                };
              let description =
                switch (StringMap.find "description" map) {
                | exception Not_found => failwith "Field `description` doesn't exist. "
                | Str {str: description} => description
                | _ => failwith "Field `description` was not a simple string."
                };
              pr metab "version = \"%s\"\n" version;
              pr metab "description = \"%s\"\n\n" description;
              pr metab "archive(byte) = \"%s.cma\"\n" mainModule;
              pr metab "archive(native) = \"%s.cmxa\"\n" mainModule;
              Io.writeFile (!destination +|+ "META") (Buffer.contents metab)
            | _ => assert false
            }
          }
        }
      )
  };
};
