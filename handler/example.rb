puts "hello pmilter handler called from #{Pmilter.name}"
puts "client ipaddr #{Pmilter::Session.new.client_ipaddr}"
puts "client daemon #{Pmilter::Session.new.client_daemon}"
