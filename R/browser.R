
#' @export
browser = R6::R6Class(
  "zeroconf_browser",
  public = list(
    initialize = function(service, domain = "local", start=TRUE) {
      private$service = service
      private$domain = domain
      private$detect_os()
      if (start)
        self$start()
    },
    get_results = function() {
      private$results
    },
    start = function() {
      if (inherits(private$process, "process"))
        stop("Current browser process is still running, ","
             it must be stopped before starting a new browser.")
      
      private$process = processx::process$new(private$exec, private$args, stdout = "|", stderr = "|")
      
      # Wait for inialization
      Sys.sleep(0.5)
      self$update()
    },
    stop = function(kill = FALSE) {
      if (kill)
        private$process$kill()
      else 
        private$process$interrupt()
      
      if (private$process$is_alive())
        stop("Unable to stop browser process, try using `stop(kill=TRUE)`")
      
      private$process = NULL
    },
    update = function() {
      private$update_output()
      private$update_error()
      
      cur_output = output_skip(private$output, private$output_n_skip)
      cur_output = output_collapse(cur_output)
      
      private$results = private$output_parser(
        cur_output, private$output_col_names
      )[,c("name","service","domain")]
    },
    get_error = function() {
      private$error
    },
    get_output = function() {
      private$output
    },
    get_process_status = function() {
      private$process$is_alive()
    }
  ),
  private = list(
    results = NULL,
    service = NULL,
    domain  = NULL,
    
    exec = NULL,
    args = NULL,
    process = NULL,
    
    output_n_skip = NULL,
    output_parser = NULL,
    output_col_names = NULL,
    
    output = character(),
    error = character(),
    
    update_output = function() {
      private$output = append(private$output, private$process$read_output_lines())
    },
    update_error = function() {
      private$error = append(private$error, private$process$read_error_lines())
    },
    detect_os = function() {
      sys_type = Sys.info()[["sysname"]]
      
      if (sys_type %in% c("Darwin", "Windows")) {
        private$exec = get_dnssd()
        private$args = c("-B", private$service, private$domain)
        private$output_n_skip = 4
        private$output_parser = parse_dnssd
        private$output_col_names = c("timestamp", "add_remove", "flags", "interface", 
                                     "domain", "service", "name")
      } else if (sys_type == "Linux") {
        private$exec = get_avahi("browse")
        private$args = c("-p", "-k", "-d", private$domain, private$service)
        private$output_n_skip = 0
        private$output_parser = parse_avahi
        private$output_col_names = c("E", "interface", "protocol", "name", "service", "domain")
      } else {
        stop("Unsupported system type: ", sys_type)
      }
    }
  )
)

