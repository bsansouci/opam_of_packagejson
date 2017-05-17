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
    let usage = "Usage: opam_of_pkgjson.exe [options] package.json";
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
      | [] => failwith "Please call opam_of_pkgjson.exe with a json file to convert to opam."
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
              | exception _ => m
              | Str {str} =>
                pr_field b field str;
                m
              | _ => assert false
              };
            let pr_field_custom m field newName =>
              switch (StringMap.find field m) {
              | exception _ => ()
              | Str {str} => pr_field b newName str
              | _ => assert false
              };
            switch json {
            | Obj {map} =>
              /** Yup hardcoding the version number */
              pr b "opam-version: \"1.2\"\n";
              let opamMap =
                switch (StringMap.find "opam" map) {
                | exception _ => None
                | Obj {map: innerMap} => Some innerMap
                | _ => assert false
                };
              switch opamMap {
              | Some opamMap => pr_field_custom opamMap "libraryName" "name"
              | _ => ignore @@ (map ||>> "name")
              };

              /** Print the simple fields first */
              ignore @@ (map ||>> "version" ||>> "license" ||>> "tags" ||>> "homepage");

              /** Slightly less simple since they require a different name */
              pr_field_custom map "bugs" "bug-reports";

              /** Example:
                     "repository": {
                        "type": "git",
                        "url" : "https://github.com/facebookincubator/immutable-re.git"
                      },
                  */
              switch (StringMap.find "repository" map) {
              | Obj {map: innerMap} =>
                let url =
                  switch (StringMap.find "url" innerMap) {
                  | Str {str} => str
                  | exception _ => failwith "Field `url` not found inside `repository`"
                  | _ => failwith "Field `url` should be a string."
                  };
                pr b "dev-repo: \"%s\"\n" url
              | exception _ => failwith "Field `repository` not found."
              | _ => failwith "Field `repository` should be an object with a `type` and a `url`."
              };

              /** Contributors */
              let hasContributors =
                switch (StringMap.find "contributors" map) {
                | exception _ => false
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
                            | exception _ =>
                              failwith "Field `name` inside `contributor` not found."
                            | Str {str} => str
                            | _ => failwith "Field `name` should have type string."
                            };
                          let email =
                            switch (StringMap.find "email" innerMap) {
                            | exception _ =>
                              failwith "Field `email` inside `contributor` not found."
                            | Str {str} => str
                            | _ => failwith "Field `email` should have type string."
                            };
                          pr b "  \"%s <%s>\"\n" name email
                        | _ =>
                          failwith "Field `contributor` should be an array of strings or objects with a `name` and `email` fields."
                        }
                    )
                    content;
                  pr b "]\n";
                  true
                | _ =>
                  failwith "Field `contributor` should be a string or an object containing the fields `name` and `email`."
                };

              /** `author` can either be "author": "Benjamin San Souci <benjamin.sansouci@gmail.com>" or
                  "author": {
                    "name": "Benjamin San Souci",
                    "email": "benjamin.sansouci@gmail.com"
                  }

                  Same for contributor.
                  */
              switch (StringMap.find "author" map) {
              | exception _ => failwith "Field `author` not found but required."
              | Str {str} =>
                pr b "maintainer: \"%s\"\n" str;
                if (not hasContributors) {
                  pr b "authors: [ \"%s\" ]\n" str
                }
              | Obj {map: innerMap} =>
                let name =
                  switch (StringMap.find "name" innerMap) {
                  | exception _ => failwith "Field `name` inside `author` not found."
                  | Str {str} => str
                  | _ => failwith "Field `name` should have type string."
                  };
                let email =
                  switch (StringMap.find "email" innerMap) {
                  | exception _ => failwith "Field `email` inside `author` not found."
                  | Str {str} => str
                  | _ => failwith "Field `email` should have type string."
                  };
                pr b "maintainer: \"%s <%s>\"\n" name email;
                if (not hasContributors) {
                  pr b "authors: [ \"%s <%s>\" ]\n" name email
                }
              | _ =>
                failwith "Field `author` should be a string or an object containing the fields `name` and `email`."
              };

              /** Array fields */
              switch (StringMap.find "keywords" map) {
              | exception _ => ()
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
              switch opamMap {
              | Some opamMap =>
                switch (StringMap.find "dependencies" opamMap) {
                | exception _ => ()
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
                | _ => assert false
                }
              | None => ()
              };

              /** Build command */
              pr b "build: [\n";
              pr b "  [ make \"build\" ]\n";
              pr b "]\n";

              /** Ocaml version hardcoded for now */
              pr b "available: [ ocaml-version >= \"4.02\" & ocaml-version < \"4.05\" ]\n"
            | _ => assert false
            };
            Io.writeFile (!destination +|+ "opam") (Buffer.contents b)
          };
          if !should_gen_install {
            let default_extensions = [".cmo", ".cmx", ".cmi", ".o", ".cma", ".cmxa", ".a"];
            switch json {
            | Obj {map} =>
              let (libraryName, path, mainModule, installType) =
                switch (StringMap.find "opam" map) {
                | exception _ =>
                  let name =
                    switch (StringMap.find "name" map) {
                    | exception _ => failwith "Field `name` didn't exist."
                    | Str {str} => str
                    | _ => failwith "Field `name` didn't exist."
                    };
                  (name, "_build" +|+ "src", name, `Lib)
                | Obj {map: innerMap} =>
                  let libraryName =
                    switch (StringMap.find "libraryName" innerMap) {
                    | exception _ =>
                      switch (StringMap.find "name" map) {
                      | exception _ => failwith "Field `name` didn't exist."
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
                    | exception _ => "_build" +|+ "src"
                    | Str {str} => str
                    | _ => failwith "Couldn't find a `installPath` field or isn't a simple string."
                    };
                  let installType =
                    switch (StringMap.find "type" innerMap) {
                    | exception _ => `Lib
                    | Str {str} when str == "binary" => `Bin
                    | Str {str} when str == "library" => `Lib
                    | _ => failwith "Field `type` should be either \"binary\" or \"library\"."
                    };
                  let mainModule =
                    switch (StringMap.find "mainModule" innerMap) {
                    | exception _ =>
                      switch (StringMap.find "name" map) {
                      | exception _ => failwith "Field `name` doesn't exist."
                      | Str {str} => str
                      | _ => failwith "Field `name` isn't a simple string."
                      }
                    | Str {str} => str
                    | _ => failwith "Field `mainModule` inside field `opam` isn't a simple string."
                    };
                  (libraryName, path, mainModule, installType)
                | _ => assert false
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
                | `Bin => [
                    Install.(
                      `Bin,
                      {
                        src: path +|+ (mainModule ^ ".native"),
                        dst: Some (mainModule ^ ".exe"),
                        maybe: false
                      }
                    )
                  ]
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
                | exception _ =>
                  switch (StringMap.find "name" map) {
                  | exception _ => failwith "Field `name` doesn't exist."
                  | Str {str} => str
                  | _ => failwith "Field `name` isn't a simple string."
                  }
                | Obj {map: innerMap} =>
                  switch (StringMap.find "mainModule" innerMap) {
                  | exception _ =>
                    switch (StringMap.find "name" map) {
                    | exception _ => failwith "Field `name` doesn't exist."
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
                | exception _ => failwith "Field `version` doesn't exist. "
                | Str {str: version} => version
                | _ => failwith "Field `version` was not a simple string."
                };
              let description =
                switch (StringMap.find "description" map) {
                | exception _ => failwith "Field `description` doesn't exist. "
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
