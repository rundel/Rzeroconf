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