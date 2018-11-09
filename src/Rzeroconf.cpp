#include <Rcpp.h>

#include <dns_sd.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>

// [[Rcpp::plugins(cpp14)]]

class resolver {
  DNSServiceRef client;
  std::mutex data;
  
public:
  Rcpp::List txt_record;
  std::string hostname;
  std::string ip;
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
                            const char *fullname, const char *hosttarget, uint16_t opaqueport, 
                            uint16_t txtLen, const unsigned char *txtRecord, void *context)
  {
    resolver* res = static_cast<resolver*>(context);
    
    if (error_code == kDNSServiceErr_NoSuchRecord) {
      throw std::runtime_error("No Such Record");
    } else if (error_code) {
      std::stringstream s;
      s << "Error code: " << error_code;
      throw std::runtime_error(s.str());
    }
    
    std::lock_guard<std::mutex> guard(res->data);
    
    union { uint16_t s; u_char b[2]; } port = { opaqueport };
    res->port = ((uint16_t)port.b[0]) << 8 | port.b[1];
    
    res->flags = flags;
    res->iface = iface;
    res->hostname = hosttarget;
    //res->txt_record = txt_record_to_vector(txtLen, txtRecord);
    res->txt_record = Rcpp::CharacterVector();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //res->ip = get_addr_info(iface, hosttarget).ip;
    
    hostent* h = gethostbyname(hosttarget);
    if (h != NULL) {
      res->ip = inet_ntoa(*(in_addr*) h->h_addr_list[0]);
    } else {
      res->ip = Rcpp::String(NA_STRING);
    }
  }
  
  
  // Based on https://github.com/pocoproject/poco-dnssd/blob/master/Bonjour/src/BonjourBrowserImpl.cpp
  static Rcpp::CharacterVector txt_record_to_vector(uint16_t txtLen, const void *txtRecord) {
    Rcpp::CharacterVector keys;
    Rcpp::CharacterVector values;
    
    uint16_t n = TXTRecordGetCount(txtLen, txtRecord);
    Rcpp::Rcout << "Found n=" << n << "\n";
    
    for(uint16_t i = 0; i < n; ++i) {
      Rcpp::Rcout << "i=" << i << "\n";
      
      char key_buffer[256];
      char *value_ptr;
      uint8_t value_length;
      
      DNSServiceErrorType result = TXTRecordGetItemAtIndex(
        txtLen, txtRecord, i, sizeof(key_buffer), key_buffer, 
        &value_length, (const void**) &value_ptr
      );
      
      if (result != kDNSServiceErr_NoError) {
        Rcpp::warning("Invalid data in txt record");
        continue;
      }
      
      std::string key = key_buffer;
      //std::string value = std::string(value_ptr, value_length);
        
      //keys.push_back(key);
      //values.push_back("test");
      //values.push_back( std::string( );
      
      Rcpp::Rcout << "key    : " << key << "\n" 
                  << "val_len: " << (unsigned) value_length << "\n"
                  << "value  : " << value_ptr << "\n";
    }
    
    //values.names() = keys;
    return values;
  }
  
  //static Rcpp::List txt_record_to_string(uint16_t txtLen, const unsigned char *txtRecord)
  //{
  //  std::stringstream s;
  //  Rcpp::CharacterVector names;
  //  Rcpp::List values;
  //  
  //  
  //  const unsigned char *ptr = txtRecord;
  //  const unsigned char *max = txtRecord + txtLen;
  //  
  //  while (ptr < max) {
  //    const unsigned char *const end = ptr + 1 + ptr[0];
  //    if (end > max) { 
  //      throw std::runtime_error("Invalid data in txt record"); 
  //    }
  //    ptr++;
  //    
  //    s.str(std::string());
  //    
  //    bool found_name = false;
  //    while (ptr<end) {
  //      if (*ptr == '=') {
  //        found+name = true;
  //        names.push_back(s.str()); 
  //        s.str(std::string());     // Reset stringstream     
  //      } else {
  //        s << *ptr; 
  //      }
  //      
  //      ptr++;
  //    }
  //    values.push_back(s.str());
  //    if (!found_name)
  //  }
  //  
  //  values.names() = names;
  //  return values;
  //}
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
  std::list<std::string> ip;
  std::list<int>         port;
  Rcpp::List             txt_record;
   
public:
  zc_browser(std::string const& type, std::string const& domain = "local") 
  : browse_type(type),
    browse_domain(domain),
    stop_thread(false),
    poll_thread()
  { 
    DNSServiceBrowse(&client, 0, 0, browse_type.c_str(), browse_domain.c_str(), browse_reply, this);
    poll_thread = std::thread(&zc_browser::poll_dnssd, this);
  }
  
  ~zc_browser() {
    if (client) 
      DNSServiceRefDeallocate(client);
    
    stop_thread = true;
    poll_thread.join();
  }
  
  
  
  void poll_dnssd() {

    int dns_sd_fd = DNSServiceRefSockFD(client);
    int nfds      = dns_sd_fd + 1;
    fd_set readfds;
    timeval tv;
    
    while (!stop_thread) {
      FD_ZERO(&readfds);
      FD_SET(dns_sd_fd, &readfds);
      
      tv.tv_sec  = 1;
      tv.tv_usec = 0;
      
      int result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
      if (result == 1) {
        if (FD_ISSET(dns_sd_fd, &readfds)) {
          DNSServiceProcessResult(client);
        }
      }
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
Rcpp::List browse(std::string const& type = "_http._tcp", std::string const& domain = "local", unsigned wait=500)
{
  zc_browser b(type, domain);
  std::this_thread::sleep_for(std::chrono::milliseconds(wait));
  //b.update();
  
  return b.get_responses();
}

//' @export
// [[Rcpp::export]]
Rcpp::XPtr<zc_browser> create_browser(std::string const& type, std::string const& domain = "local") {
  return Rcpp::XPtr<zc_browser>(new zc_browser(type, domain), true);
}



//' @export
// [[Rcpp::export]]
Rcpp::DataFrame get_browser_results(Rcpp::XPtr<zc_browser> b) {
  return b->get_responses();
}


