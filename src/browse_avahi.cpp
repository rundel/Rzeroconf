// [[Rcpp::plugins(cpp14)]]
// [[Rcpp::depends(BH)]]

#include <Rcpp.h>

#include <mutex>

#include <boost/format.hpp>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <tuple>


typedef std::tuple<std::string, std::string, std::string> service;

class avahi_browser {
  AvahiClient *client;
  AvahiServiceBrowser *browser;
  AvahiThreadedPoll *poll;
  
  std::mutex data;
  
  std::string browse_type;
  std::string browse_domain;
  
  std::vector<std::string> errors;
  bool valid;
  
  std::map<service, Rcpp::List> results;

public:
  
  avahi_browser( const avahi_browser& ) = delete; // non construction-copyable
  avahi_browser& operator=( const avahi_browser& ) = delete; // non copyable
  
  avahi_browser(std::string const& type, std::string const& domain = "local") 
    : browse_type(type),
      browse_domain(domain),
      valid(true)
  { }
  
  ~avahi_browser() {
    stop();
  }
  
  void stop() {
    Rcpp::Rcout << "Calling destructor!\n";
    
    avahi_threaded_poll_quit(poll);
    if (browser)
      avahi_service_browser_free(browser);
    if (client)
      avahi_client_free(client);
    if (poll)
      avahi_threaded_poll_free(poll);
  }
  
  void start() {
    poll = avahi_threaded_poll_new();
    if (!poll)
      throw std::runtime_error("Failed to create simple poll object.");
    
    int error;
    client = avahi_client_new(avahi_threaded_poll_get(poll), (AvahiClientFlags) 0, client_callback, this, &error);
    
    if (!client)
      throw std::runtime_error("Failed to create client");
    
    browser = avahi_service_browser_new(
      client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
      browse_type.c_str(), browse_domain.c_str(), (AvahiLookupFlags) 0, 
      browser_callback, this
    );
    
    if (!browser) 
      throw std::runtime_error("Failed to create service browser");
    
    avahi_threaded_poll_start(poll);
  }
  
  Rcpp::List get_responses() {
    check_status();
    
    std::lock_guard<std::mutex> guard(data);
    
    Rcpp::List l;
    for (auto const& s : results) {
      l.push_back(s.second);
    }
  
    return l;
  }
  
private:
  
  void check_status() {
    if (valid)
      return;
    
    std::string errs;
    for(auto const& err : errors ) {
      errs += err + "\n";
    }
    
    throw std::runtime_error(errs);
  }

  static void client_callback(AvahiClient *c, AvahiClientState state, void* context) {
    //avahi_browser* b = static_cast<avahi_browser*>(context);
    //std::lock_guard<std::mutex> guard(b->data);
    
    switch (state) {
      case AVAHI_CLIENT_S_REGISTERING:
      case AVAHI_CLIENT_S_RUNNING:
      case AVAHI_CLIENT_S_COLLISION:
      case AVAHI_CLIENT_CONNECTING:
      case AVAHI_CLIENT_FAILURE:
        break;
    }
  }
  
  static void browser_callback(
      AvahiServiceBrowser *browser, AvahiIfIndex interface,
      AvahiProtocol protocol, AvahiBrowserEvent event,
      const char *name, const char *type, const char *domain,
      AvahiLookupResultFlags flags, void* context
  ) {
    avahi_browser* b = static_cast<avahi_browser*>(context);
 
    std::lock_guard<std::mutex> guard(b->data);
    
    switch (event) {
      case AVAHI_BROWSER_FAILURE: {
        std::string err = avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(browser)));
        b->errors.push_back("[AVAHI_BROWSER_FAILURE] " + err);
        b->valid = false;
        break;
      }
        
      case AVAHI_BROWSER_NEW: {
        avahi_service_resolver_new(
          b->client, interface, protocol, name, type, domain, 
          AVAHI_PROTO_UNSPEC, (AvahiLookupFlags) 0, resolve_callback, b
        );
        break;
      }
        
      case AVAHI_BROWSER_REMOVE: {
        service s = std::make_tuple(std::string(name), std::string(type), std::string(domain));
        b->results.erase(s);
        break;
      }
        
      case AVAHI_BROWSER_ALL_FOR_NOW:
      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        //if (b->poll)
        //  avahi_threaded_poll_stop(b->poll);
        break;
    }
  }
  
  static void resolve_callback(
      AvahiServiceResolver *r, AvahiIfIndex interface, 
      AvahiProtocol protocol, AvahiResolverEvent event,
      const char *name, const char *type, const char *domain,
      const char *host_name, const AvahiAddress *address,
      uint16_t port, AvahiStringList *txt,
      AvahiLookupResultFlags flags, void* context
  ) {
    avahi_browser* b = static_cast<avahi_browser*>(context);
    
    switch (event) {
      case AVAHI_RESOLVER_FAILURE: {
        std::string err = avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r)));
        b->errors.push_back("[AVAHI_RESOLVER_FAILURE] " + err);
        b->valid = false;
        break;
      }
        
      case AVAHI_RESOLVER_FOUND: {
        char addr[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addr, sizeof(addr), address);
        char *txt_ptr = avahi_string_list_to_string(txt);
        
        std::lock_guard<std::mutex> guard(b->data);
        
        std::string n(name);
        std::string t(type);
        std::string d(domain);
        
        service key = std::make_tuple(n, t, d);
        
        Rcpp::List res = Rcpp::List::create(
          Rcpp::Named("name")     = n,
          Rcpp::Named("type")     = t,
          Rcpp::Named("domain")   = d,
          Rcpp::Named("hostname") = std::string(host_name),
          Rcpp::Named("address")  = std::string(addr),
          Rcpp::Named("port")     = port,
          Rcpp::Named("txt")      = std::string(txt_ptr)
        );
        
        avahi_free(txt_ptr);
        
        b->results[key] = res;
      }
    }
    
  }
};


//' @export
// [[Rcpp::export]]
Rcpp::XPtr<avahi_browser> avahi_browse_service(std::string const& type, std::string const& domain = "local") {
  Rcpp::XPtr<avahi_browser> ptr = Rcpp::XPtr<avahi_browser>(new avahi_browser(type, domain), true);
  ptr->start();
  ptr.attr("class") = "avahi_browser";
  
  return ptr;
}

//' @export
// [[Rcpp::export]]
Rcpp::List avahi_get_results(Rcpp::XPtr<avahi_browser> ptr) {

  return ptr->get_responses();
}
