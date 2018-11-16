// [[Rcpp::plugins(cpp14)]]
// [[Rcpp::depends(BH)]]

#include <Rcpp.h>

#include <dns_sd.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <boost/format.hpp>

#include "dns_sd_util.hpp"




class resolver {
  DNSServiceRef client;
  std::mutex data;
  
public:
  Rcpp::CharacterVector txt_record;
  std::string hostname;
  //std::string ip;
  Rcpp::CharacterVector ip;
  uint32_t iface;
  uint16_t port;
  uint16_t uport; 
  DNSServiceFlags flags;
  
  resolver(uint32_t iface, std::string const& name, 
           std::string const& type, std::string const& domain, 
           unsigned wait = 50) 
    : resolver(iface, name.c_str(), type.c_str(), domain.c_str(), wait)
  { }
  
  resolver(uint32_t iface, const char* name, const char* type, const char* domain, unsigned wait = 50) {
    DNSServiceFlags flags = 0;
    DNSServiceErrorType err;
    
    err = DNSServiceResolve(&client, flags, iface, name, type, domain, resolve_reply, this);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait));
    err = DNSServiceProcessResult(client);
  }
  
  ~resolver() {
    if (client) 
      DNSServiceRefDeallocate(client);
  }
  
private:
  static void resolve_reply(DNSServiceRef sdref, const DNSServiceFlags flags, 
                            uint32_t iface, DNSServiceErrorType error_code,
                            const char *fullname, const char *hosttarget, uint16_t port, 
                            uint16_t txt_len, const unsigned char *txt_record, void *context)
  {
    resolver* res = static_cast<resolver*>(context);
    
    if (error_code != kDNSServiceErr_NoError) {
      Rcpp::warning(boost::str(
        boost::format("Error during resolve (%s)") % get_service_error_str(error_code)
      ));
    }
    
    std::lock_guard<std::mutex> guard(res->data);
    
    res->port = ntohs(port);
    res->flags = flags;
    res->iface = iface;
    res->hostname = hosttarget;
    res->txt_record = (txt_len == 0) ? Rcpp::CharacterVector() : txt_record_to_vector(txt_len, txt_record);
    res->ip = get_ip_address(hosttarget);
  }
  
  static Rcpp::CharacterVector get_ip_address(const char* hostname) {
    
    Rcpp::CharacterVector ips;
    
    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    addrinfo *addrs;
    if (getaddrinfo(hostname, NULL, &hints, &addrs) != 0) {
      throw std::runtime_error("opps");
    }
    
    
    for(addrinfo* p = addrs; p != NULL; p = p->ai_next) {
      void *ptr;
      char ip[INET6_ADDRSTRLEN];
      
      switch (p->ai_family) {
      case AF_INET:
        ptr = &((sockaddr_in  *) p->ai_addr)->sin_addr;
        break;
      case AF_INET6:
        ptr = &((sockaddr_in6 *) p->ai_addr)->sin6_addr;
        break;
      }
      
      if(inet_ntop(p->ai_family, ptr, ip, sizeof(ip)))
        ips.push_back(ip);
    }
    
    freeaddrinfo(addrs);
    return ips;
  }
  
  static Rcpp::CharacterVector txt_record_to_vector(uint16_t txt_len, const unsigned char *txt_record) {
    Rcpp::CharacterVector keys;
    Rcpp::CharacterVector values;
    
    uint8_t *p = (uint8_t*)txt_record;
    uint8_t *end = p + txt_len;
    
    while (p < end) {
      uint8_t *keyval_end = p + 1 + p[0];
      
      if (keyval_end > end) {
        Rcpp::warning("Bad txt record data");
        return Rcpp::CharacterVector();
      }
      
      unsigned long len = 0;
      uint8_t *x = p+1;
      
      while (x+len < keyval_end && x[len] != '=') len++;
      
      keys.push_back(std::string((const char *) x, len));
      if (x+len < keyval_end) { // Found an =
        values.push_back(std::string((const char *) x + len + 1, p[0] - (len + 1)));
      } else {
        values.push_back("");
      }
      
      p += 1 + p[0];
    }
    
    values.names() = keys;
    
    return values;
  }
};



class zc_browser {
  DNSServiceRef client;
  std::mutex data;
  
  std::atomic<bool> stop_thread;
  std::thread       poll_thread;
  
  std::string browse_type;
  std::string browse_domain;
  
  std::list<std::string> op;
  std::list<int>         iface;
  std::list<std::string> iface_name;
  std::list<std::string> name;
  std::list<std::string> type;
  std::list<std::string> domain;
  std::list<std::string> hostname;
  //std::list<std::string> ip;
  Rcpp::List             ip;
  std::list<int>         port;
  Rcpp::List             txt_record;
  
public:
  
  zc_browser( const zc_browser& ) = delete; // non construction-copyable
  zc_browser& operator=( const zc_browser& ) = delete; // non copyable
  
  zc_browser(std::string const& type, std::string const& domain = "local") 
    : browse_type(type),
      browse_domain(domain),
      stop_thread(false),
      poll_thread()
  { 
    DNSServiceErrorType err = DNSServiceBrowse(&client, 0, 0, browse_type.c_str(), browse_domain.c_str(), browse_reply, this);
    if (err != kDNSServiceErr_NoError) {
      throw std::runtime_error(
          boost::str( boost::format("Error creating browser (%s)") % get_service_error_str(err) )
      );
    }
    poll_thread = std::thread(&zc_browser::poll, this); 
  }
  
  ~zc_browser() {
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
  
  Rcpp::List get_responses() {
    
    std::lock_guard<std::mutex> guard(data);
    
    Rcpp::List d = Rcpp::List::create(
      Rcpp::Named("op")             = op,
      Rcpp::Named("interface")      = iface,
      Rcpp::Named("interface_name") = iface_name,
      Rcpp::Named("name")           = name,
      Rcpp::Named("type")           = type,
      Rcpp::Named("domain")         = domain,
      Rcpp::Named("hostname")       = hostname,
      Rcpp::Named("ip")             = ip,
      Rcpp::Named("port")           = port,
      Rcpp::Named("txt_record")     = txt_record
    );  
    
    std::vector<int> row_names(op.size());
    if (op.size() != 0)
      std::iota(row_names.begin(), row_names.end(), 1);
    
    d.attr("class") = Rcpp::CharacterVector::create("tbl_df", "tbl", "data.frame");
    d.attr("row.names") = row_names;
    
    return d;
  }
  
private:
  static void browse_reply(DNSServiceRef sdref, const DNSServiceFlags flags, 
                           uint32_t if_index, DNSServiceErrorType error_code,
                           const char* name, const char* type, const char* domain, void* context)
  {
    zc_browser* b = static_cast<zc_browser*>(context);
    
    bool add = (flags & kDNSServiceFlagsAdd);
    std::string op = add ? "Add" : "Remove";
    
    if (error_code) {
      Rcpp::Rcout << "Error code " << error_code << "\n";
      return;
    } 
    
    std::lock_guard<std::mutex> guard(b->data);
    
    char if_name[IF_NAMESIZE];
    if_indextoname(if_index, if_name);
    
    b->op.push_back(op);
    b->iface.push_back(if_index);
    b->iface_name.push_back(if_name);
    b->name.push_back(name); 
    b->type.push_back(type);
    b->domain.push_back(domain);
    
    resolver res(if_index, name, type, domain);
    b->hostname.push_back(res.hostname);
    b->ip.push_back(res.ip);
    b->port.push_back(res.port);
    b->txt_record.push_back(res.txt_record);
  }
};



//' @export
// [[Rcpp::export]]
Rcpp::XPtr<zc_browser> browse_service(std::string const& type, std::string const& domain = "local") {
  Rcpp::XPtr<zc_browser> p = Rcpp::XPtr<zc_browser>(new zc_browser(type, domain), true);
  p.attr("class") = "zc_browser";
  
  return p;
}



// [[Rcpp::export]]
Rcpp::DataFrame get_browser_results(Rcpp::XPtr<zc_browser> b) {
  return b->get_responses();
}


