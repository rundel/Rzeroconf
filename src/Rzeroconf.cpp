#include <Rcpp.h>
#include <dns_sd.h>
#include <thread>
#include <chrono>

#include <net/if.h>
#include <arpa/inet.h>

class get_addr_info {
  DNSServiceRef client;
  std::mutex data;
  
public:
  std::string hostname;
  std::string ip;
  
  get_addr_info(uint32_t iface, const char* hostname, bool ipv4 = true, unsigned wait = 50) {
    DNSServiceFlags flags = 0;
    DNSServiceErrorType err;
    
    DNSServiceProtocol protocol = ipv4 ? kDNSServiceProtocol_IPv4 : kDNSServiceProtocol_IPv6;
    
    err = DNSServiceGetAddrInfo(&client, flags, iface, protocol, hostname, get_addr_info_reply, this);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait));
    err = DNSServiceProcessResult(client);
  }
  
  ~get_addr_info() {
    if (client) 
      DNSServiceRefDeallocate(client);
  }
  
private:
  
  static void get_addr_info_reply
  (
      DNSServiceRef sdRef,
      const DNSServiceFlags flags,
      uint32_t iface,
      DNSServiceErrorType error_code,
      const char *hostname,
      const struct sockaddr *address,
      uint32_t ttl,
      void  *context
  ) {
    
    get_addr_info* g = static_cast<get_addr_info*>(context);
    
    if (error_code) {
      std::stringstream s;
      s << "Error code: " << error_code;
      throw std::runtime_error(s.str());
    }
    
    std::lock_guard<std::mutex> guard(g->data);
    
    g->hostname = hostname;
    g->ip = get_ip(address);
  }
  
  // Based on https://beej.us/guide/bgnet/html/multi/inet_ntopman.html
  static std::string get_ip(const sockaddr* sa)
  {
    char ip[INET6_ADDRSTRLEN];
    
    switch(sa->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET, &(((sockaddr_in *)sa)->sin_addr), ip, INET_ADDRSTRLEN);
      break;
      
    case AF_INET6:
      inet_ntop(AF_INET6, &(((sockaddr_in6 *)sa)->sin6_addr), ip, INET6_ADDRSTRLEN);
      break;
      
    default:
      throw std::runtime_error("Unknown AF family");
    }
    
    return std::string(ip);
  }
};



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
    if (txtLen > 1)
      res->txt_record = txt_record_to_string(txtLen, txtRecord);
    else
      res->txt_record = Rcpp::List();
    
    res->ip = get_addr_info(iface, hosttarget).ip;
  }
  
  static Rcpp::List txt_record_to_string(uint16_t txtLen, const unsigned char *txtRecord)
  {
    std::stringstream s;
    Rcpp::CharacterVector names;
    Rcpp::List values;
    
    
    const unsigned char *ptr = txtRecord;
    const unsigned char *max = txtRecord + txtLen;
    
    while (ptr < max) {
      const unsigned char *const end = ptr + 1 + ptr[0];
      if (end > max) { 
        throw std::runtime_error("Invalid data in txt record"); 
      }
      ptr++;
      
      s.str(std::string());
      
      while (ptr<end) {
        if (*ptr == '=') {
          names.push_back(s.str()); 
          s.str(std::string());     // Reset stringstream     
        } else {
          s << *ptr; 
        }
        
        ptr++;
      }
      values.push_back(s.str());
    }
    
    values.names() = names;
    return values;
  }
};

class browser {
  DNSServiceRef client;
  std::mutex data;
  
  Rcpp::IntegerVector   iface;
  Rcpp::CharacterVector iface_name;
  Rcpp::CharacterVector name;
  Rcpp::CharacterVector type;
  Rcpp::CharacterVector domain;
  Rcpp::CharacterVector hostname;
  Rcpp::CharacterVector ip;
  Rcpp::IntegerVector   port;
  Rcpp::List            txt_record;
  
public:
  browser() {
    
  }
  
  ~browser() {
    if (client) 
      DNSServiceRefDeallocate(client);
  }
  
  void update(std::string const& type, std::string const& domain, unsigned wait) {
    DNSServiceFlags flags = 0;
    DNSServiceErrorType err = kDNSServiceErr_NoError;
    err = DNSServiceBrowse(&client, flags, 0, type.c_str(), domain.c_str(), browse_reply, this);
    std::this_thread::sleep_for(std::chrono::milliseconds(wait));
    err = DNSServiceProcessResult(client);
  }
  
  Rcpp::DataFrame get_responses() {
    
    Rcpp::DataFrame df;
    
    df.push_back(iface);
    df.push_back(iface_name);
    df.push_back(name);
    df.push_back(type);
    df.push_back(domain);
    df.push_back(hostname);
    df.push_back(ip);
    df.push_back(port);
    df.push_back(txt_record);
    
    int n = iface.size();
    Rcpp::IntegerVector row_names(n);
    std::iota(row_names.begin(), row_names.end(), 1);
    
    df.names() = Rcpp::CharacterVector::create(
      "interface", "interface_name", 
      "name", "type", "domain", 
      "hostname", "ip", "port", 
      "txt_record"
    );
    df.attr("class") = Rcpp::CharacterVector::create("tbl_df", "tbl", "data.frame");
    df.attr("row.names") = row_names;
    
    return df;
  }
  
private:
  static void browse_reply(DNSServiceRef sdref, const DNSServiceFlags flags, 
                           uint32_t if_index, DNSServiceErrorType error_code,
                           const char* name, const char* type, const char* domain, void* context)
  {
    browser* b = static_cast<browser*>(context);
    
    bool add = (flags & kDNSServiceFlagsAdd);
    std::string op = add ? "Add" : "Remove";
    
    if (error_code) {
      Rcpp::Rcout << "Error code " << error_code << "\n";
      return;
    } 
    
    std::lock_guard<std::mutex> guard(b->data);
    
    char if_name[IF_NAMESIZE];
    if (if_indextoname(if_index, if_name) != NULL)
      b->iface_name.push_back(if_name);
    else
      b->iface_name.push_back(std::string());
    
    b->iface.push_back(if_index);
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
Rcpp::DataFrame browse(std::string const type = "_http._tcp", std::string const domain = "local", unsigned wait=500)
{
  browser b;
  b.update(type, domain, wait);
  
  return b.get_responses();
}

