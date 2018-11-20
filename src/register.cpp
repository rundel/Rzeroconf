// [[Rcpp::plugins(cpp14)]]
// [[Rcpp::depends(BH)]]

#include <Rcpp.h>

#include <dns_sd.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include <arpa/inet.h>

#include <boost/format.hpp>

#include "dns_sd_util.hpp"



class zc_register {
  DNSServiceRef client;
  std::mutex data;
  
  std::atomic<bool> stop_thread;
  std::thread       poll_thread;
   
public:
  
  zc_register( const zc_register& ) = delete; // non construction-copyable
  zc_register& operator=( const zc_register& ) = delete; // non copyable
  
  zc_register(
    std::string const& name, std::string const& type,
    std::string const& domain, uint16_t port,
    Rcpp::List txt_record_
  ) { 
    DNSServiceFlags flags = 0;
    uint32_t interfaceIndex = 0; // all
    
    const void* txt_record = NULL;
    uint16_t txt_len = 0;
    
    const char* host = NULL;
    
    DNSServiceRegister(&client, flags, interfaceIndex, 
                       name.c_str(), type.c_str(), domain.c_str(), host, 
                       htons(port),
                       txt_len, txt_record,
                       register_reply, this);
    
    poll_thread = std::thread(&zc_register::poll, this); 
  }
  
  ~zc_register() {
    stop_thread = true;
    if (poll_thread.joinable())
      poll_thread.join();
    
    if (client) 
      DNSServiceRefDeallocate(client);
  }
  
  
  
  void poll() {
    
    int dns_sd_fd = client ? DNSServiceRefSockFD(client) : -1;
    int nfds      = dns_sd_fd + 1;
    fd_set readfds;
    timeval tv;
    
    while (!stop_thread) {
      FD_ZERO(&readfds);
      if (client)
        FD_SET(dns_sd_fd, &readfds);
      
      tv.tv_sec  = 0;
      tv.tv_usec = 5000;
      
      if (select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv) > 0) {
        if (FD_ISSET(dns_sd_fd, &readfds)) {
          DNSServiceProcessResult(client);
        }
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }
  
private:
  static void register_reply(DNSServiceRef client, const DNSServiceFlags flags, 
                             DNSServiceErrorType error_code, 
                             const char* name, const char* type, const char* domain, 
                             void* context)
  {
    zc_register* b = static_cast<zc_register*>(context);
  
    std::lock_guard<std::mutex> guard(b->data);
    
    Rcpp::Rcout << "Registered: " << name << type << domain << "\n";
  }
};


//' @export
// [[Rcpp::export]]
Rcpp::XPtr<zc_register> register_service(
    std::string const& name, std::string const& type,
    std::string const& domain, uint16_t port,
    Rcpp::List txt_record
) {
  return Rcpp::XPtr<zc_register>(new zc_register(name, type, domain, port, txt_record), true);
}

