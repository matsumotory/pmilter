puts "hello pmilter handler called from #{Pmilter.name}"
puts "client ipaddr #{Pmilter::Session.new.client_ipaddr}"
puts "client daemon #{Pmilter::Session.new.client_daemon}"
puts "handler phase name: #{Pmilter::Session.new.handler_phase_name}"
