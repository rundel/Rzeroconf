get_dnssd = function() {
  cmd = Sys.which("dns-sd")
  if (cmd == "") {
    stop(
      "Unable to locate `dns-sd` which is necessary on this OS. \n",
      "Depending on your OS try the following:\n",
      " * Windows: Install Bonjour for Windows (https://support.apple.com/bonjour)\n",
      " * MacOS  : It is installed by default on all systems\n",
      call. = FALSE
    )
  }
  
  cmd
}

get_avahi = function(method = c("browse", "resolve", "publish")) {
  method = match.arg(method)
  cmd = Sys.which(paste0("avahi-", method))
  
  if (cmd == "") {
    stop(
      "Unable to locate `dns-sd` which is necessary on this OS. \n",
      "Depending on your OS package system try installing the following:\n",
      " * deb: avahi-utils\n",
      " * rpm: avahi-tools\n",
      call. = FALSE
    )
  }
}


check_cmd = function(cmd) {
  cmd = strsplit(cmd, " ")[[1]][1]
  processx::run("which", cmd, error_on_status = FALSE)
}