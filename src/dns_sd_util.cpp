#include <string>
#include <dns_sd.h>

std::string get_service_error_str(DNSServiceErrorType err) {
  switch (err) {
  case kDNSServiceErr_NoError:
    return "kDNSServiceErr_NoError";
  case kDNSServiceErr_Unknown:
    return "kDNSServiceErr_Unknown";
  case kDNSServiceErr_NoSuchName:
    return "kDNSServiceErr_NoSuchName";
  case kDNSServiceErr_NoMemory:
    return "kDNSServiceErr_NoMemory";
  case kDNSServiceErr_BadParam:
    return "kDNSServiceErr_BadParam";
  case kDNSServiceErr_BadReference:
    return "kDNSServiceErr_BadReference";
  case kDNSServiceErr_BadState:
    return "kDNSServiceErr_BadState";
  case kDNSServiceErr_BadFlags:
    return "kDNSServiceErr_BadFlags";
  case kDNSServiceErr_Unsupported:
    return "kDNSServiceErr_Unsupported";
  case kDNSServiceErr_NotInitialized:
    return "kDNSServiceErr_NotInitialized";
  case kDNSServiceErr_AlreadyRegistered:
    return "kDNSServiceErr_AlreadyRegistered";
  case kDNSServiceErr_NameConflict:
    return "kDNSServiceErr_NameConflict";
  case kDNSServiceErr_Invalid:
    return "kDNSServiceErr_Invalid";
  case kDNSServiceErr_Firewall:
    return "kDNSServiceErr_Firewall";
  case kDNSServiceErr_Incompatible:
    return "kDNSServiceErr_Incompatible";
  case kDNSServiceErr_BadInterfaceIndex:
    return "kDNSServiceErr_BadInterfaceIndex";
  case kDNSServiceErr_Refused:
    return "kDNSServiceErr_Refused";
  case kDNSServiceErr_NoSuchRecord:
    return "kDNSServiceErr_NoSuchRecord";
  case kDNSServiceErr_NoAuth:
    return "kDNSServiceErr_NoAuth";
  case kDNSServiceErr_NoSuchKey:
    return "kDNSServiceErr_NoSuchKey";
  case kDNSServiceErr_NATTraversal:
    return "kDNSServiceErr_NATTraversal";
  case kDNSServiceErr_DoubleNAT:
    return "kDNSServiceErr_DoubleNAT";
  case kDNSServiceErr_BadTime:
    return "kDNSServiceErr_BadTime";
#ifdef MACDNSSD
  case kDNSServiceErr_BadSig:
    return "kDNSServiceErr_BadSig";
  case kDNSServiceErr_BadKey:
    return "kDNSServiceErr_BadKey";
  case kDNSServiceErr_Transient:
    return "kDNSServiceErr_Transient";
  case kDNSServiceErr_ServiceNotRunning:
    return "kDNSServiceErr_ServiceNotRunning";
  case kDNSServiceErr_NATPortMappingUnsupported:
    return "kDNSServiceErr_NATPortMappingUnsupported";
  case kDNSServiceErr_NATPortMappingDisabled:
    return "kDNSServiceErr_NATPortMappingDisabled";
  case kDNSServiceErr_NoRouter:
    return "kDNSServiceErr_NoRouter";
  case kDNSServiceErr_PollingMode:
    return "kDNSServiceErr_PollingMode";
  case kDNSServiceErr_Timeout:
    return "kDNSServiceErr_Timeout";
#endif
  }
  return "Invalid error";
}
