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
  | false =>
    failwith @@ "Direction passed '" ^ !destination ^ "' isn't a directory."
  | true => ()
  }
};


/** Check the packagejson file */
let packagejson =
  switch !batch_files {
  | [] =>
    failwith "Please call opam_of_pkgjson.exe with a json file to convert to opam."
  | [packagejson, ..._] =>
    if (not (Sys.file_exists packagejson)) {
      failwith @@ "Json file '" ^ packagejson ^ "' doesn't not found."
    } else {
      packagejson
    }
  };

let ic = open_in packagejson;

let json = Json.parse_json_from_chan ic;

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

    /** Print the simple fields first */
    ignore @@ (
      map ||>> "name" ||>> "version" ||>> "license" ||>> "tags" ||>> "homepage"
    );

    /** Slightly less simple since they require a different name */
    pr_field_custom map "url" "dev-repo";
    pr_field_custom map "bugs" "bugs-reports";
    pr_field_custom map "author" "maintainer";
    switch (StringMap.find "author" map) {
    | Str {str} =>
      pr b "authors: [\n";
      pr b "  \"%s\"\n" str;
      pr b "]\n"
    | exception _ => ()
    | _ => assert false
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
    switch (StringMap.find "opam" map) {
    | exception _ => ()
    | Obj {map: innerMap} =>
      switch (StringMap.find "dependencies" innerMap) {
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
    | _ => assert false
    };

    /** Build command */
    pr b "build: [\n";
    pr b "  [ make \"build\" ]\n";
    pr b "]\n";

    /** Ocaml version hardcoded for now */
    pr
      b "available: [ ocaml-version >= \"4.02\" & ocaml-version < \"4.05\" ]\n"
  | _ => assert false
  };
  let oc = open_out (!destination +|+ "opam");
  Buffer.output_buffer oc b
};

if !should_gen_install {
  let default_extensions = [
    ".cmo",
    ".cmx",
    ".cmi",
    ".cmt",
    ".o",
    ".cma",
    ".cmxa",
    ".a"
  ];
  switch json {
  | Obj {map} =>
    let libraryName =
      switch (StringMap.find "name" map) {
      | Str {str} => str
      | _ => failwith "Couldn't find a name field or isn't a simple string"
      };
    switch (StringMap.find "opam" map) {
    | exception _ => ()
    | Obj {map: innerMap} =>
      let path =
        switch (StringMap.find "installPath" innerMap) {
        | exception _ =>
          failwith "Couldn't find a `installPath` field or isn't a simple string"
        | Str {str} => str
        | _ =>
          failwith "Couldn't find a `installPath` field or isn't a simple string"
        };
      let mainModule =
        switch (StringMap.find "mainModule" innerMap) {
        | exception _ =>
          failwith "Couldn't find a `mainModule` field inside `opam` or isn't a simple string"
        | Str {str} => str
        | _ =>
          failwith "Couldn't find a `mainModule` field inside `opam` or isn't a simple string"
        };

      /** Generate the .install file */
      let thing =
        Install.(
          `Header (Some libraryName),
          [
            (
              `Lib,
              {src: !destination +|+ "opam", dst: Some "opam", maybe: false}
            ),
            (
              `Lib,
              {src: !destination +|+ "META", dst: Some "META", maybe: false}
            ),
            ...List.map
                 (
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
          ]
        );
      let oc = open_out (!destination +|+ (libraryName ^ ".install"));
      Buffer.output_buffer oc (Install.to_buffer thing)
    | _ => assert false
    }
  | _ => assert false
  }
};

if !should_gen_meta {
  let pr b fmt => Printf.bprintf b fmt;
  switch json {
  | Obj {map} =>
    switch (StringMap.find "opam" map) {
    | exception _ => ()
    | Obj {map: innerMap} =>
      let mainModule =
        switch (StringMap.find "mainModule" innerMap) {
        | exception _ =>
          failwith "Couldn't find a `mainModule` field inside `opam` or isn't a simple string"
        | Str {str} => str
        | _ =>
          failwith "Couldn't find a `mainModule` field inside `opam` or isn't a simple string"
        };
      let metab = Buffer.create 1024;
      switch (StringMap.find "version" map) {
      | Str {str: version} => pr metab "version = \"%s\"\n" version
      | _ => ()
      };
      switch (StringMap.find "description" map) {
      | Str {str: description} =>
        pr metab "description = \"%s\"\n\n" description
      | _ => ()
      };
      pr metab "archive(byte) = \"%s.cma\"\n" mainModule;
      pr metab "archive(native) = \"%s.cmxa\"\n" mainModule;
      let oc = open_out (!destination +|+ "META");
      Buffer.output_buffer oc metab
    | _ => assert false
    }
  | _ => assert false
  }
};
