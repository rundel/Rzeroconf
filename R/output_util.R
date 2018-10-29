parse_dnssd = function(output, col_names) {
  if (is.null(output)) return(NULL)
  output = gsub(" +", "," ,output) 
  readr::read_csv(output, col_names = col_names)
}

parse_avahi = function(output, col_names) {
  if (is.null(output)) return(NULL)
  readr::read_delim(output, delim=";")
}

output_skip = function(output, n=0) {
  sub = seq_along(output) > n
  output[sub]
}

output_collapse = function(output) {
  paste(output, collapse="\n")
}
