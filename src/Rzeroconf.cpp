#include <Rcpp.h>
#include "servus.h"

void print_res (std::vector<std::string> const& v) {
  
  if (v.size() == 0) {
    Rcpp::Rcout << "<empty>";
  } else {
    for(auto const& e : v) {
      Rcpp::Rcout << e << " ";
    }
  }
  
  Rcpp::Rcout << "\n";
}

//' @export
// [[Rcpp::export]]
void discover(std::string name = "_http._tcp", unsigned time=5000) 
{
  servus::Servus service(name);
  print_res( service.discover(servus::Servus::IF_ALL, time) );
  print_res( service.getInstances() );
}

