// [[Rcpp::plugins(cpp14)]]
// [[Rcpp::depends(BH)]]

#include <Rcpp.h>

#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>


#include <boost/format.hpp>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <tuple>

class avahi_domain_browser {
  AvahiClient *client;
  AvahiDomainBrowser *browser;
  AvahiThreadedPoll *poll;
  
  std::string browse_domain;
  
public:
  
  avahi_domain_browser( const avahi_domain_browser& ) = delete; // non construction-copyable
  avahi_domain_browser& operator=( const avahi_domain_browser& ) = delete; // non copyable
  
  avahi_domain_browser(std::string const& type, std::string const& domain = "local") 
    : browse_domain(domain)
  { }
  
  ~avahi_domain_browser() {
    stop();
  }
  
  void start() {
    poll = avahi_threaded_poll_new();
    
    if (!poll) throw std::runtime_error("Failed to create simple poll object.");
    
    int error;
    client = avahi_client_new(avahi_threaded_poll_get(poll), (AvahiClientFlags) 0, 
                              client_callback, this, &error);
    
    if (!client) throw std::runtime_error("Failed to create client");

    browser = avahi_domain_browser_new(	
      client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
      browse_domain.c_str(), AVAHI_DOMAIN_BROWSER_BROWSE, (AvahiLookupFlags) 0,
      domain_browser_callback, this
    );
      
    if (!browser) throw std::runtime_error("Failed to create domain browser");
    
    avahi_threaded_poll_start(poll);
  }
  
  void stop() {
    avahi_threaded_poll_quit(poll);
    if (browser)
      avahi_domain_browser_free(browser);
    if (client)
      avahi_client_free(client);
    if (poll)
      avahi_threaded_poll_free(poll);
  }
  
private:
  
  static void client_callback(AvahiClient *c, AvahiClientState state, void* context) {
    switch (state) {
      case AVAHI_CLIENT_S_REGISTERING:
      case AVAHI_CLIENT_S_RUNNING:
      case AVAHI_CLIENT_S_COLLISION:
      case AVAHI_CLIENT_CONNECTING:
      case AVAHI_CLIENT_FAILURE:
        break;
    }
  }
  
  static void domain_browser_callback(
      AvahiDomainBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, 
      AvahiBrowserEvent event, const char *domain, 
      AvahiLookupResultFlags flags, void *context
  ) {
    //avahi_domain_browser* b = static_cast<avahi_domain_browser*>(context);
    
    switch (event) {
      case AVAHI_BROWSER_NEW:
        Rcpp::Rcout << "AVAHI_BROWSER_NEW: ";
        break;
      case AVAHI_BROWSER_REMOVE:
        Rcpp::Rcout << "AVAHI_BROWSER_REMOVE: ";
        break;
      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        Rcpp::Rcout << "AVAHI_BROWSER_CACHE_EXHAUSTED: ";
        break;
      case AVAHI_BROWSER_ALL_FOR_NOW:
        Rcpp::Rcout << "AVAHI_BROWSER_ALL_FOR_NOW: ";
        break;
      case AVAHI_BROWSER_FAILURE:
        Rcpp::Rcout << "AVAHI_BROWSER_FAILURE: ";
        break;
    }
    
    if (domain)
      Rcpp::Rcout << "domain=" << domain;
    
    Rcpp::Rcout << "\n";
  }
};


//' @export
// [[Rcpp::export]]
Rcpp::XPtr<avahi_domain_browser> avahi_browse_domain(std::string const& domain = "local") {
  Rcpp::XPtr<avahi_domain_browser> ptr = Rcpp::XPtr<avahi_domain_browser>(new avahi_domain_browser(domain), true);
  ptr->start();
  
  return ptr;
}
