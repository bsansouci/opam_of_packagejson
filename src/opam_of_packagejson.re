module M =
  Index.Run {
    type err;
    let readJSONFile f cb => {
      let ic = open_in f;
      cb (Json.parse_json_from_chan ic);
      close_in ic
    };
    let writeFile f contents => {
      let oc = open_out f;
      output_string oc contents;
      close_out oc
    };
  };

M.start ();
