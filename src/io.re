type err;

external readFile :
  string => ([ | `utf8] [@bs.string]) => (err => string => unit) => unit =
  "readFile" [@@bs.module "fs"];

let ifErrorThrow : err => unit = [%bs.raw {|function(err) {
  if (err) throw err;
}|}];

external writeFile : string => string => unit = "writeFile" [@@bs.module "fs"];
