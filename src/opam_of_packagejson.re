module NativeIo: Io.t = {
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
  type processStatus =
    | WEXITED int
    | WSIGNALED int
    | WSTOPPED int;
  let exec cmd => {
    let (ic, oc) = Unix.open_process cmd;
    let buf = Buffer.create 64;
    try (
      while true {
        Buffer.add_channel buf ic 1
      }
    ) {
    | End_of_file => ()
    };
    /* I'm so confused. Why is this needed?
         I tried to do `type processStatus = Unix.process_status` above and got:

          Error: Signature mismatch:
           ...
           Type declarations do not match:
             type processStatus = Unix.process_status
           is not included in
             type processStatus =
                 WEXITED of int
               | WSIGNALED of int
               | WSTOPPED of int
           File "src/opam_of_packagejson.re", line 13, characters 2-42:
             Actual declaration
           Their kinds differ.

        Not sure what this means.
       */
    let err =
      switch (Unix.close_process (ic, oc)) {
      | Unix.WEXITED c => WEXITED c
      | Unix.WSIGNALED c => WSIGNALED c
      | Unix.WSTOPPED c => WSTOPPED c
      };
    (err, Buffer.contents buf)
  };
};

module M = Index.Run NativeIo;

M.start ();
