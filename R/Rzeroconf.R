#' libgeos
#'
#' @import Rcpp
#'
#' @useDynLib Rzeroconf, .registration = TRUE
"_PACKAGE"

#' @export  
print.zc_browser = function(ptr) {
  print(get_browser_results(ptr))
}

#' @export  
print.avahi_browser = function(ptr) {
  l = avahi_get_results(ptr)
  l = lapply(l, as.data.frame, stringsAsFactors=FALSE)
  l = do.call(rbind, l)
  l = as_tibble(l)
  
  print(l)
}