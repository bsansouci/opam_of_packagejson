module StringComp = struct
  type t = string
  let compare str1 str2 = String.compare str1 str2
end

include Map.Make(StringComp)

