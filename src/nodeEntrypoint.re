module M =
  Index.Run {
    type err;
    external readFile : string => _ [@bs.as "utf8"] => (err => string => unit) => unit =
      "readFile" [@@bs.module "fs"];
    let ifErrorThrow: err => unit = [%bs.raw {|function(err) { if (err) throw err; }|}];
    let readJSONFile f cb =>
      readFile
        f
        (
          fun e s => {
            ifErrorThrow e;
            cb (Json.parse_json_from_string s)
          }
        );
    external writeFile : string => string => unit = "writeFileSync" [@@bs.module "fs"];
  };

M.start ();
