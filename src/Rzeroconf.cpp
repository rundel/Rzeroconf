#include <Rcpp.h>


//' @export
// [[Rcpp::export]]
void discover(std::string name = "_http._tcp", unsigned time=5000) 
{
  servus::Servus service(name);
  print_res( service.discover(servus::Servus::IF_ALL, time) );
  print_res( service.getInstances() );
}

