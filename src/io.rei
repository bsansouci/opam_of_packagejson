module type t = {
  type err;
  let readJSONFile: string => (Json_types.t => unit) => unit;
  let writeFile: string => string => unit;
  type processStatus =
    | WEXITED int
    | WSIGNALED int
    | WSTOPPED int;
  let exec: string => (processStatus, string);
};
