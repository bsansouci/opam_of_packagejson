type field =
[ `Bin
| `Doc
| `Etc
| `Lib
| `Lib_root
| `Libexec
| `Libexec_root
| `Man
| `Misc
| `Sbin
| `Share
| `Share_root
| `Stublibs
| `Toplevel
| `Unknown of string ]

let field_to_string = function
| `Bin -> "bin"
| `Doc -> "doc"
| `Etc -> "etc"
| `Lib -> "lib"
| `Lib_root -> "lib_root"
| `Libexec -> "libexec"
| `Libexec_root -> "libexec_root"
| `Man -> "man"
| `Misc -> "misc"
| `Sbin -> "sbin"
| `Share -> "share"
| `Share_root -> "share_root"
| `Stublibs -> "stublibs"
| `Toplevel -> "toplevel"
| `Unknown name -> name

type move = { src : string; dst : string option; maybe : bool; }

let move ?(maybe = false) ?dst src = { src; dst; maybe }

type t = [ `Header of string option ] * (field * move) list

let to_buffer (`Header h, mvs) =
  let b = Buffer.create 1024 in
  let pr b fmt = Printf.bprintf b fmt in
  let pr_header b = function None -> () | Some h -> pr b "# %s\n\n" h in
  let pr_src b src maybe =
    pr b "  \"%s%s\"" (if maybe then "?" else "") src
  in
  let pr_dst b dst = match dst with
  | None -> ()
  | Some dst -> pr b " {\"%s\"}" dst
  in
  let pr_field_end b last = if last <> "" (* not start *) then pr b " ]\n" in
  let pr_field b last field =
    if last = field then pr b "\n" else
    (pr_field_end b last; pr b "%s: [\n" field)
  in
  let pr_move b last (field, { src; dst; maybe }) =
    pr_field b last field;
    pr_src b src maybe;
    pr_dst b dst;
    field
  in
  let sortable (field, mv) = (field_to_string field, mv) in
  let moves = List.(sort compare (rev_map sortable mvs)) in
  pr_header b h;
  let last = List.fold_left (pr_move b) "" moves in
  pr_field_end b last;
  b
